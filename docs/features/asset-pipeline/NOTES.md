# asset-pipeline — 작업 메모

## 2026-04-29 · 빌드 사본 vs 소스 자산 디렉토리 (셰이더와 같은 패턴 재적용)

`shader-hot-reload`에서 발견했던 "빌드 사본 vs 소스" 이슈를 자산에도 미리 처리. CMakeLists.txt가 빌드 후 `assets/` 통째를 exe 옆으로 복사(`copy_directory`)하고, Debug 빌드에서만 소스 디렉토리 절대 경로를 매크로(`WATERSHADER_ASSETS_DIR`)로 박음. `Graphics::GetAssetPath`가 `#ifdef`로 분기해 Debug=소스 dir, Release=module-relative 사용. 향후 모델 hot-reload 도입 시 그대로 활용 가능.

## 2026-04-29 · `Model::VertexType`이 private이라 익명 namespace 헬퍼 접근 불가

**증상:** `Model.cpp` 익명 namespace에 `LoadObj` 헬퍼를 두고 호출하니 `Model::VertexType`이 private이라 컴파일 에러(`C2248`).

**수정:** `LoadObj`를 익명 namespace에서 `Model::LoadObj` private static 멤버 함수로 옮김. 같은 클래스 내부라 VertexType 자유 접근. tinyobjloader 호출 자체는 동일.

**교훈:** 클래스 내부 자료 구조(VertexType 등)를 다루는 헬퍼는 익명 namespace보다 클래스의 private static 멤버가 자연스럽다. C++ 접근 제어와 일관됨.

## 2026-04-29 · plane이 안 보이는 이유 — backface culling

**증상:** `assets/models/32x32Plane.obj`(Blender export)를 임포트한 뒤 `Model Rotation X=90`(plane이 카메라 정면)에서 plane이 안 보임. `X=181`(거의 뒤집힌 자세)에서만 보임.

**원인:**
- Blender(OpenGL convention)는 face winding을 **CCW(반시계)**로 정의 — face normal과 winding이 일치(외부에서 봤을 때 CCW).
- D3D11 RasterizerState 기본값은 `FrontCounterClockwise = FALSE` → **CW가 front face**.
- 결과: OBJ에서 가져온 CCW winding이 D3D에서 backface로 간주되어 cull됨.
- X=90도에서 plane이 카메라 향함 + winding CCW → D3D backface → 안 보임. X=181도에서 plane이 뒤집혀 backface가 카메라 향함 + 그 winding이 CW → D3D frontface → 보임.

**수정:** D3DClass에 명시적 RasterizerState 생성 — `FrontCounterClockwise = TRUE` + `CullMode = D3D11_CULL_BACK`. 즉 OpenGL/Blender convention을 D3D 측에서 채택.

```cpp
D3D11_RASTERIZER_DESC desc = {};
desc.FillMode = D3D11_FILL_SOLID;
desc.CullMode = D3D11_CULL_BACK;
desc.FrontCounterClockwise = TRUE;
desc.DepthClipEnable = TRUE;
```

**양면 그리기 처리 방침 (UE/Unity 표준):** 기본은 backface culling 켬. 단면 메시(나뭇잎/물/foam/유리)는 셰이더/material 단위로 별도 RasterizerState(`CullMode = D3D11_CULL_NONE`)를 만들어 토글. 본 feature에선 기본 RS만, 양면 RS는 water-base/foam-mask에서 필요할 때 추가.

**교훈:** OBJ 임포트 시 winding/normal/cull 세 요소를 함께 점검. "plane이 안 보임"은 모델 로딩 실패가 아니라 cull 문제일 가능성이 더 높음 — 다음 메시(rock 등) 임포트 시 같은 진단 절차(Model Rotation으로 자세 돌려보기) 적용.

## 2026-04-29 · 워크벤치 스코프 확장 (Model Rotation, Camera input, FOV)

**배경:** 본 feature는 원래 plane 임포트 + DSV에 한정한 인프라 작업이었음. 그러나 "plane이 안 보임" 이슈를 진단하면서 Model Rotation X/Y/Z 슬라이더가 필요해졌고, 동시에 카메라가 plane 주변을 자유롭게 돌아볼 수 있어야 후속 모든 셰이더 작업이 빨라짐.

**확장된 범위:**
- `Model Rotation` 카테고리 — X/Y/Z 슬라이더(0~360) + Reset
- `Camera` 카테고리 — FOV(30~120), Move Speed, Turn Speed, Reset
- 카메라 키보드 input — WASD/QE/화살표키. `Graphics::Frame(deltaTime, const Input&)`로 시그니처 확장하여 `System`이 input을 전달.
- ImGui와의 충돌은 이미 `System::MessageHandler`의 `WantCaptureKeyboard` 처리로 해결됨 — 텍스트 박스 포커스 시에만 ImGui가 키 가져감, 그 외엔 카메라.
- ImGui 패널 카테고리 헤더는 `ImGui::SeparatorText("...")`로 통일.

**교훈:** 인프라 feature(plane 임포트)와 워크벤치 도구(Rotation/Camera 슬라이더)는 한 묶음으로 가는 것이 자연스러움 — 임포트가 됐는지 확인하려면 메시를 돌려봐야 하고, 돌리려면 컨트롤이 있어야 함. SPEC 작성 시 진단 도구를 함께 스코프에 넣는 것이 후속 디버깅 비용을 줄임.
