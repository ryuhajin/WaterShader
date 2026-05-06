# water-base — 구현 노트

> Branch: `feature/water-base` · Status: implementation done · Updated: 2026-05-02

SPEC에 미리 적어둔 결정 외에 구현 중 마주친 디테일들.

## 1. WaterParams struct 도입 시점

Phase A/B에서는 `Render` 시그니처에 인자를 직접 추가했지만, Phase C에서 `normalScroll1/2 + normalScale + normalSRV + normalSampler`를 더하면 인자가 18개를 넘어 가독성이 무너짐. 이 시점에 `ColorShader::WaterParams`를 도입해 `shallowColor / deepColor / normalScroll1/2 / fresnelPower / reflectionStrength / normalScale`를 한 번에 묶음. Phase D에서 wave[2]도 같은 struct에 추가. Graphics는 `water_` 멤버 하나로 모든 water 파라미터를 관리.

`reflectionStrength`도 WaterParams에 포함. Day 3에서는 environment 그룹에 있었지만 사실상 water가 환경을 얼마나 반사할지의 비율 — 의미상 water 파라미터.

## 2. cbuffer 패킹: Wave struct는 32 bytes로 패딩

HLSL의 `Wave` struct는 `float2 dir + float amp + float wavelength + float speed + float3 _pad`로 정의 — 16 bytes(첫 행) + 16 bytes(둘째 행) = 32 bytes. C++ `WaveCB`도 동일 레이아웃(`XMFLOAT2 + 3*float + 3*float`). 두 wave면 64 bytes 추가. Map/Unmap에서 `pad`도 0 초기화 (디버그 출력 깨짐 방지).

처음에는 wave 배열을 HLSL에서 `Wave g_Waves[2]` 한 줄로 선언했으나 cbuffer 안의 struct 배열은 컴파일러 패킹과 C++ 레이아웃을 맞추기 까다로워 명시적으로 `g_Wave0_*`, `g_Wave1_*` 변수 이름을 풀어둠. 가독성은 떨어지지만 cbuffer 데이터가 정확히 매칭되는지 명확.

## 3. Sine wave normal — 미분으로 analytic 재계산

`y = A·sin(phase)`이고 `phase = dot(dir, p.xz)·k − ω·t`라면:

```
∂phase/∂x = dir.x · k
∂phase/∂z = dir.y · k
∂y/∂x = ∂phase/∂x · A·cos(phase) = dir.x · k · A · cos(phase)
∂y/∂z = ∂phase/∂z · A·cos(phase) = dir.y · k · A · cos(phase)
```

여러 wave의 합은 각 derivative도 합. 표면 normal은 grad(z = y(x,z) − const)을 정규화:

```
N = normalize(float3(-∂y/∂x, 1, -∂y/∂z))
```

`AccumulateSineWave` 함수에서 `amp <= 0` 또는 `wavelen <= 0`이면 일찍 return — 슬라이더로 amp를 0으로 끌면 해당 wave가 완전히 비활성. wavelen=0 보호는 0으로 나누기 방지.

## 4. TBN — plane 가정으로 단순화

```hlsl
float3 T = normalize(mul(float3(1, 0, 0), (float3x3)g_World));
float3 B = normalize(mul(float3(0, 0, 1), (float3x3)g_World));
float3 N0 = normalize(input.normalWS);
```

plane이 XZ에 누워 있고 g_World로 회전된다는 가정. tangent = world X 방향, bitangent = world Z 방향. 노멀 맵의 RG 채널이 X/Z 방향 perturbation에 매핑되고, B 채널은 N0 방향. 일반 메시에 적용하려면 vertex tangent를 추가하거나 partial derivative `ddx/ddy(worldPos)`로 cotangent frame 도출 — 본 프로젝트는 plane만 다루므로 단순 가정이 맞음.

## 5. Texture stub → 일반 2D 로더 + flat fallback

Day 3 시점에는 `Texture::Initialize`가 `return true`만 하는 빈 stub였음. Phase C에서 DirectXTK `CreateDDSTextureFromFile`/`CreateWICTextureFromFile`을 확장자로 분기해 채움. 추가로 `InitializeFlat(r,g,b,a)`를 만들어 1x1 immutable Texture2D로 폴백. `assets/textures/water_normal.dds` 또는 `.png`가 없으면 RGB(128,128,255) (= tangent normal (0,0,1))로 폴백 → 셰이더 sampling 결과가 항상 평면 normal이 되어 정점 normal이 그대로 사용됨. 사용자는 셰이더 코드 변경 없이 자산만 드롭하면 즉시 적용.

## 6. Fresnel + reflection의 의미 변화

