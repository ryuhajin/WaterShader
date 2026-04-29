# dx11-setup

> Branch: `feature/dx11-setup` · Status: done · Updated: 2026-04-29

## 1. Goal / Visual Target

- **한 문장 요약:** ImGui 패널의 **Tint Color**(RGB)와 **Y축 회전각(0~360도)** 슬라이더를 움직이면 화면의 삼각형이 즉시 색이 바뀌고 그 각도만큼 회전하는, **셰이더 파라미터 파이프라인이 살아있음을 증명하는 최소 실행 데모**. 자동 회전 없음 — 슬라이더가 직접 각도를 지정.
- **시각 목표 키워드:** 즉시 반응(immediate feedback), 디버깅 가능, 작업의 출발점.
- **참고 이미지/영상:** 없음 (이번엔 시각 완성도가 아니라 파이프라인 검증 목적).
- **스코프 가드 — 절대로 만들지 않을 것:**
  - 텍스처 로딩, 노멀 매핑, 라이팅 계산
  - foam, fresnel, ripple 등 water 관련 어떤 효과
  - HLSL hot reload (별도 feature)
  - 멀티 패스, 포스트 프로세싱
  - plane / quad / cube 같은 새 mesh — 기존 `Model`의 단일 삼각형으로 충분

## 2. HLSL 접근법 / 수식 / 의사코드

**핵심 함수:** `mul`, `saturate`. 그 외 없음.

**핵심 변경사항:**

- VS는 position을 `mul(position, mvp)`로 변환해서 `SV_POSITION`으로 출력.
- PS는 `g_TintColor.rgb`를 그대로 `SV_TARGET`에 출력 (Tint Color = 모델의 base color).
- vertex color 입력은 사용하지 않음 — InputLayout / VertexType에서 제거됨.

**의사코드 (`shaders/simple.hlsl` 갱신):**

```hlsl
cbuffer PerFrameCB : register(b0)
{
    row_major float4x4 g_MVP;
    float4 g_TintColor;     // rgb=base color, a=unused (16-byte align용)
};

struct VSInput { float3 position : POSITION; };
struct PSInput { float4 position : SV_POSITION; };

PSInput VSMain(VSInput input)
{
    PSInput o;
    o.position = mul(float4(input.position, 1.0), g_MVP);
    return o;
}

float4 PSMain(PSInput i) : SV_TARGET
{
    return float4(g_TintColor.rgb, 1.0);
}
```

**참고 자료:**

- [ImGui DX11 example](https://github.com/ocornut/imgui/tree/master/examples/example_win32_directx11)
- [Microsoft DX11 cbuffer 문서](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-constants)

## 3. Inputs / Outputs

| 종류    | 이름             | 형식                  | 범위 / 비고                                |
|---------|------------------|-----------------------|--------------------------------------------|
| CBuffer | g_MVP            | row_major float4x4    | b0, model*view*projection                  |
| CBuffer | g_TintColor      | float4                | b0, rgb 0..1, a unused                     |
| ImGui   | Tint Color       | ColorEdit3            | linear RGB, 기본 (1,1,1)                   |
| ImGui   | Y Rotation       | SliderFloat           | 0..360 deg (clamp), 기본 0                 |
| ImGui   | Reset Rotation   | Button                | 누르면 회전각 0으로                        |
| Output  | RT0              | float4                | back buffer                                |

**의존하는 다른 feature:** 없음 (첫 feature).

## 4. Acceptance Criteria / Test Plan

- [ ] HLSL 컴파일 성공, 경고 0
- [ ] 빌드 후 실행하면 윈도우와 ImGui 패널이 함께 뜸
- [ ] ColorEdit3로 색을 바꾸면 삼각형 색이 즉시 반영됨
- [ ] Y Rotation 슬라이더로 0~360도 회전각을 직접 지정할 수 있고 자동 회전은 없음
- [ ] Reset Rotation 버튼이 회전각을 0도로 되돌림
- [ ] ImGui 위에서 마우스를 클릭/드래그할 때 Win32 입력이 ImGui로 정상 전달됨 (slider 동작)
- [ ] 윈도우 리사이즈 시 백버퍼와 viewport가 함께 갱신되고 깨지지 않음
- [ ] 종료 시 ImGui/D3D shutdown 누수 없음 (디버그 레이어 경고 0)
- [ ] (선택) 결과 GIF 1개 `references/` 에 저장

## 5. 구현 메모 (참고용)

채택할 외부 라이브러리 사용 지점:
- **ImGui 백엔드:** `imgui_impl_win32` + `imgui_impl_dx11` (vcpkg `imgui[win32-binding,dx11-binding]` 가 켜져 있는지 `vcpkg.json` 확인 필요).
- WndProc 후킹: `System` 클래스의 메시지 루프에서 `ImGui_ImplWin32_WndProcHandler` 를 가장 먼저 호출.

코드 변경 예상 지점:
- `vcpkg.json` — imgui feature 옵션 확인.
- `CMakeLists.txt` — imgui binding 헤더 경로 확인. 필요 시 `imgui[win32-binding,dx11-binding]` 활성화.
- `src/System.{h,cpp}` — ImGui Win32 init/shutdown, WndProc 후킹, 프레임 begin/end.
- `src/Graphics.{h,cpp}` — ImGui DX11 init/shutdown, 프레임 begin/render-data, ImGui 패널 그리기 위치, 회전 누적 시간 보유.
- `src/ColorShader.{h,cpp}` — `ID3D11Buffer` constant buffer 추가, `UpdateSubresource` 또는 `Map/Unmap`로 매 프레임 업데이트, `VSSetConstantBuffers` 바인딩.
- `shaders/simple.hlsl` — 위 의사코드대로 cbuffer 추가, VS에서 `mul` 적용, PS에서 tint 곱셈.

이 메모는 SPEC이 아니라 구현 시 출발점. 실제 구현 중 발견되는 사항은 같은 폴더의 `NOTES.md`에 기록.
