# wave-macro

> Branch: `feature/wave-macro` · Status: in-progress · Updated: 2026-10-03

## 1. Goal / Visual Target

- **한 문장 요약:** Gerstner 파도 4개를 슬롯별 슬라이더 20개로 직접 맞추는 대신, 상위 파라미터 6개(바람 방향·퍼짐·크기·높이·거칠기·속도)로 4개 파도를 자동 생성하는 **Simple** 영역을 Water 창에 추가하고, 기존 슬롯 UI는 **Advanced** 접기 메뉴로 내린다.
- **문제:** 자연스러운 파도를 만들려면 "길이 약 1.5배 간격, 높이∝길이, 속도∝√길이, 바람 방향 주변으로 부채꼴"이라는 규칙을 사용자가 머릿속으로 4개 슬롯에 나눠 맞춰야 한다. 처음 보는 사람은 Wave 1~4의 관계부터 이해하기 어렵다.
- **참고:**
  - Unreal Engine Water 플러그인 `GerstnerWaterWaveGeneratorSimple` — 파도 개수·최소/최대 파장·진폭·바람 각도·방향 퍼짐·큰/작은 파도 steepness로 파도 묶음을 생성하는 같은 구조
  - GPU Gems 1, Ch.1 "Effective Water Simulation from Physical Models" — https://developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-1-effective-water-simulation-physical-models
- **회고 포인트:** 아티스트가 만지는 값(바람·크기·거칠기)과 셰이더가 쓰는 값(파도 4개 × 5)을 분리한 설계, 기존 기본값에서 생성 규칙을 역산한 과정.
- **스코프 가드 — 절대로 만들지 않을 것:**
  - 셰이더 변경 없음 (`vertexShader.hlsl`, cbuffer 레이아웃 그대로)
  - 파도 개수 변경, FFT 바다, 랜덤 시드 기반 생성 없음
  - 프리셋 파일 버전업 없음 (기존 key-value 확장 규칙만 사용)
  - 프리셋을 불러올 때 파도 값을 바꾸지 않음 → 렌더 결과 동일

## 2. 접근법 / 수식

셰이더는 그대로이고, CPU(`src/WaveMacro.h`)에서 Simple 값 → 파도 4개를 계산해 `WaterParams::waves`에 써넣는다.

**슬롯별 고정 비율표** — 현재 기본 파도(`ColorShader.h`)를 역산한 값. 기본 Simple 값이 지금의 기본 파도를 그대로 재현한다.

| 슬롯 | 길이 비율 | 높이 비율 | 방향 오프셋 (× spread) | sharpness 오프셋 |
|---|---|---|---|---|
| Wave 1 | 1.0 | 1.0 | 0 | +0.00 |
| Wave 2 | 0.65625 | 0.6 | −7/12 | +0.05 |
| Wave 3 | 0.3875 | 1/3 | +7/12 | +0.10 |
| Wave 4 | 0.25625 | 0.2 | −1 | +0.15 |

**수식:**

```
wavelength_i = size × lenRatio_i
amplitude_i  = height × ampRatio_i
speed_i      = speedScale × 0.55 × sqrt(wavelength_i / 1.6)   // 깊은 물 분산: 속도 ∝ √파장
steepness_i  = clamp(chop + steepOffset_i, 0, 1)
angle_i      = wind + dirOffset_i × spread
direction_i  = (cos angle_i, sin angle_i)
```

**Simple ↔ Advanced 관계:**

- Simple 슬라이더 중 하나라도 바뀌면 4개 파도를 모두 다시 생성해 덮어쓴다.
- Advanced 슬롯은 항상 직접 편집할 수 있다.
- "Custom" 여부는 저장하지 않는다. 매 프레임 `GenerateWaves(simple)` 결과와 현재 파도를 허용오차로 비교해 판정한다.
  - 허용오차: 길이·높이·속도 상대 3%, sharpness 0.01, 방향 1°

**옛 프리셋 호환:** Simple 키가 없는 프리셋은 파도 값에서 Simple 값을 역산(`EstimateWaveMacro`)한다.

- Wave 1에서 바람 방향·크기·높이·거칠기·속도를 구한다.
- spread는 Wave 4와의 각도 차로 구한다.
- 역산 결과로 다시 생성한 파도가 원래 파도와 다르면 Custom으로 표시된다.

## 3. Inputs / Outputs

| 종류 | 이름 | 형식 | 범위 / 비고 |
|---|---|---|---|
| ImGui | Wind direction | slider + 나침반 | −180..180°, 기본 20 (월드 기준, 시점 기준 설명 표시) |
| ImGui | Direction spread | slider | 0..90°, 기본 60 |
| ImGui | Wave size | slider | 0.8..8, 기본 1.6 (Wave 1 마루 간격, 최소 0.8 → Wave 4 ≥ 0.2) |
| ImGui | Height | slider | 0..0.3, 기본 0.03 (Wave 1 높이) |
| ImGui | Choppiness | slider | 0..0.85, 기본 0.55 (Wave 4 sharpness ≤ 1) |
| ImGui | Speed | slider | 0..3 ×, 기본 1.0 |
| Preset | `waveWindDeg` `waveSpreadDeg` `waveSize` `waveHeight` `waveChop` `waveSpeed` | key value | 없으면 파도에서 역산 |
| Output | `WaterParams::waves[4]` | struct | 기존 cbuffer 경로 그대로 |

**의존하는 다른 feature:** `ui-panels`(Water 창, 나침반·시점 설명 헬퍼), `water-polish`(Gerstner 4파).

## 4. Acceptance Criteria / Test Plan

- [x] Debug/Release 빌드 경고 0
- [x] `WaveMacroTest` 통과: 기본값 재현, Generate→Estimate 왕복, 경계값에서 NaN 없음·sharpness ≤ 1·파장 ≥ 0.2·방향 단위벡터
- [x] 렌더 회귀: `--capture` 15장이 변경 전과 픽셀 차이 0 (로드 시 파도 불변)
- [x] Water 창: Simple 6개 + 상태 줄(Generated / Custom), Advanced(기본 닫힘) 안에 기존 슬롯 그대로
- [ ] Advanced 수정 → Custom 표시 → Simple 슬라이더 이동 시 재생성
- [ ] 프리셋 전환 시 Simple 값이 프리셋별로 바뀜, Save 후 재시작해도 유지
- [x] UI before/after 스크린샷 + NOTES
