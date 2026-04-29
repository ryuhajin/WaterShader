# shader-hot-reload

> Branch: `feature/shader-hot-reload` · Status: draft · Updated: 2026-04-29

## 1. Goal / Visual Target

- **한 문장 요약:** 셰이더 파일(`shaders/*.hlsl`)을 외부 에디터에서 저장하면 앱 재시작·빌드 없이 ~200ms 내에 새 셰이더가 화면에 적용되고, 컴파일 실패 시 직전 셰이더가 유지되며 ImGui에 에러 메시지가 표시되는 인프라 feature.
- **시각 목표 키워드:** 작업 사이클 단축. "에디터 저장 → 빌드 → 재시작 → 클릭"을 "에디터 저장 → 즉시 결과"로.
- **참고 이미지/영상:** 없음 (인프라 feature).
- **스코프 가드 — 절대로 만들지 않을 것:**
  - 셰이더 자체의 시각적 변화 — 본 feature는 호스트 측 흐름만 다룸.
  - include 그래프 추적 / 멀티 hlsl 의존성 자동 감지 — `simple.hlsl` 단일 파일만 감시.
  - InputLayout attribute의 추가/제거(레이아웃 자체 변경) — 같은 attribute 구성에서 셰이더 로직만 바뀌는 케이스만 지원.
  - 멀티 셰이더 클래스 일반화 — `ColorShader` 하나에만 적용. 향후 다른 셰이더가 들어오면 그때 공통화.

## 2. HLSL 접근법 / 수식 / 의사코드

이번 feature는 셰이더 코드 자체보다 **호스트(C++) 측 polling + recompile 흐름**이 핵심.

**ColorShader 내부 추가 상태:**

```cpp
std::wstring                            shaderPath_;
std::filesystem::file_time_type         lastWriteTime_{};
std::chrono::steady_clock::time_point   nextCheckTime_{};
std::string                             lastError_;          // 비어있으면 정상
std::string                             lastReloadStamp_;    // "HH:MM:SS"
constexpr auto kPollInterval = std::chrono::milliseconds(200);
```

**매 프레임** — `Graphics::Render`에서 `ColorShader::CheckHotReload(device, now)` 호출:

```text
if (now < nextCheckTime_): return
nextCheckTime_ = now + kPollInterval
mtime = std::filesystem::last_write_time(shaderPath_)
if (mtime == lastWriteTime_): return
lastWriteTime_ = mtime
Reload(device)
```

**`Reload(device)`** — 성공 시에만 셰이더 객체 교체 (atomic-ish):

```text
새 VS bytecode 컴파일 → 실패면 lastError_ 저장 후 return
새 PS bytecode 컴파일 → 실패면 lastError_ 저장 후 return
새 ID3D11VertexShader / PixelShader / InputLayout 생성 → 실패면 return
vertexShader_ / pixelShader_ / layout_ 교체 (이전 객체는 ComPtr 자동 해제)
lastError_.clear()
lastReloadStamp_ = 현재 시각 "HH:MM:SS"
```

**핵심 함수 재사용:**
- 기존 `CompileShader`(`src/ColorShader.cpp` 익명 namespace) 그대로. 단, 컴파일 에러 메시지(`ID3DBlob* errors`)를 `OutputDebugStringA`만 하지 말고 `lastError_`로도 흡수.
- `std::filesystem::last_write_time`, `std::chrono::steady_clock::now`.

**ImGui 표시:** `Graphics::DrawImGuiPanel`의 Shader Bench 안에 작은 섹션 추가:
- 정상: `ImGui::Text("Shader: reloaded %s", lastReloadStamp_.c_str())` (또는 "ready")
- 에러: `ImGui::TextColored(red, "Compile error:"); ImGui::TextWrapped("%s", lastError_.c_str())`

**참고 자료:**
- [`std::filesystem::last_write_time` (cppreference)](https://en.cppreference.com/w/cpp/filesystem/last_write_time)
- [Microsoft D3DCompileFromFile 문서](https://learn.microsoft.com/en-us/windows/win32/api/d3dcompiler/nf-d3dcompiler-d3dcompilefromfile)

## 3. Inputs / Outputs

| 종류    | 이름                   | 형식                                  | 비고                                              |
|---------|------------------------|---------------------------------------|---------------------------------------------------|
| 파일    | `shaders/simple.hlsl`  | mtime                                 | `std::filesystem::last_write_time`                |
| 상수    | poll interval          | `std::chrono::milliseconds`           | 기본 200ms                                        |
| ImGui   | 상태 표시              | Text 1~수줄                           | 정상: `Reloaded HH:MM:SS`, 에러: `Compile error: ...` |
| Output  | `vertexShader_` 등     | `ID3D11VertexShader/PixelShader/InputLayout` | 컴파일 성공 시에만 교체                    |

**의존하는 다른 feature:** `feature/dx11-setup` (cbuffer 파이프라인, ImGui 패널).

## 4. Acceptance Criteria / Test Plan

- [ ] 빌드 성공, C++ 경고 0, HLSL 컴파일 경고 0
- [ ] `simple.hlsl`을 외부 에디터에서 저장 → 1초(200ms 폴링 + 여유) 내 결과 변화가 화면에 반영
- [ ] 의도적으로 HLSL syntax error를 넣고 저장 → 화면 깨짐 없음(직전 셰이더 유지), Shader Bench 패널에 에러 메시지 표시
- [ ] 에러를 고치고 다시 저장 → 정상 적용되고 에러 메시지 사라지며 timestamp 갱신
- [ ] dx11-setup 동작 회귀 없음 (Tint R/G/B 슬라이더, Y Rotation 0~360, Reset Rotation)
- [ ] 종료 시 D3D 디버그 레이어 경고 0
- [ ] (선택) 동작 GIF 1개 `references/`에 저장

## 5. 구현 메모 (참고용)

- `Graphics::Render`가 매 프레임 `ColorShader::CheckHotReload(device, std::chrono::steady_clock::now())` 호출. polling이 매우 가벼우므로 별도 스레드 불필요.
- `Reload`가 만드는 새 객체들이 모두 성공해야 commit. 중간 실패 시 멤버는 그대로. 이렇게 하면 컴파일 에러 시 프레임이 깨지지 않음.
- `shaderPath_`는 기존 `GetShaderPath`로 한 번 계산해서 보관. 매 polling마다 경로 재계산 불필요.
- `lastError_` 표시는 `ImGui::TextWrapped`로 다중 줄 수용. D3DCompiler의 에러 텍스트는 보통 파일경로+줄번호 포함.
- 향후 다른 셰이더 클래스 추가 시 같은 흐름을 추출해서 `ShaderHotReloader` 유틸로 분리할 수 있음. 지금은 `ColorShader` 안에 응집해두고 유지.

이 메모는 SPEC이 아니라 구현 시 출발점. 실제 구현 중 발견되는 사항은 같은 폴더의 `NOTES.md`에 기록.
