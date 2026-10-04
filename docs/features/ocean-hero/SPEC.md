# ocean-hero

> Branch: `feature/ocean-hero` · Status: in-progress · Updated: 2026-10-04

## 1. Goal / Visual Target

- **한 문장 요약:** 포트폴리오 PDF 메인으로 쓸 오션 항공 컷에서 근경 → 원경 전환을 자연스럽게 만들고, 고른 결과를 4번째 셰이더 프리셋 "Hero"와 카메라 슬롯 4에 저장한다.
- **문제:** 카메라 슬롯 4(높이 1.24, pitch 19.8, yaw 6, 화각 55) + Basic에서 중경이 일정 간격의 가로 줄무늬로 보이고, 근경 리플 · 중경 파도 줄 · 원경 거울면의 성격이 서로 다르다.
  - Basic 바람(−105°)이 시선(+Z)과 거의 평행해 마루가 화면에 가로로 눕는다. 리플 A(Long streaks)도 바람에 정렬되어 같은 가로 줄을 더한다.
  - 원경에서 normal map이 mip 평균으로 평평해지지만, 사라진 경사가 거칠기(σ²)로 넘어가지 않아 원경이 매끈한 거울이 된다.
  - 수평선 쪽 대기 원근이 없다.
- **시각 목표 키워드:** 사선으로 흐르는 물결, 넓은 돌풍 패치, 원경으로 갈수록 넓어지는 글리터, 옅어지는 수평선. 글린트가 맺히는 느낌은 유지한다.
- **스코프 가드 — 절대로 만들지 않을 것:**
  - Basic / Sunset / Tropical 프리셋 값은 바꾸지 않는다 (새 파라미터 기본값 0 = 기존 화면과 동일)
  - Gerstner wave 개수(4)와 상수버퍼의 wave 배열 레이아웃은 바꾸지 않는다
  - FFT 바다, 거품(foam-mask)은 이 기능에서 다루지 않는다

## 2. 접근법

**프리셋 / 도구**

- 셰이더 프리셋을 3 → 4개로 늘린다(`hero`). 4번째 줄이 없는 기존 파일은 Hero = Basic으로 읽는다.
- 캡처 도구: `--camera-file`, `--capture-presets`, `--capture-shots`(카메라 슬롯 `slot1`~`slot4` 포함), `--render-size WxH`, `--capture-format png`.

**셰이더 (`PixelShader.hlsl`, 새 상수 `g_DetailParams`)**

```
// a. 리플 원경 거칠기 (Toksvig): mip 평균으로 짧아진 normal 길이 → 경사 분산
σ²_ripple = Σ_layer (1 - |n|) / |n|  ×  rippleRoughness
// b. 돌풍 패치: 월드 XZ value noise 2옥타브, 바람 방향으로 흐름
gust = noise(xz / gustScale - wind·t)   → normalStrength, σ² 를 ±gustStrength 만큼 변조
// c. 수평선 대기 원근
color = lerp(color, Sky(수평 시선), hazeStrength × (1 - exp(-dist / hazeDistance)))
```

**참고 자료:**

- Toksvig, "Mipmapping Normal Maps" (2005) — 평균 normal 길이로 거칠기 추정
- Bruneton et al., "Real-time Realistic Ocean Lighting using Seamless Transitions from Geometry to BRDF" (2010)

## 3. Inputs / Outputs

| 종류 | 이름 | 형식 | 범위 / 비고 |
|---|---|---|---|
| CBuffer | `g_DetailParams.x` | float | rippleRoughness 0~2, 기본 0 |
| CBuffer | `g_DetailParams.y` | float | gustStrength 0~1, 기본 0 |
| CBuffer | `g_DetailParams.z` | float | gustScale 5~200 (월드 단위), 기본 30 |
| CBuffer | `g_DetailParams.w` | float | hazeStrength 0~1, 기본 0 |
| CBuffer | `g_FarParams.y` | float | hazeDistance 20~600, 기본 150 |
| ImGui | Water › Ocean detail | slider | 위 5개 |
| Preset | `hero` | 줄 | bench / ocean 각 1줄 |
| Camera | 슬롯 4 | 줄 | 최종 선택 구도 |
| CLI | `--camera-file` `--capture-presets` `--capture-shots` `--render-size` `--capture-format` | string | 샘플 · 최종 렌더 |

**의존하는 다른 feature:** `wave-far-normals`(원경 σ²), `bench-tools`(캡처), `ocean-shots`, `preset-themes`

## 4. Acceptance Criteria / Test Plan

- [ ] Debug/Release 빌드 경고 0
- [ ] 기존 Basic / Sunset / Tropical 18장이 수정 전과 해시까지 같다
- [ ] 기존 3줄 프리셋 파일을 읽으면 Hero = Basic, 저장 후 재시작해도 Hero 값 유지
- [ ] 디버그 모드 7(σ² / 돌풍 / haze)로 분포 확인
- [ ] 구도 4종 × 웨이브 4종 샘플 비교 시트
- [ ] 사용자가 고른 조합을 Hero 프리셋 + 카메라 슬롯 4에 저장, 2560×1440 PNG 최종 렌더
- [ ] NOTES

## 5. Notes

진행 기록은 `NOTES.md`.