Day 3 합성식:
```
lit = lambert + envColor * g_ReflectionStrength
```
환경 반사가 항상 일정 강도로 더해짐 — "물" 느낌이 안 남. 이번 합성식:
```
fresnel = pow(1 - NdotV, g_FresnelPower)
lit = lerp(litWater, envColor, saturate(fresnel * g_ReflectionStrength))
```
정면(NdotV=1)에서는 fresnel=0이라 환경 반사 0% → 물 본연 색만. 비스듬히(NdotV→0)에서는 fresnel→1이라 거의 거울처럼 환경. `g_ReflectionStrength`는 fresnel=1일 때의 최대 반사 비율로 의미가 바뀜.

## 7. 양면 RS 적용 범위

Skybox는 cube 안쪽에서 보는 구조라 기본 CullBack(+FrontCCW)이 그대로 안쪽 face를 보여줌. Water plane만 양면이 필요. 그래서 Render 흐름:

```
Skybox 그리기 (default RS)
  → SetRasterizerDoubleSided
  → Water 그리기
  → SetRasterizerDefault
ImGui 그리기 (default RS)
```

ImGui와 다음 프레임 시작이 항상 default RS라는 invariant를 유지.

## 8. ImGui Wave direction은 angle로 노출

Wave direction은 cbuffer에 `XMFLOAT2 (cos, sin)`으로 들어가지만 슬라이더로 두 값을 직접 조정하면 직관적이지 않음. ImGui에서는 `atan2(y, x)`로 각도를 추출해 `SliderFloat("Direction (deg)", ..., -180, 180)`로 노출하고, 슬라이더 변경 시 `(cos(rad), sin(rad))`로 재계산. 정규화도 자동으로 보존됨.

## 9. amp=0이면 출렁임 OFF

Wave amp 슬라이더를 0으로 끌면 해당 wave가 완전히 무효화. 두 wave 모두 0이면 정점 변위 없음 → input.position 그대로 사용. ImGui로 즉석에서 정점 변위 ON/OFF 토글 가능.

## 10. 셰이더 파일 구조 — VS/PS 분리 + hlsli 인클루드

water 셰이더 본체를 직접 작성·디버깅하기 편하도록 `simple.hlsl` 한 파일을 다음 5개로 재구성:

```
shaders/
├── Common.hlsli       — PerFrameCB cbuffer + VSInput/PSInput struct
├── Lighting.hlsli     — Lambert / FresnelSchlick 헬퍼
├── Cubemap.hlsli      — g_Skybox(t0) + g_Sampler(s0) + SampleEnv 헬퍼
├── vertexShader.hlsl  — #include Common.hlsli; sine wave 변위 + VSMain
└── PixelShader.hlsl   — #include Common+Lighting+Cubemap.hlsli; g_NormalMap(t1)+g_NormalSampler(s1) + PSMain
```

**hlsli ↔ hlsl 분담 원칙:**
- **hlsli**: 선언(declaration) + 단순 산술 헬퍼. include guard(`WS_*_HLSLI`) 필수. 사용자가 본체 작업 중 자주 건드리지 않을 부분.
- **hlsl**: 합성식·변위 본체·바인딩 사용처. 사용자가 디버깅·튜닝하면서 손볼 영역.

**cbuffer는 한 덩어리로 유지**: HLSL의 `cbuffer`는 단일 선언 단위라 여러 파일에 쪼갤 수 없음. PerFrameCB를 hlsli로 빼면 그 안의 모든 필드(VS-only인 Wave 포함)도 같이 따라옴. PS가 안 읽는 필드는 컴파일러가 무시하므로 비용 0. C++ `PerFrameCB` 매핑이 한 곳에 묶여 있어 유지보수도 단순.

**바인딩 슬롯 충돌 방지**: 한 셰이더에서 같은 register를 두 번 선언하면 컴파일 에러. `Cubemap.hlsli`가 t0/s0을 잡고 있으므로 이 파일을 인클루드하는 `PixelShader.hlsl`에서 t0/s0을 재선언하지 않음. water-only 리소스는 t1/s1(`g_NormalMap`/`g_NormalSampler`)만 PS에서 별도 선언. `skybox.hlsl`은 자체 t0/s0 선언이 있으므로 `Cubemap.hlsli`를 인클루드하지 않음.

**`.hlsli`는 컴파일 타깃이 아님**: CMake `add_executable` 소스에 들어가지 않고, POST_BUILD `copy_if_different`로 빌드 디렉터리에 데이터 파일로만 복사. 런타임에 `D3DCompileFromFile`이 `D3D_COMPILE_STANDARD_FILE_INCLUDE`로 `.hlsl`이 인클루드할 때 같은 디렉터리에서 해상.

**Hot-reload 다중 파일 watch**: `ColorShader::watchedFiles_`(vector)에 5개 경로 모두 등록. 200ms 폴링에서 어느 하나라도 mtime이 바뀌면 VS+PS 모두 재컴파일. `Common.hlsli` 한 줄만 고쳐도 즉시 반영됨.
