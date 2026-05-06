# water-base

> Branch: `feature/water-base` · Status: done · Updated: 2026-05-02

## 1. Goal / Visual Target

- **한 문장 요약:** plane을 stylized 수면처럼 — 시야각에 따라 환경 반사 강도 변화(Fresnel), shallow/deep 두 색의 깊이감, 두 장 노멀 맵의 UV scroll로 표면 흐름, sine 합 정점 변위로 출렁임.
- **시각 목표 키워드:** Fresnel reflection, depth color, scrolling normals, surface waves.
- **참고 이미지/영상:** 없음 (Day 6 폭포 단계에서 references/에 정리 예정).
- **스코프 가드 — 절대로 만들지 않을 것:**
  - Foam (depth-difference / noise threshold) 없음 — `feature/foam-mask` (Day 5)
  - Ripple SDF 없음 — `feature/ripple-sdf` (Day 5)
  - Refraction (screen-space UV 왜곡) 없음 — 본 일정 스코프 밖
  - Caustics, Screen-Space Reflection 없음 — 스코프 밖
  - 동적 그림자 없음 — 전체 일정 정책
  - 톤매핑 / HDR 큐브맵 없음 — Day 3과 동일 LDR

## 2. HLSL 접근법 / 수식 / 의사코드

**핵심 함수:** `mul`, `normalize`, `dot`, `lerp`, `pow`, `reflect`, `Sample` (TextureCube + Texture2D), `sin`/`cos`.

**핵심 트릭 — Fresnel-driven blending:**

```
NdotV   = saturate(dot(N, V))
fresnel = pow(1 - NdotV, g_FresnelPower)         // Schlick-like (정면 약, grazing 강)
final   = lerp(litWater, envColor, fresnel * g_ReflectionStrength)
```

`g_ReflectionStrength`는 fresnel=1일 때의 최대 반사 비율. fresnel=0(정면)에서는 water 색만 보이고, fresnel=1(grazing)에서는 거울처럼 환경 반사.

**핵심 트릭 — Shallow/Deep by NdotV:**

```
waterCol = lerp(g_DeepColor, g_ShallowColor, NdotV)
// 위에서 보면(NdotV=1) shallow, 옆에서 보면(NdotV=0) deep — 깊이감 가짜로 만듦
```

**핵심 트릭 — Normal map 두 장 UV scroll + tangent-to-world (plane 가정 TBN):**

```
uv1 = uv * scale + scroll1 * time
uv2 = uv * scale + scroll2 * time
nTan = normalize(sample(uv1) + sample(uv2))   // [-1,1] 범위
T = mul(float3(1,0,0), (float3x3)World)        // plane lies in XZ
B = mul(float3(0,0,1), (float3x3)World)
N = normalize(nTan.x*T + nTan.y*B + nTan.z*N0)
```

**핵심 트릭 — Sine wave 합 + analytic normal 재계산:**

각 wave는 `dir(xz), amplitude, wavelength, speed`로 정의:

```
k     = 2π / wavelength
ω     = k · speed
phase = dot(dir, p.xz) · k − ω · time
y    += amp · sin(phase)
∂y/∂x = dir.x · k · amp · cos(phase)   // 누적
∂y/∂z = dir.y · k · amp · cos(phase)   // 누적
N     = normalize( float3(-∂y/∂x, 1, -∂y/∂z) )
```

이 정점 변위 normal과 위의 노멀 맵을 합성. (정점 normal은 큰 형상, 노멀 맵은 잔물결.)

**`shaders/simple.hlsl` 확장 (PS):**

