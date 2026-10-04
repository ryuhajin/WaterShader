# preset-themes

> Branch: `feature/preset-themes` · Status: in-progress · Updated: 2026-10-04

## 1. Goal / Visual Target

- **한 문장 요약:** Basic / Sunset / Tropical 세 프리셋을 포트폴리오 대표 이미지용 테마로 다시 튜닝한다. 테마마다 파도 기하(Simple 6값)와 노멀맵 조합을 서로 다르게 잡는다.
- **테마:**
  - **Basic:** 일반적인 바다나 호수. 잔잔하게 흐르는 느낌.
  - **Sunset:** 노을 진 바다. 바람이 세서 파도가 거칠다.
  - **Tropical:** 휴양지의 에메랄드빛 바다. 파도는 Basic보다 조금 크다.
- **현재 문제:**
  - Sunset·Tropical은 기본 파도의 높이만 0.8배·0.75배로 줄인 값이라 파도 모양이 사실상 같다.
  - Basic은 Advanced에서 손으로 고친 값이라 Custom으로 표시된다. 원경이 하얗게 번진다(wave-far-normals NOTES).
  - Tropical은 청록에 가깝고, Grazing 색이 Facing보다 밝게 뒤집혀 있다.
- **시각 목표 키워드:** 잔잔함과 흐름 / 거친 바다 위 노을 반짝임 길 / 에메랄드빛 얕은 바다
- **스코프 가드 — 절대로 만들지 않을 것:**
  - 코드·셰이더 변경 없음. `assets/shader_presets.txt`와 문서만 바꾼다.
  - 새 하늘·새 노멀맵 텍스처 추가 없음. 기존 환경 7종, 노멀맵 5종 안에서 고른다.
  - 프리셋 파일 버전업 없음.

## 2. 접근법 / 수식

셰이더는 그대로이고, 프리셋 값만 바꾼다.

- **파도:** 테마별 Simple 6값(wind, spread, size, height, chop, speed)을 정하고, `GenerateWaves`(`src/WaveMacro.h`)와 같은 식으로 계산한 파도 4개(24개 값)를 함께 적는다. 로드할 때 파도는 재생성되지 않으므로, 둘이 맞아야 Custom이 아닌 Simple로 표시된다.
- **파도 성격을 가르는 값:** 1번 파 기울기 `kA = 2π/size × height`, chop, spread, speed.
  - Gerstner 루프 방지: 파도 4개의 steepness 합 / 4 < 1.
- **노멀맵:** 레이어 A(넓은 무늬)·B(잔 디테일) 조합, Align to wind, 타일링, 흐름 방향·속도를 테마별로 다르게 잡는다.
  - 저장값: UI 흐름 방향 d°, 속도 s → `normalScroll = (−cos d, −sin d) × s`
- **조명·색:** 하늘에서 보정한 태양 방향·세기를 기준으로, 물 색·반사·글린트·노출을 테마에 맞춘다.

## 3. Inputs / Outputs

| 종류 | 이름 | 형식 | 범위 / 비고 |
|---|---|---|---|
| Preset | 고정 25개 + 파도 4×6 | float | `assets/shader_presets.txt` v3, 한 줄에 한 프리셋 |
| Preset | `wave*` 6개 | key value | Simple 슬라이더 표시값, 파도 24개와 일치해야 함 |
| Preset | `normalMapA/B`, `rippleAlignA/B`, `normalStrength`, `detailScale` | key value | 테마별 노멀 조합 |
| Preset | `sunGlint*`, `farGlintSpread`, `fresnelF0`, `environment`, `exposureEV` | key value | 테마별 조명 |
| Output | 캡처 6샷 × 3프리셋 | jpg | `captures/<label>/` (local-only) |

**의존하는 다른 feature:** `wave-macro`(Simple 생성 규칙), `wave-far-normals`(원경 기울기·Far spread·Align to wind), `normal-select`(노멀맵 5종), `hdr-env`·`basic-sky`(하늘과 보정값).

## 4. Acceptance Criteria / Test Plan

- [ ] 세 프리셋 모두 Water 창에서 Custom이 아닌 Simple로 표시
- [ ] 세 프리셋의 Simple 6값과 노멀맵 조합이 서로 다름
- [ ] Basic: 원경이 하얗게 번지지 않고, 파도가 잔잔하게 흐름
- [ ] Sunset: 파도가 거칠지만 Gerstner 고리·뾰족한 꼭짓점이 없음, 노을 반짝임 길이 보임
- [ ] Tropical: 에메랄드빛으로 읽힘, 파도가 Basic보다 큼
- [ ] 노출이 날아가지 않음 (흰 픽셀 비율 확인)
- [ ] before / 단계별 캡처 + 비교 시트 + NOTES
