# env-cubemap

> Branch: `feature/env-cubemap` · Status: drafting · Updated: 2026-05-01

## 1. Goal / Visual Target

- **한 문장 요약:** DDS 큐브맵을 로드해 inline cube skybox로 배경에 그리고, plane이 카메라 시야각에 따라 환경을 reflect.
- **시각 목표 키워드:** 환경 반사, skybox 배경, view-dependent reflection. Day 4 water-base에서 fresnel + reflection이 의미를 가지도록 하는 인프라.
- **참고 이미지/영상:** 없음 (인프라 단계).
- **스코프 가드 — 절대로 만들지 않을 것:**
  - water 셰이더 (UV scroll / fresnel / normal map) 안 함 — `feature/water-base` 단계.
  - foam / ripple 없음 — Day 5.
  - IBL diffuse irradiance 없음 (specular reflect only).
  - HDR 큐브맵 / 블러된 mip 큐브맵 없음 — LOD 0만 사용.
  - 그림자 없음 (전체 일정 정책).
  - 톤매핑/감마 보정 없음 — back buffer 그대로 (LDR).

## 2. HLSL 접근법 / 수식 / 의사코드

**핵심 함수:** `mul`, `normalize`, `reflect`, `Sample` (TextureCube).

**핵심 트릭 — skybox depth:**

```
o.pos = clip.xyww   →  z/w = w/w = 1.0  (가장 먼 평면)
DSS = LessEqual     →  z=1.0 픽셀이 reject되지 않게 함
```

**핵심 트릭 — view에서 translation 제거:**

```
viewNoTrans = view; viewNoTrans.r[3] = (0, 0, 0, 1)
→ skybox는 카메라 회전만 반영, 카메라 이동은 무시 → 무한히 멀게 느껴짐
```

**`shaders/skybox.hlsl` 신규:**

```hlsl
cbuffer SkyboxCB : register(b0)
{
    row_major float4x4 g_ViewNoTranslation;
    row_major float4x4 g_Projection;
};
TextureCube  g_Skybox  : register(t0);
SamplerState g_Sampler : register(s0);

struct VSIn { float3 pos : POSITION; };
struct PSIn { float4 pos : SV_POSITION; float3 dir : TEXCOORD0; };

PSIn VSMain(VSIn input)
{
    PSIn o;
    float4 viewPos = mul(float4(input.pos, 1.0f), g_ViewNoTranslation);
    float4 clip    = mul(viewPos, g_Projection);
    o.pos = clip.xyww;        // depth = 1
    o.dir = input.pos;        // local cube vertex pos = sample direction
    return o;
}

float4 PSMain(PSIn input) : SV_TARGET
{
    return g_Skybox.Sample(g_Sampler, normalize(input.dir));
}
```

**`shaders/simple.hlsl` 확장 (reflect 추가):**

```hlsl
cbuffer PerFrameCB : register(b0)
{
    row_major float4x4 g_World;
    row_major float4x4 g_View;
    row_major float4x4 g_Projection;
    float4 g_LightDirection;
    float4 g_LightColor;
    float4 g_TintColor;
    float4 g_CameraPositionWS;   // xyz=cam pos, w unused
    float  g_Time;
    float  g_ReflectionStrength; // 0~1
    float2 _padding;
};
TextureCube  g_Skybox  : register(t0);
SamplerState g_Sampler : register(s0);

struct VSInput { float3 position:POSITION; float3 normal:NORMAL; float2 uv:TEXCOORD0; };
struct PSInput { float4 position:SV_POSITION; float3 normalWS:NORMAL; float3 worldPos:TEXCOORD0; float2 uv:TEXCOORD1; };

PSInput VSMain(VSInput input)
{
    PSInput o;
    float4 worldPos = mul(float4(input.position, 1.0f), g_World);
    o.position = mul(mul(worldPos, g_View), g_Projection);
    o.normalWS = normalize(mul(input.normal, (float3x3)g_World));
    o.worldPos = worldPos.xyz;
    o.uv = input.uv;
    return o;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 N = normalize(input.normalWS);
    float3 L = normalize(-g_LightDirection.xyz);
    float  NdotL = saturate(dot(N, L));
    float3 lambert = g_TintColor.rgb * g_LightColor.rgb * g_LightColor.a * NdotL;

    float3 V = normalize(g_CameraPositionWS.xyz - input.worldPos);
    float3 R = reflect(-V, N);
    float3 envColor = g_Skybox.Sample(g_Sampler, R).rgb;

    float3 lit = lambert + envColor * g_ReflectionStrength;
    return float4(lit, 1.0f);
}
```

**참고 자료:**

