# asset-pipeline

> Branch: `feature/asset-pipeline` · Status: draft · Updated: 2026-04-29

## 1. Goal / Visual Target

- **한 문장 요약:** OBJ 메시(`assets/models/plane.obj`)를 tinyobjloader로 임포트해 화면에 그리고, depth buffer(DSV)를 파이프라인에 추가하여 후속 water/폭포/바위 작업의 인프라 토대를 만든다.
- **시각 목표 키워드:** 메시 임포트가 동작한다, 깊이가 살아있다 — 본 feature 자체는 큰 시각 변화 없음(셰이더 효과는 후속에서).
- **참고 이미지/영상:** 없음 (인프라 feature).
- **스코프 가드 — 절대로 만들지 않을 것:**
  - 노멀맵·머티리얼 텍스처 로딩 (water-base / texture-assets feature에서)
  - `.mtl` 파일 파싱 — vertex position/normal/UV만 활용
  - 셰이더의 normal/UV를 실제로 사용한 라이팅·UV scroll (water-base에서)
  - 카메라 orbit/pan 컨트롤 (별도 feature)
  - 디버그 시각화 토글 (별도 `feature/debug-viz`에서)
  - 멀티 메시 동시 그리기 — `Model` 인스턴스 하나에 plane만
  - 모델 hot-reload (셰이더 hot-reload는 동작하지만 메시 reload는 본 범위 밖)

## 2. HLSL 접근법 / 수식 / 의사코드

`shaders/simple.hlsl`의 입력 레이아웃을 확장. PS는 기존 base color 출력 유지 — normal/UV는 들어오기만 하고 본 feature에서는 사용 안 함(water-base의 사전 작업).

```hlsl
struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normalWS : NORMAL;       // world-space normal (후속 라이팅 대비)
    float2 uv       : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
    PSInput o;
    o.position = mul(float4(input.position, 1.0f), g_MVP);
    o.normalWS = mul(input.normal, (float3x3)g_World);  // world rotation은 g_World가 필요
    o.uv = input.uv;
    return o;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return float4(g_TintColor.rgb, 1.0f);
}
```

→ cbuffer 확장: 기존 `g_MVP`만으로는 normal 변환 불가. **`g_World` (또는 normal matrix)** 추가 필요. 또는 본 feature에서는 normal 변환 안 하고 그냥 `o.normalWS = input.normal`로 두고, world 매트릭스 분리는 water-base에서. 단순화 위해 **후자** 채택 — 본 feature에서는 normal/UV가 셰이더에 들어오는지만 검증, world 변환은 water-base에서.

**핵심 흐름:**
1. tinyobjloader가 `plane.obj` 파싱 → vertex 배열 (position, normal, uv) + index 배열 생성.
2. `Model::InitializeBuffers`가 그 배열로 `ID3D11Buffer` (VB/IB) 생성.
3. `ColorShader::polygonLayout`에 `NORMAL`, `TEXCOORD` element 추가.
4. `D3DClass`가 depth-stencil texture + DSV 생성, `OMSetRenderTargets`에 바인딩, `BeginScene`에서 `ClearDepthStencilView` 호출.
5. `D3DClass::Resize`도 depth texture 재생성.

