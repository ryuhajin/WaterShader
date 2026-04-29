# dx11-setup — 작업 메모

## 2026-04-29 · ImGui NewFrame 누락으로 첫 실행 abort

**증상:** 첫 실행 시 종료 코드 3, D3D11 디버그 레이어가 라이브 오브젝트 25개를 보고하면서 종료.

**원인:** ImGui 프레임 흐름에서 백엔드 NewFrame은 호출했지만 **코어 `ImGui::NewFrame()`을 호출하지 않음**. 그 상태에서 `ImGui::Begin()` 진입 시 `IM_ASSERT(g.WithinFrameScope)` 실패 → debug build에서 `abort()` → 종료 코드 3 → D3D 객체가 정리되지 않은 채 프로세스 종료 → 라이브 오브젝트 경고 폭주.

**라이브 오브젝트 경고는 부산물**: abort로 cleanup이 안 된 결과지 그 자체가 누수가 아님. NewFrame을 고치면 함께 사라짐.

**수정:** `Graphics::Render()` 시작점에 표준 ImGui 프레임 시작 순서 묶음.

```cpp
ImGui_ImplDX11_NewFrame();
ImGui_ImplWin32_NewFrame();
ImGui::NewFrame();   // ← 누락됐던 핵심
```

이전에는 `ImGui_ImplWin32_NewFrame()`이 `System::Frame()`에, `ImGui_ImplDX11_NewFrame()`이 `Graphics::Render()`에 분리되어 있었음. ImGui 프레임 흐름은 한 곳에서 관리하는 게 표준이라 모두 Graphics로 통합.

**교훈:** ImGui 통합 시 백엔드 두 개의 NewFrame만 챙기지 말고 코어 `ImGui::NewFrame()`까지 한 묶음으로 호출해야 함. 향후 다른 ImGui-사용 feature에서도 동일 실수 주의.

## 2026-04-29 · Tint Color 의미 재정의 + 회전 자동 → 수동 전환

**증상:** 첫 검증 시도에서 두 가지 문제:
1. Tint Color를 흰색(1,1,1)으로 둬도 삼각형이 파란색으로 보임.
2. 삼각형이 자동으로 계속 회전해서 검증용 데모로는 산만함.

**원인 1 — 색:** `Model.cpp`의 세 정점이 모두 파란색 계열로 지정되어 있었고, 픽셀 셰이더가 `input.color * g_TintColor.rgb`로 곱셈 합성하므로 흰색 tint는 vertex color를 그대로 통과시킴. vertex color는 단지 정점마다 다른 색을 보여주려고 넣은 placeholder였을 뿐 의미가 없었음.

**원인 2 — 회전:** `Graphics::Render`가 매 프레임 `yRotationSpeedDegPerSec_ * deltaTime`을 누적했기 때문. 슬라이더가 "속도"를 조절하는 구조라 0이 아닌 한 멈추지 않음.

**수정:**
- **Tint Color = 모델 base color**로 재정의. 픽셀 셰이더 출력을 `g_TintColor.rgb`로 단순화하고, 더 이상 쓰이지 않게 된 vertex color 입력을 dead code로 두지 않고 `simple.hlsl`의 VSInput/PSInput, `Model::VertexType`, `ColorShader::polygonLayout`에서 모두 제거. 후속 water 셰이더에서도 "base color" 의미는 자연스럽게 재사용됨.
- **회전 = 각도 직접 지정**. `yRotationSpeedDegPerSec_` + `yRotationRadians_` 두 변수를 `yRotationDegrees_` 한 변수로 통합. 슬라이더는 0~360도 clamp, 누적 로직 제거. Reset 버튼은 단순히 0도로 되돌림.

**교훈:** "검증용 데모"는 사용자가 파라미터를 만질 때만 변하는 게 가장 명확함. 시간 기반 자동 변화는 "효과 시연"용일 때만 의미가 있음. 또한 입력 셰이더에 placeholder 데이터(의미 없는 vertex color 등)를 두면 후속 디버깅 때 무엇이 base인지 흐려지므로, 의미 없는 입력은 셰이더 정의 단계에서 잘라내는 게 낫다.
