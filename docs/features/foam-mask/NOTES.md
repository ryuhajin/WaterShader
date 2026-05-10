# foam-mask — 진행 현황 + 재개 가이드

> 2026-05-10 · feature/foam-mask · **Status: deferred (post-2026-05-11)**

## 1. 폐기 사유

D-1 (마감 2026-05-11 15:00) 시점에서 시간 제약 + 우선순위 재배치로 본 마감 범위에서 제외. 마감 산출물은 **water-base + water-normal-map**까지로 확정. foam은 마감 후 재개 예정.

메모리 컨벤션 그대로: 폐기/미완 기능도 SPEC을 main에 박아 시도 기록을 남긴다. 본 NOTES는 그 시도가 어디까지 갔고 재개할 때 어떤 자산이 살아있는지 기술한다.

## 2. 어디까지 갔는지

**셰이더 — 구현 완료:**
- `shaders/Common.hlsli` — cbuffer에 `g_FoamColor`, `g_FoamMaskParams`(scale/threshold/softness), `g_FoamCrest`(crestMin/crestMax/scrollU/scrollV) 추가. PSInput에 `crestHeight : TEXCOORD2` 추가.
- `shaders/vertexShader.hlsl` — `output.crestHeight = displaced.y - input.position.y` 한 줄로 sine wave 변위량 PS 전달.
- `shaders/PixelShader.hlsl` — `ComputeFoamCrest()` (crestMin~crestMax 정규화), `SampleFoamMask()` (scroll + threshold/softness smoothstep), 최종 합성 라인 `lit = lerp(lit, g_FoamColor.rgb, foam)` 추가. `g_FoamMap : register(t2)` 바인딩, `g_NormalSampler` (s1) 재사용. Debug Mode 4(=foam mask raw), 5(=crest factor) 추가.

**C++ 와이어링 — 구현 완료:**
- `src/ColorShader.cpp::PerFrameCB` — foam 3개 필드 동기화. `data->foamColor/foamMaskParams/foamCrest` 채움.
- `src/ColorShader.h::WaterParams` — foam 파라미터 + 기본값. `Render()`/`RenderShader()` 시그니처에 `foamSRV` 인자 추가.
- `src/ColorShader.cpp::RenderShader` — SRV 슬롯 t0/t1/t2 (cubemap, normal, foam), 3개 동시 바인드.
- `src/Graphics.cpp::Initialize` — `foamMap_` 텍스처를 `wave_foam.dds → .png → .jpg → R=0 flat` 폴백 체인으로 로드. `attempts[]` 패턴은 normal map과 동일.
- `src/Graphics.cpp::DrawImGuiPanel` — `Foam (whitecap on wave crests)` 섹션 신설: Color/Mask Scale/Threshold/Softness/Crest Min/Max/Scroll U/V.
- `src/Graphics.h` — `foamMap_`, `foamMapStatus_` 멤버 추가.

**자산 — 자체 제작 1장:**
- `assets/textures/wave_foam.dds` (BC4_UNORM, 512×512, R-channel mask, texconv 변환분)
- `assets/textures/wave_foam.png` (PNG 원본, 동일 패턴)
- `assets/textures/waveForm.psd` (Photoshop 편집 원본)

**미진행:**
- 시각 검증. ImGui Debug Mode 4/5 + 0번(정상 렌더)에서 foam이 wave crest 위에 점박이로 떠 있는지 확인 필요.
- sweet spot 파라미터 조정. 현재 기본값(Threshold 0.6, Softness 0.1, Crest Min/Max 0.02/0.08)은 짐작값.
- main 머지.

## 3. 재개 가이드

```
git checkout feature/foam-mask
cmake --build build/vs2022 --config Debug
./build/vs2022/Debug/WaterShader.exe
```

ImGui 패널에서:

1. **Foam Map Loader**: `[OK] DDS loaded -> ...wave_foam.dds` 확인 (Debug View 섹션 맨 아래)
2. **Debug Mode 4 (Foam mask raw)** — 검은 배경에 흰 점박이 패턴이 표면에 보이는지. `Foam Mask Scale` 1 → 5로 키우면 패턴 작아져야 함.
3. **Debug Mode 5 (Crest factor)** — Wave 0/1 amplitude 0.05~0.1로 두면 마루 부분만 흰색 그라디언트로 표시. amplitude=0이면 전부 검정.
4. **Debug Mode 0 (정상 렌더)** — wave 마루에 흰 거품 점박이. 골에는 거품 없음.
5. 슬라이더 검증:
   - Foam Threshold 0.0 → 1.0 sweep: 거품 점박이 면적 줄어듦
   - Foam Crest Min 0 → 0.1: 큰 마루만 거품, 작은 마루 사라짐
   - Foam Scroll U/V 0.05: 거품 패턴이 시간에 따라 흐름
   - Foam Color RGB/alpha: 색상 + 최대 블렌드 강도

검증 통과 시 `feature/foam-mask` → main 머지 (`git merge --no-ff`). SPEC Status는 `deferred → done`로 갱신.

## 4. 알려진 이슈 / 트러블슈팅 단서

- **None known at code-time.** 시각 검증을 안 했을 뿐, 컴파일은 0 경고 통과 확인.
- **JPG 디코드 실패 가능성:** Photoshop이 직접 export한 JPG를 normal map에서 본 적 있음(`feature/water-normal-map` NOTES 참조). foam은 처음부터 DDS+PNG 둘 다 둬서 회피.
- **cbuffer 크기 변화:** PerFrameCB가 400 → 448 바이트로 확장됨 (foam 3 × float4). HLSL 16-byte 정렬 그대로 유지.
- **TEXCOORD2 충돌:** 다른 셰이더(skybox)와 슬롯 겹침 없음 — color/water용 PSInput에만 추가.

## 5. 마감 후 재개 시 우선순위

본 feature를 재개하기 전에 main에 있는 다음 항목을 먼저 처리할 수도 있다:

- **떠 있는 specular stash (`git stash list`)** — Blinn-Phong specular. 본 foam보다 시각 임팩트는 작지만 normal 효과를 살려주는 보완. `feature/water-specular` SPEC 작성 후 정식 머지.
- **water-detail-normal** — Macro + Detail 2-layer normal map. 실제 ocean 렌더의 표준 패턴이라 포트폴리오 가치 큼.

foam은 wave amplitude가 충분(0.05+)할 때 비주얼 임팩트가 큼. waterfall(Day 6) 추가 전에 풍부한 호수 씬을 만들고 싶으면 foam → waterfall 순서가 자연스러움.

## 6. 참고 — 본 시도에서 남긴 셰이더 스니펫 (재이용 가치)

`feature/foam-mask` 브랜치의 `shaders/PixelShader.hlsl`에 다음 두 함수가 살아있음:

```hlsl
float ComputeFoamCrest(float crestHeight)
{
    return saturate((crestHeight - g_FoamCrest.x) /
                    max(g_FoamCrest.y - g_FoamCrest.x, 1e-4f));
}

float SampleFoamMask(float2 uv)
{
    float2 foamUV = uv * g_FoamMaskParams.x + g_FoamCrest.zw * g_Time;
    float r = g_FoamMap.Sample(g_NormalSampler, foamUV).r;
    return smoothstep(g_FoamMaskParams.y - g_FoamMaskParams.z,
                      g_FoamMaskParams.y + g_FoamMaskParams.z, r);
}
```

재개 시 그대로 통과 가능. 만약 detail normal feature를 먼저 머지하면 sampler 슬롯 정리에서 g_NormalSampler 재할당이 일어날 수 있음 — 그땐 foam의 sampler 참조도 같이 갱신.