**참고 자료:**
- [tinyobjloader API](https://github.com/tinyobjloader/tinyobjloader#examples)
- [Microsoft DSV / Depth Buffer 가이드](https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-resources-types)

## 3. Inputs / Outputs

| 종류    | 이름                       | 형식                            | 비고                                                        |
|---------|----------------------------|---------------------------------|-------------------------------------------------------------|
| 파일    | `assets/models/plane.obj`  | Wavefront OBJ                   | 32×32 grid plane (~1089 vertex), Blender export             |
| 라이브러리 | tinyobjloader            | vcpkg manifest                  | OBJ 파싱                                                    |
| C++ 타입 | `Model::VertexType`       | `{ float[3] pos; float[3] n; float[2] uv; }` | 32-byte stride                              |
| D3D 리소스 | depth-stencil texture   | `DXGI_FORMAT_D24_UNORM_S8_UINT` | 백버퍼 크기와 동일, Resize 시 재생성                        |
| D3D 리소스 | DSV                     | `ID3D11DepthStencilView`        | OMSetRenderTargets 바인딩                                   |
| InputLayout | POSITION/NORMAL/TEXCOORD | `R32G32B32_FLOAT` × 2 + `R32G32_FLOAT` | offset 0/12/24                                  |

**의존하는 다른 feature:** `dx11-setup`(cbuffer + ImGui), `shader-hot-reload`.

## 4. Acceptance Criteria / Test Plan

- [ ] 빌드 성공, C++/HLSL 경고 0
- [ ] vcpkg manifest에 tinyobjloader 등록 + 빌드 시 자동 설치
- [ ] `assets/models/plane.obj`를 빌드 시 exe 옆 `assets/models/`로 복사 (CMake)
- [ ] 앱 실행 → 화면에 plane이 그려짐 (현재 삼각형 대신). Y Rotation 슬라이더로 회전 정상.
- [ ] depth-stencil이 바인딩되어 있음을 D3D 디버그 레이어가 컴플레인 안 함 (depth 미바인딩 경고 없음)
- [ ] Resize 시 백버퍼·depth 동시 재생성 (창 크기 변경 시 깨짐 없음)
- [ ] hot-reload 회귀 없음 (`simple.hlsl` 저장 → 즉시 반영)
- [ ] dx11-setup 회귀 없음 (Tint R/G/B, Y Rotation, Reset, ImGui 패널)
- [ ] 종료 시 D3D 디버그 레이어 라이브 오브젝트 경고 0
- [ ] (선택) plane이 들어간 스크린샷 1개 `references/`에 저장

## 5. 구현 메모 (참고용)

**vcpkg.json 변경:**
```json
"dependencies": [
  "directxtk",
  "tinyobjloader",
  { "name": "imgui", "features": ["win32-binding", "dx11-binding"] }
]
```

**CMakeLists.txt 변경:**
- `find_package(tinyobjloader CONFIG REQUIRED)` 추가, `target_link_libraries`에 `tinyobjloader::tinyobjloader`.
- `assets/models/*.obj`를 빌드 시 exe 옆 `assets/models/`로 복사하는 `add_custom_command`. (셰이더와 같은 패턴)
- Debug 빌드에서 `WATERSHADER_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"` 매크로 추가 — 향후 모델 hot-reload나 직접 watch에 대비. 본 feature에서는 정의만 해두고 사용은 안 해도 OK.

**Model 클래스 확장:**
- `VertexType`에 normal/UV 추가.
- `InitializeBuffers(ID3D11Device*)` → `Initialize(ID3D11Device*, const std::wstring& objPath)`로 시그니처 확장. Graphics에서 plane 경로 지정.
- tinyobjloader 호출은 `Model.cpp`의 익명 namespace 헬퍼로 분리.

**ColorShader 변경:**
- `polygonLayout`에 NORMAL/TEXCOORD element 추가, offset 12/24, format R32G32B32_FLOAT/R32G32_FLOAT.

**D3DClass 변경:**
- 멤버 추가: `Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture_`, `Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView_`.
- `CreateDepthStencil(width, height)` 헬퍼 — texture + DSV 생성.
- `OMSetRenderTargets`에 RTV/DSV 함께.
- `BeginScene`에서 `ClearDepthStencilView(D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL)`.
- `Resize`에서 depth texture 재생성.
- `D3D11_DEPTH_STENCIL_DESC`로 default depth state는 일단 OK (DepthEnable=TRUE 기본).

**Graphics 변경:**
- `Model::Initialize` 호출 시 plane.obj 경로 전달. 경로는 셰이더와 동일하게 module-relative + Debug에서 source override.

이 메모는 SPEC이 아니라 구현 시 출발점. 실제 구현 중 발견되는 사항은 같은 폴더의 `NOTES.md`에 기록.
