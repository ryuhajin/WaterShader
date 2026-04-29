# scene-lighting

> Branch: `feature/scene-lighting` · Status: draft · Updated: 2026-04-30

## 1. Goal / Visual Target

- **한 문장 요약:** cbuffer를 `World/View/Projection`으로 분리하고 방향광 cbuffer + 시간 cbuffer(`g_Time`)를 추가하여, plane을 회전시키면 자세에 따라 **Lambert 명암**이 변하고 ImGui Light Direction 슬라이더로 빛 방향이 즉시 반영되며 시간이 누적된다.
- **시각 목표 키워드:** Lambert 명암, world-space 라이팅, 시간 흐름. 본 feature는 후속 모든 셰이더(water/foam/waterfall)의 라이팅·시간 토대.
- **참고 이미지/영상:** 없음 (인프라 단계).
- **스코프 가드 — 절대로 만들지 않을 것:**
  - 텍스처(albedo / normal map / 마스크) 사용 안 함 — water-base/texture-assets 단계.
  - 큐브맵 반사 없음 — `feature/env-cubemap`에서.
  - Specular/Phong/Blinn 라이팅 없음 — water-base의 fresnel에서 처리.
  - 그림자, point/spot light, 멀티 라이트 없음 — 본 프로젝트 전체에서 shadow map 구현 안 함(ROADMAP 메모 참조).
  - 톤매핑/감마 보정 없음 — back buffer 그대로(LDR).

## 2. HLSL 접근법 / 수식 / 의사코드

**핵심 함수:** `mul`, `normalize`, `saturate`, `dot`.

**cbuffer 확장 (`shaders/simple.hlsl`):**

```hlsl
cbuffer PerFrameCB : register(b0)
{
    row_major float4x4 g_World;
    row_major float4x4 g_View;
    row_major float4x4 g_Projection;
    float4 g_LightDirection;  // xyz=정규화된 world dir(광원→표면), w=unused
    float4 g_LightColor;      // rgb=색, a=intensity 0~3
    float4 g_TintColor;       // 기존 base color
    float  g_Time;            // 누적 초
    float3 _padding;          // 16-byte align
};
```

**VS — world position + world-space normal:**

```hlsl
PSInput VSMain(VSInput input)
{
    PSInput o;
    float4 worldPos = mul(float4(input.position, 1.0f), g_World);
    o.position = mul(mul(worldPos, g_View), g_Projection);
    o.normalWS = normalize(mul(input.normal, (float3x3)g_World));
    o.uv = input.uv;
    return o;
}
```

**PS — Lambert 라이팅:**

```hlsl
float4 PSMain(PSInput input) : SV_TARGET
{
    float3 N = normalize(input.normalWS);
    float3 L = normalize(-g_LightDirection.xyz); // 표면→광원
    float  NdotL = saturate(dot(N, L));
    float3 lit = g_TintColor.rgb * g_LightColor.rgb * g_LightColor.a * NdotL;
    return float4(lit, 1.0f);
}
```

**Light direction 계산 (C++):** ImGui yaw/pitch (deg)에서 unit vec.

```cpp
// yaw=0 → +Z (정북). yaw 시계방향. pitch +90 → 머리 위, pitch -90 → 발 밑.
const float yawRad = XMConvertToRadians(lightYawDeg_);
const float pitchRad = XMConvertToRadians(lightPitchDeg_);
const float cosPitch = cosf(pitchRad);
XMFLOAT4 lightDir(sinf(yawRad) * cosPitch, -sinf(pitchRad), cosf(yawRad) * cosPitch, 0.0f);
```

**C++ side 변경:**

- `ColorShader::PerFrameCB` struct 확장: World/View/Proj/LightDir/LightColor/TintColor/Time + padding.
- `ColorShader::Render` 시그니처 확장: 행렬 3개 + light dir + light color(rgb,intensity) + tintColor + time.
- `Graphics::Render`가 World/View/Projection 따로 만들어 전달. 현재 `world * view * projection`로 합쳐 보내던 부분을 분리.
- `Graphics`에 멤버 추가: `lightYawDeg_=45`, `lightPitchDeg_=-45`, `lightColor_={1,1,1}`, `lightIntensity_=1.0f`, `elapsedTime_=0.0f`.
- `Graphics::Frame` 시작에서 `elapsedTime_ += deltaTime`.

**참고 자료:**