```hlsl
cbuffer PerFrameCB : register(b0)
{
    row_major float4x4 g_World;
    row_major float4x4 g_View;
    row_major float4x4 g_Projection;
    float4 g_LightDirection;
    float4 g_LightColor;
    float4 g_TintColor;
    float4 g_CameraPositionWS;
    float4 g_ShallowColor;
    float4 g_DeepColor;
    float  g_Time;
    float  g_ReflectionStrength;
    float  g_FresnelPower;
    float  g_NormalScale;
    float4 g_NormalScroll;        // xy=scroll1, zw=scroll2
    // wave 2개 (각 32 bytes로 정렬)
    float2 g_Wave0_Dir; float g_Wave0_Amp; float g_Wave0_Wavelen;
    float  g_Wave0_Speed; float3 _wave0_pad;
    float2 g_Wave1_Dir; float g_Wave1_Amp; float g_Wave1_Wavelen;
    float  g_Wave1_Speed; float3 _wave1_pad;
};

TextureCube  g_Skybox       : register(t0);
SamplerState g_Sampler      : register(s0);
Texture2D    g_NormalMap    : register(t1);
SamplerState g_NormalSampler: register(s1);

// PS 합성
float4 PSMain(PSInput input) : SV_TARGET
{
    // 노멀 맵 두 장 UV scroll
    float2 uv1 = input.uv * g_NormalScale + g_NormalScroll.xy * g_Time;
    float2 uv2 = input.uv * g_NormalScale + g_NormalScroll.zw * g_Time;
    float3 n1 = g_NormalMap.Sample(g_NormalSampler, uv1).xyz * 2 - 1;
    float3 n2 = g_NormalMap.Sample(g_NormalSampler, uv2).xyz * 2 - 1;
    float3 nTan = normalize(n1 + n2);

    // TBN: plane 가정
    float3 N0 = normalize(input.normalWS);
    float3 T  = normalize(mul(float3(1,0,0), (float3x3)g_World));
    float3 B  = normalize(mul(float3(0,0,1), (float3x3)g_World));
    float3 N  = normalize(nTan.x*T + nTan.y*B + nTan.z*N0);

    // Lighting / View
    float3 V = normalize(g_CameraPositionWS.xyz - input.worldPos);
    float NdotV = saturate(dot(N, V));
    float3 L = normalize(-g_LightDirection.xyz);
    float NdotL = saturate(dot(N, L));

    // Shallow/Deep
    float3 waterCol = lerp(g_DeepColor.rgb, g_ShallowColor.rgb, NdotV);
    float3 litWater = waterCol * g_LightColor.rgb * g_LightColor.a * NdotL;

    // Fresnel + reflection
    float fresnel = pow(1 - NdotV, g_FresnelPower);
    float3 R = reflect(-V, N);
    float3 envColor = g_Skybox.Sample(g_Sampler, R).rgb;

    float3 lit = lerp(litWater, envColor, fresnel * g_ReflectionStrength);
    return float4(lit * g_TintColor.rgb, 1.0f);
}
```

**`shaders/simple.hlsl` 확장 (VS — sine wave displacement):**

```hlsl
struct WaveParams { float2 dir; float amp; float wavelength; float speed; };

float3 SineDisplace(float3 p, out float3 normalOut)
{
    WaveParams w[2] = {
        { g_Wave0_Dir, g_Wave0_Amp, g_Wave0_Wavelen, g_Wave0_Speed },
        { g_Wave1_Dir, g_Wave1_Amp, g_Wave1_Wavelen, g_Wave1_Speed },
    };

    float h = 0;
    float dx = 0, dz = 0;
    [unroll] for (int i = 0; i < 2; ++i)
    {
        float k     = 6.2831853 / w[i].wavelength;
        float omega = k * w[i].speed;
        float phase = dot(w[i].dir, p.xz) * k - omega * g_Time;
        float s = sin(phase);
        float c = cos(phase);
        h  += w[i].amp * s;
        dx += w[i].dir.x * k * w[i].amp * c;
        dz += w[i].dir.y * k * w[i].amp * c;
    }
    normalOut = normalize(float3(-dx, 1, -dz));
    return float3(p.x, p.y + h, p.z);
}

PSInput VSMain(VSInput input)
{
    PSInput o;
    float3 displaced;
    float3 waveNormal;
    displaced = SineDisplace(input.position, waveNormal);
    float4 worldPos = mul(float4(displaced, 1.0f), g_World);
    o.position = mul(mul(worldPos, g_View), g_Projection);
    o.normalWS = normalize(mul(waveNormal, (float3x3)g_World));
    o.worldPos = worldPos.xyz;
    o.uv = input.uv;
    return o;
}
```