- [Microsoft TextureCube (HLSL)](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/sm5-object-texturecube)
- [Real-Time Rendering 4th — Ch. 9 Reflection (cubemap reflection)]
- [DirectXTK DDSTextureLoader](https://github.com/microsoft/DirectXTK/wiki/DDSTextureLoader)

## 3. Inputs / Outputs

| 종류    | 이름                       | 형식                       | 범위 / 기본값                                  |
|---------|---------------------------|---------------------------|------------------------------------------------|
| Asset   | `assets/textures/skybox.dds` | DDS cubemap (6면)        | LOD 0 사용. 사용자가 원하면 동일 경로로 교체   |
| CBuffer | `g_ViewNoTranslation` (skybox) | `float4x4`            | view에서 translation 행 0                      |
| CBuffer | `g_Projection` (skybox)    | `float4x4`               | 기존 projection                                |
| CBuffer | `g_CameraPositionWS` (color) | `float4`               | xyz=카메라 world 좌표                          |
| CBuffer | `g_ReflectionStrength` (color) | `float`               | 0~1                                            |
| Resource| `g_Skybox` (t0, both)      | `TextureCube`            | DDS SRV                                        |
| Resource| `g_Sampler` (s0, both)     | `SamplerState`           | linear, address mode = clamp (XYZ 모두)        |
| ImGui   | Skybox Visible             | `Checkbox`               | 기본 ON                                        |
| ImGui   | Reflection Strength        | `SliderFloat`            | 0~1, 기본 0.4                                  |
| Output  | RT0 / DSV                  | back buffer + D24S8      | 기존                                           |

ImGui 패널은 `SeparatorText("Environment")` 카테고리 추가, 그 아래 Visible Checkbox + Reflection Strength.

**의존하는 다른 feature:** `scene-lighting`(g_World/View/Proj cbuffer, Lambert), `asset-pipeline`(plane mesh), `dx11-setup`(D3DClass + ImGui), `shader-hot-reload`(skybox.hlsl + simple.hlsl 모두 적용).

## 4. Acceptance Criteria / Test Plan

- [ ] 빌드 성공, C++/HLSL 경고 0
- [ ] `assets/textures/skybox.dds` 로드 → 화면 배경에 큐브맵이 보임
- [ ] 카메라 회전(WASD + 화살표) → skybox가 자연스럽게 배경처럼 따라옴 (translation 제거 효과)
- [ ] 카메라 이동 → skybox는 거리감 없이 항상 같은 거리 (무한히 멀게 느껴짐)
- [ ] plane이 환경을 반사 (시야각 따라 reflect 방향 변화)
- [ ] Reflection Strength 0 → 환경 반사 없음 (Lambert만), 1 → 환경 반사가 라이팅에 더해짐
- [ ] Skybox Visible 체크 끄면 배경이 BeginScene clear color로 바뀜, plane 반사는 그대로 (큐브맵 SRV는 살아있음)
- [ ] 회귀: Light/Tint/Rotation/Camera/FOV/Reset/shader hot-reload 모두 정상
- [ ] 종료 시 D3D 디버그 레이어 라이브 오브젝트 경고 0

## 5. 구현 메모 (참고용)

- **`o.pos = clip.xyww` 트릭**: skybox depth가 항상 1.0 (가장 먼 평면)이 되어 다른 모든 지오메트리 위에 그려짐. Less DSS는 z<1 픽셀만 통과시키므로 z=1.0이 reject됨 → **DSS = LessEqual 필수**.
- **view에서 translation 제거**: skybox는 카메라 회전만 반영. 이동 무시 → 무한히 멀게 느껴짐. `viewNoTrans.r[3] = (0,0,0,1)` 또는 `XMMatrixSet`으로 row 3 직접 교체.
- **Sampler clamp**: 큐브맵은 6면 사이 경계 보간 필요. `D3D11_TEXTURE_ADDRESS_CLAMP`로 black seam 방지. wrap이면 면 사이가 깨짐.
- **TextureCube SRV ViewDimension**: `D3D11_SRV_DIMENSION_TEXTURECUBE`. `CreateDDSTextureFromFile`이 DDS의 `DDSCAPS2_CUBEMAP` 플래그 보고 자동 설정.
- **Cubemap 리소스 공유**: skybox와 simple 셰이더 둘 다 t0에 같은 큐브맵 SRV 바인딩. 메모리 1장으로 두 곳 사용.
- **Reflection Strength = 0 일 때**: SRV는 여전히 바인딩, PS에서 샘플은 일어나지만 결과가 0과 곱해져 무시됨. 분기 안 하는 게 셰이더가 단순.
- **Skybox 정점 winding**: 24 verts(면당 4) + 36 indices. 우리 RS는 `FrontCounterClockwise=TRUE` + `CullBack`. 카메라가 cube **안쪽**에 있으므로 바깥에서 봤을 때의 CCW 윈딩이 안쪽 face가 되도록 정점 순서 정해야 함.
- **PerFrameCB 사이즈 변화**: 기존 144 bytes(World+View+Proj+LightDir+LightCol+Tint+Time+pad) → 추가 16 bytes(`g_CameraPositionWS`) + reflectionStrength 합쳐 16 bytes 행 → 총 176 bytes. ColorShader::InitializeShader의 cbuffer 크기 갱신.
- **Hot-reload**: skybox.hlsl도 ColorShader와 동일 패턴. SkyboxShader가 자체 last_write_time 폴링.
- **assets/textures/ 폴더 첫 사용**: CMake `copy_directory` 규칙은 이미 assets 전체를 복사하므로 추가 변경 불필요. 단 `simple.hlsl`만 복사하던 명시 규칙에 `skybox.hlsl` 추가 필요.

이 메모는 SPEC이 아니라 구현 시 출발점. 실제 구현 중 발견 사항은 같은 폴더의 `NOTES.md`에 기록.