- [Microsoft HLSL constant buffer alignment](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-packing-rules)
- [Lambertian reflectance (Wikipedia)](https://en.wikipedia.org/wiki/Lambertian_reflectance)

## 3. Inputs / Outputs

| 종류    | 이름                | 형식                   | 범위 / 기본값                                        |
|---------|---------------------|------------------------|------------------------------------------------------|
| CBuffer | `g_World`           | `float4x4`             | per-draw world transform                             |
| CBuffer | `g_View`            | `float4x4`             | camera view                                          |
| CBuffer | `g_Projection`      | `float4x4`             | perspective FOV                                      |
| CBuffer | `g_LightDirection`  | `float4`               | xyz unit vec (광원→표면), w unused                   |
| CBuffer | `g_LightColor`      | `float4`               | rgb 0..1 linear, a=intensity 0..3                    |
| CBuffer | `g_TintColor`       | `float4`               | rgb 0..1 (기존)                                      |
| CBuffer | `g_Time`            | `float`                | 누적 초                                              |
| ImGui   | Light Yaw           | `SliderFloat` deg      | 0~360, 기본 45                                       |
| ImGui   | Light Pitch         | `SliderFloat` deg      | -90~90, 기본 -45                                     |
| ImGui   | Light R / G / B     | `SliderInt`            | 0~255, 기본 (255, 255, 255)                          |
| ImGui   | Light Intensity     | `SliderFloat`          | 0~3, 기본 1.0                                        |
| ImGui   | Reset Light         | Button                 | yaw=45, pitch=-45, color=white, intensity=1          |
| ImGui   | Time 표시           | `Text`                 | `"Time: %.2fs"`                                      |
| Output  | RT0 / DSV           | back buffer + D24S8    | 기존                                                 |

ImGui 패널은 `SeparatorText("Lighting")` 카테고리 추가, 그 아래 Direction(yaw/pitch) + Color RGB + Intensity + Reset + Time 한 묶음으로.

**의존하는 다른 feature:** `dx11-setup`(cbuffer + ImGui), `asset-pipeline`(plane mesh + Model Rotation), `shader-hot-reload`.

## 4. Acceptance Criteria / Test Plan

- [ ] 빌드 성공, C++/HLSL 경고 0
- [ ] plane을 Model Rotation X/Y/Z로 돌리면 자세에 따라 명암 변화 (Lambert NdotL)
- [ ] Light Yaw/Pitch 슬라이더 → plane 명암 즉시 변화 (예: pitch=-90 머리 위에서 plane이 위에서 밝게)
- [ ] Light Color 빨강 → plane 라이트가 빨간색으로 칠해짐
- [ ] Light Intensity 0 → 검정 (라이팅 꺼짐). Intensity 3 → 과노출 직전까지 밝아짐
- [ ] Reset Light 버튼 → 기본값 복귀 (yaw=45, pitch=-45, color=white, intensity=1)
- [ ] `g_Time`이 시간에 따라 증가하며 ImGui Text가 일정 속도로 갱신 (예: 30초 후 ~30.00s 표시)
- [ ] 회귀: Tint Color, Model Rotation, Camera (WASD/QE/화살표/FOV/Reset), shader hot-reload 모두 정상
- [ ] 종료 시 D3D 디버그 레이어 라이브 오브젝트 경고 0

## 5. 구현 메모 (참고용)

- **cbuffer alignment**: HLSL cbuffer는 16-byte 단위로 정렬. `float g_Time` 다음에 `float3 _padding`을 두어 한 행 채움. C++ `PerFrameCB` struct도 동일하게 padding(매크로 alignment 또는 명시적 `float padding[3]`).
- **light dir 정규화**: ImGui yaw/pitch에서 매 프레임 계산 후 cbuffer로. 슬라이더가 직접 vec3 노출하면 사용자가 unit vec 유지 부담.
- **intensity를 LightColor.a에 병합**: cbuffer 슬롯 절약. 분리해도 무방. 내부 컨벤션으로 유지.
- **시간 누적**: `Graphics::Frame(deltaTime, input)`에서 `elapsedTime_ += deltaTime`. wraparound 처리는 안 함(float 정밀도에서 수십 분 단위는 충분).
- **world matrix**: `Graphics::Render`가 modelRotation_으로 만든 X*Y*Z 곱을 그대로 `g_World`로. 현재 합쳐 보내던 MVP 라인을 분리.
- **Render 시그니처 변화**: ColorShader::Render의 인자가 늘어나니 호출자(Graphics::Render) 한 곳만 갱신. 향후 인자가 더 늘어나면 `RenderParams` struct로 묶는 것 고려 — 본 feature에서는 단순 인자 확장.
- **Reload 시 cbuffer 재생성 불필요**: cbuffer는 Initialize에서 한 번 만들고 layout만 hot-reload에서 재생성. 본 feature에서 cbuffer 사이즈가 커지므로 Initialize에서 새 size로 재할당. Reload는 셰이더만 다시 컴파일.

이 메모는 SPEC이 아니라 구현 시 출발점. 실제 구현 중 발견되는 사항은 같은 폴더의 `NOTES.md`에 기록.