amplitude 0이면 변위 없음 → 평면 입력 normal로 회귀. ImGui로 amplitude=0 슬라이더로 끄기 가능.

**참고 자료:**

- Schlick approximation (Fresnel) — Real-Time Rendering 4th, Ch. 9
- [Catlike Coding — Waves](https://catlikecoding.com/unity/tutorials/flow/waves/) (Gerstner / sine 식 도식)
- [GPU Gems Ch.1 — Effective Water Simulation](https://developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-1-effective-water-simulation-physical-models)
- [DirectXTK CreateWICTextureFromFile / CreateDDSTextureFromFile](https://github.com/microsoft/DirectXTK/wiki/WICTextureLoader)

## 3. Inputs / Outputs

| 종류    | 이름                       | 형식                       | 범위 / 기본값                                  |
|---------|---------------------------|---------------------------|------------------------------------------------|
| Asset   | `assets/textures/water_normal.dds` | 2D normal map     | tile 가능. 사용자 준비 (DDS 권장, PNG/JPG도 허용) |
| CBuffer | `g_ShallowColor`           | `float4`                  | (0.50, 0.85, 1.00, 1) 청록                     |
| CBuffer | `g_DeepColor`              | `float4`                  | (0.05, 0.15, 0.40, 1) 짙은 파랑                |
| CBuffer | `g_FresnelPower`           | `float`                   | 1~8, 기본 5                                    |
| CBuffer | `g_NormalScale`            | `float`                   | 0.1~5, 기본 1.0                                |
| CBuffer | `g_NormalScroll` (xy/zw)   | `float4`                  | xy=(0.03, 0.02), zw=(-0.02, 0.04)              |
| CBuffer | `g_Wave0_*`, `g_Wave1_*`   | dir/amp/wavelen/speed     | 기본: amp=0.05, wavelen=2.0, speed=0.5         |
| Resource| `g_Skybox` (t0)            | `TextureCube`             | Day 3 그대로 — 변경 없음                       |
| Resource| `g_Sampler` (s0)           | `SamplerState`            | linear, clamp                                  |
| Resource| `g_NormalMap` (t1)         | `Texture2D`               | DDS/WIC SRV                                    |
| Resource| `g_NormalSampler` (s1)     | `SamplerState`            | linear, **wrap**                               |
| RS      | `doubleSided`              | `RasterizerState`         | CullMode=NONE, FrontCCW=TRUE                   |
| ImGui   | Water 섹션                 | `SeparatorText("Water")`  | Fresnel/Color/Normal/Waves 묶음                |

ImGui 슬라이더:
- Fresnel Power (1~8)
- Shallow Color (ColorEdit3)
- Deep Color (ColorEdit3)
- Normal Scale (0.1~5)
- Normal Scroll1 (xy, -0.2~0.2)
- Normal Scroll2 (xy, -0.2~0.2)
- Waves[0..1] TreeNode: Direction Angle(°), Amplitude, Wavelength, Speed

**의존하는 다른 feature:** `env-cubemap`(g_Skybox/g_Sampler/g_CameraPositionWS/g_ReflectionStrength 그대로 재사용), `scene-lighting`(World/View/Proj/Light cbuffer + Lambert), `asset-pipeline`(plane mesh + UV).

## 4. Acceptance Criteria / Test Plan

- [ ] 빌드 성공, C++/HLSL 경고 0
- [ ] **Fresnel:** plane 위에서 내려다보면 shallow 색 위주 + 약한 반사, 옆에서 보면 거의 거울 (강한 환경 반사)
- [ ] **Shallow/Deep:** 시야각이 깊어질수록 deep 색이 더 보임 (NdotV → 0)
- [ ] **Normal scroll:** 시간 흐름에 따라 표면 normal이 흐르고 반사가 어른거림
- [ ] **Vertex displacement:** plane이 sine 파동으로 출렁임. 파동에 따라 반사 방향도 변동
- [ ] **양면 그리기:** plane을 아래에서 봐도 보임 (양면 RS 검증)
- [ ] **ImGui:** Fresnel Power, Shallow/Deep, Normal Scale, Scroll, Wave amp/wavelen/speed/dir 모두 즉시 반영
- [ ] 회귀: Skybox toggle, Reflection Strength, Light(Yaw/Pitch/Color/Intensity), Tint, Rotation, Camera 자유 이동, FOV, hot-reload 정상
- [ ] 종료 시 D3D 디버그 라이브 오브젝트 경고 0

## 5. 구현 메모 (참고용)

- **Fresnel power 5**가 stylized 물의 흔한 시작점. 1~3은 거의 모든 곳에서 반사, 8 이상은 grazing 끝에서만 반사.
- **TBN plane 가정**: 우리 plane은 XZ에 누워 있고 model rotation만 적용됨. T=worldX, B=worldZ로 간주. 추후 일반 메시에 적용하려면 vertex tangent를 추가하거나 partial derivative `ddx/ddy(worldPos)`로 cotangent frame 도출.
- **Sine 합 normal 재계산은 미분 식**: `y = A·sin(phase)`이면 `∂y/∂x = ∂phase/∂x · A·cos(phase) = dir.x · k · A · cos(phase)`. 누적 합 후 `N = normalize(-∂y/∂x, 1, -∂y/∂z)`.
- **Gerstner 확장**: 같은 식에 `disp.xz += dir * A · cos(phase)` 한 줄 추가 — 마루 뾰족, 골 평평. normal 식도 약간 달라짐(꼬임 보정). 시간 남으면 폴리쉬에서 추가.
- **Sampler wrap (s1)**: 노멀 맵은 tile해서 흐르므로 wrap. skybox(s0)는 clamp 그대로.
- **Texture stub 채우기**: 현재 `src/Texture.h`는 `Initialize`가 `return true`만 하는 빈 stub. 일반 2D DDS/WIC 로더로 채워서 normal map과 향후 일반 텍스처 모두 지원하도록 한다.
- **양면 RS 이유**: water plane은 단면이라 카메라가 아래로 들어갔을 때 백 컬링되어 보이지 않음. CullMode=NONE으로 양면 그리기.
- **cbuffer 사이즈 변화**: 기존 272 bytes(Day 3) → +`g_ShallowColor`(16) +`g_DeepColor`(16) +`g_NormalScale`/`g_FresnelPower` 행(16, 기존 _padding 활용 + 추가) +`g_NormalScroll`(16) +Wave[2]×32 = 64. 총 ~400 bytes. ColorShader::InitializeShader의 cbuffer 크기는 sizeof(PerFrameCB)로 이미 자동 계산됨.
- **Wave 기본값**: amp=0.05, wavelen=2.0, speed=0.5, dir0=(1,0), dir1=(0.7,0.7) 정도가 작은 plane에서 자연스러운 작은 출렁임.
- **Time 스케일 주의**: `g_Time`이 누적이므로 wave의 `phase = dot(dir, p.xz)·k − ω·time`에서 time이 크면 cos이 매우 빠르게 진동. 첫 수 분간 동작 확인 후 amp를 조절하는 게 안전.

이 메모는 SPEC이 아니라 출발점. 실제 구현 중 발견 사항은 같은 폴더의 `NOTES.md`에 기록.
