# foam-mask

> Branch: `feature/foam-mask` · Status: in-progress · Updated: 2026-05-10

## 1. Goal / Visual Target

- **한 문장 요약:** sine wave 마루(crest) 위에 자체 제작 noise mask로 흰색 물보라(whitecap)를 띄워, 정적인 수면을 거친 호수/근해 느낌으로 만든다.
- **시각 목표 키워드:** whitecap, wave crest foam, stylized noise breakup.
- **참고 이미지/영상:** Phase 3 캡처 → `references/foam-mask-20260510.png`.
- **스코프 가드 — 절대로 만들지 않을 것:**
  - **해변(shore) foam** — depth buffer SRV 노출 필요, 별도 feature(`feature/shore-foam`)로 분리. 본 feature는 wave crest only.
  - **시간 기반 ripple SDF** — 별도 `feature/ripple-sdf`.
  - **Foam 그림자 / sub-surface scattering** — 스코프 외.
  - **Animated foam dissipation (생성/소멸)** — 정적 mask + threshold만. dissolve 효과는 추후.

## 2. HLSL 접근법 / 의사코드

**핵심 수식 — wave crest detection:**

vertex shader가 sine 합으로 정점을 변위시킨 뒤 displaced.y를 PSInput에 추가로 흘려 PS에서 crest 정도를 판정.

```
crestFactor = saturate( (displacedY - g_FoamCrestMin) / (g_FoamCrestMax - g_FoamCrestMin) )
            // displacedY가 g_FoamCrestMin 이하면 0 (골)
            // g_FoamCrestMax 이상이면 1 (마루)
```

**핵심 수식 — noise mask threshold:**

```
foamSample = g_FoamMap.Sample(g_NormalSampler, uv * g_FoamScale + scroll * g_Time).r
mask       = smoothstep(g_FoamThreshold - g_FoamSoftness, g_FoamThreshold + g_FoamSoftness, foamSample)
foamAmount = saturate(crestFactor * mask)
finalColor = lerp(litWater, g_FoamColor.rgb, foamAmount)
```

**의사코드 (PS 추가분):**

```hlsl
// VS에서 displacedY를 PSInput에 추가로 보낸다 (PSInput 확장)
float3 displacedPos : TEXCOORD2;  // y component used for crest

// PS 안에서:
float crest = saturate((input.displacedPos.y - g_FoamCrestMin) /
                       max(g_FoamCrestMax - g_FoamCrestMin, 1e-4f));

float2 foamUV = input.uv * g_FoamScale + g_FoamScroll * g_Time;
float  fSamp  = g_FoamMap.Sample(g_NormalSampler, foamUV).r;
float  mask   = smoothstep(g_FoamThreshold - g_FoamSoftness,
                           g_FoamThreshold + g_FoamSoftness, fSamp);
float  foam   = saturate(crest * mask);

float3 lit = lerp(litWater_with_reflection, g_FoamColor.rgb, foam);
```

`g_NormalSampler`(wrap, linear)를 그대로 재사용해 sampler 슬롯을 새로 추가하지 않음.

**참고 자료:**
- Catlike Coding — [Waves > Foam](https://catlikecoding.com/unity/tutorials/flow/waves/) — crest factor 패턴
- GPU Gems Ch.1 — Effective Water Simulation — wave crest 시각화
- Sea of Thieves GDC talk — 3-layer noise foam blending (스코프 외 참고)

## 3. Inputs / Outputs

| 종류    | 이름                  | 형식                       | 범위 / 기본값                                      |
|---------|----------------------|---------------------------|--------------------------------------------------|
| Asset   | `assets/textures/water_foam.dds` (또는 `.png`) | 2D R-channel | **사용자 자체 제작.** 256×256, R=mask. WIC 호환 위해 PNG 권장 또는 texconv DDS 변환 |
| Texture | `g_FoamMap` (t2)     | `Texture2D`               | wrap sampler 재사용 (s1)                          |
| CBuffer | `g_FoamColor`        | `float4`                  | (1.0, 1.0, 1.0, 1) 흰색                           |
| CBuffer | `g_FoamParams`       | `float4`                  | x=scale, y=threshold, z=softness, w=crestK       |
| CBuffer | `g_FoamScroll`       | `float4`                  | xy=UV/sec, zw=crestMin/crestMax (혹은 분리)       |
| ImGui   | Foam 섹션            | `SeparatorText("Foam")`   | Color/Scale/Threshold/Softness/CrestRange/Scroll |
| Output  | foam blend 추가       | `float3`                  | lit water 위에 alpha-blend 비슷한 lerp           |

ImGui 슬라이더:
- Foam Color (ColorEdit3)
- Foam Mask Scale (0.5~10)
- Foam Threshold (0~1)
- Foam Softness (0.01~0.5)
- Foam Crest Min (-0.05~0.1) / Crest Max (0.0~0.3) — sine wave amplitude 범위 안에서
- Foam Scroll U/V (per sec) — 2개

**의존하는 다른 feature:** `water-base` (sine wave + lit water), `water-normal-map` (g_NormalSampler s1, ImGui Debug View 패턴 재사용).

## 4. Acceptance Criteria / Test Plan

- [ ] 빌드 성공, C++/HLSL 경고 0
- [ ] **Wave crest 시각:** Wave amplitude 0.05~0.1 sweep 시 마루 부위에만 흰 점이 떠야 함 (골 부위는 깨끗)
- [ ] **Threshold sweep:** 0 → 1로 올리면 거품 점박이 면적이 점점 줄어 사라져야 함
- [ ] **Softness sweep:** 0.01(딱딱한 가장자리) → 0.5(부드럽게 풀어짐)
- [ ] **Crest Min/Max:** Min을 amplitude 최대치보다 크게 두면 거품 사라짐 (검증)
- [ ] **Foam Scroll:** Scroll = 0이면 거품 패턴 정지, 0.05면 흐름
- [ ] **Loader 검증:** ImGui Debug View에 `[OK] DDS/PNG loaded -> water_foam.*` 표시, 폴백 분기 안 탐
- [ ] **Debug View 모드 확장:** 4=foam mask raw, 5=crest factor 추가 (시각 검증용)
- [ ] 회귀: Normal Scale/Scroll, Wave amp/wavelen/speed/dir, Fresnel, Skybox toggle, Light 모두 정상
- [ ] **Foam off:** Foam Color alpha 또는 별도 toggle로 완전 끄기 가능 (회귀 비교용)
- [x] 자체 제작 텍스처 사용 — `assets/textures/water_foam.{dds,png}` (포트폴리오 self-made 카운트 +1, 누적 2장)

## 5. Notes (선택)

- **VS→PS 전달:** PSInput에 `float3 displacedPos : TEXCOORD2` 추가. `worldPos`(TEXCOORD0)와는 별도로 변위된 로컬/월드 좌표를 보내 crest 판정 안정성 확보. 또는 단순히 `float displacedY : TEXCOORD2` 단일 채널만 보내도 충분.
- **Crest 판정 안정성:** sine 합 진폭이 작을 때(amplitude=0.02) crest 범위가 좁아 거품이 잘 안 보임. amplitude를 0.05~0.1로 권장.
- **Foam mask 스타일:** 점박이(small dots) 권장. 너무 큰 패턴이면 "거품"이 아니라 "구름"처럼 보임.
- **DDS 변환:** Photoshop PNG → texconv `-f BC4_UNORM` (R 채널 단일 압축)로 DDS 변환. JPG는 water-normal-map에서 본 WIC 디코드 이슈로 비추천.
- **Loader 패턴 재사용:** `water-normal-map`에서 만든 attempts[] 루프 + multi-line status 그대로 복제. foam map용 별도 `foamMapStatus_` 멤버 추가.
- **g_NormalSampler 재사용:** 별도 sampler 안 만들고 s1 wrap sampler 공유. 슬롯 절약.
