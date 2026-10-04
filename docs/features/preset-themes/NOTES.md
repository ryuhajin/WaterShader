# preset-themes — 포트폴리오용 3테마 프리셋 (회고 기록)

> 2026-10-04 · feature/preset-themes

캡처 조건은 이전 feature와 같다(1280×720, `t = 12s`, 고정 샷 6개, 무인 모드).
재현: `WaterShader.exe --capture-feature preset-themes --capture <label>` → `captures/<label>/`, 비교 시트 `compare/` (`make_compare.ps1 -Dir ../preset-themes before step1_initial step2_tune`).
변경 전 프리셋은 `before_presets.txt`에 있다. `--preset-file`로 읽으면 before를 다시 찍을 수 있다.

---

## 문제

- Sunset·Tropical의 파도는 기본 파도에서 높이만 0.8배·0.75배로 줄인 값이었다. 세 테마 중 두 개의 파도 모양이 사실상 같았다.
- Basic은 Advanced에서 손으로 고친 값이라 Custom으로 표시됐다. 파도 사이 관계도 깨져 있었다(wave-macro NOTES). spread가 커서 원경에 마름모 격자가 보였고, 해 쪽 원경이 하얗게 번졌다.
- Tropical은 청록에 가까웠다. Grazing(0.08, 0.74, 0.68)이 Facing(0.05, 0.55, 0.55)보다 밝게 뒤집혀 있었다.
- Sunset은 태양 세기가 0이라 노을 반짝임 길이 하늘 반사로만 생겼다.

## 테마 정의

| 테마 | 의도 | 파도 | 노멀맵 |
|---|---|---|---|
| Basic | 일반 바다/호수, 잔잔하게 흐름 | 길고 낮고 느림, 좁은 spread | 3 Long streaks(바람 정렬) + 1 Soft swell |
| Sunset | 바람 센 노을 바다 | 짧고 높고 빠름, chop 최대에 가깝게, 교차하는 spread | 0 Diagonal ripples(바람 정렬) + 4 Fine chop |
| Tropical | 에메랄드빛 휴양지 바다 | Basic보다 길고 높음, 중간 chop | 2 Soft chop + 4 Fine chop |

## 최종 값 (step3_bench 시점)

> 이후 ocean 줄은 사용자가 앱에서 다듬었고, bench 줄은 step7에서 다시 만들었다. 현재 값의 원본은 `assets/shader_presets.txt`다. bench 덮어쓰기 값은 아래 "벤치 평면 세팅"에 있다.

값의 원본은 `make_presets.ps1` 맨 위 표다. 이 스크립트가 `assets/shader_presets.txt`(v4, [mesh-presets](../mesh-presets/NOTES.md))를 쓴다. 벤치 평면과 오션 그리드 버전은 파도 크기만 다르고 나머지 값은 같다.

| | Basic | Sunset | Tropical |
|---|---|---|---|
| 환경 / 노출 | 6 HDR Field day / −0.4 EV | 4 HDR Sunset sea / −1.0 | 5 HDR Beach day / −0.5 |
| 태양 yaw·고도 | 40 / 30 | 34.5 / 4 | 236.4 / 25.2 |
| 태양 색 × 세기 | (1, 0.97, 0.91) × 1.6 | (1, 0.50, 0.22) × 1.0 | (0.98, 1, 0.92) × 1.74 |
| 환경광 색 × 세기 | (0.78, 0.88, 1) × 0.95 | (0.93, 0.85, 1) × 0.9 | (0.586, 0.753, 1) × 0.60 |
| Facing / Grazing | (0.18, 0.40, 0.50) / (0.05, 0.15, 0.24) | (0.10, 0.12, 0.18) / (0.04, 0.05, 0.09) | (0.16, 0.82, 0.64) / (0.03, 0.50, 0.50) |
| 반사 세기 / F0 / Fresnel 지수 | 0.7 / 0.04 / 5 | 1.0 / 0.02 / 5 | 0.8 / 0.02 / 5 |
| 글린트 sharpness / 세기 / Far spread | 1200 / 1.2 / 0.8 | 300 / 1.0 / 1.6 | 900 / 1.0 / 1.0 |
| 파도 wind / spread | −105 / 25 | −125 / 40 | −80 / 35 |
| 파도 size / height / chop / speed (ocean) | 3.6 / 0.05 / 0.30 / 0.6 | 3.0 / 0.12 / 0.75 / 1.35 | 4.8 / 0.10 / 0.50 / 0.85 |
| 파도 size / height (bench, 나머지는 ocean과 같음) | 1.2 / 0.0166 | 1.0 / 0.04 | 1.6 / 0.0334 |
| 1번 파 기울기 kA | 0.087 | 0.251 | 0.131 |
| 노멀맵 A / B (Align) | 3 / 1 (A만) | 0 / 4 (A만) | 2 / 4 (없음) |
| normalScale / detail / strength | 0.8 / 2.5 / 0.45 | 1.3 / 3.2 / 0.9 | 1.0 / 3.0 / 0.7 |
| Flow A / B (방향°, 속도) | −105, 0.020 / −75, 0.012 | −125, 0.050 / −100, 0.035 | −80, 0.025 / −50, 0.018 |

## 값 선택 근거

- **파도 성격은 1번 파 기울기 `kA = 2π/size × height`로 갈랐다.** Basic 0.087 < Tropical 0.131 < Sunset 0.251.
  - Tropical은 길이(4.8)와 높이(0.10)가 모두 Basic보다 크다. "파도가 더 큼"이 모양과 기울기 양쪽에서 드러난다.
  - Sunset은 길이는 가장 짧고 높이는 높다. chop 0.75, 속도 1.35배로 거칠고 빠르다.
  - Gerstner 고리 방지: 파도 4개의 steepness 합 / 4 = (0.75 + 0.80 + 0.85 + 0.90) / 4 = 0.825 < 1.
- **바람 방향은 카메라 쪽으로 밀려오게 잡았다.**
  - 카메라 yaw θ의 시선을 바람 각도(0 = +X, 90 = +Z)로 바꾸면 90 − θ다. sunward 계열 샷(yaw 33~34.5)의 시선은 약 55°다.
  - Sunset −125°는 해 쪽에서 카메라로 정면으로 온다. 역광 마루가 생긴다.
  - Basic −105°는 사용자가 맞춘 값을 유지했다. Tropical −80°는 비스듬히 온다.
- **spread:** Basic 25°는 원경 마름모 격자를 없앤다(wave-macro NOTES). Sunset은 처음에 50°로 잡았지만 원경 격자가 보여 step2에서 40°로 줄였다.
- **노멀 조합:** 세 조합이 모두 다르다.
  - Basic: 긴 줄무늬(3)를 바람에 정렬해 "흐름"을 만들고, 부드러운 swell(1)로 잔무늬를 채운다.
  - Sunset: 대각 물결(0)을 바람에 정렬해 바람결 줄무늬를 만들고, 잔 chop(4)을 촘촘하게 깐다(detail 3.2, strength 0.9).
  - Tropical: soft chop(2)과 fine chop(4)를 정렬 없이 섞어 맑은 물의 잔반짝임을 만든다.
  - 맵 4는 Sunset과 Tropical이 함께 쓰지만, 레이어 세기와 타일링이 달라 역할이 다르다.
- **조명·색:**
  - Basic: 해를 sunward 샷 쪽(yaw 40)으로 옮겨 반짝임 길이 프레임 안에 들어온다. 물 색은 하늘색 대신 짙은 청록 계열이다. 기울기가 작아 원경 σ²가 작고, 노출을 −0.4로 낮춰 번짐을 막았다.
  - Sunset: 하늘의 노을 피크(yaw 34.5, 고도 4)에 주황 태양을 켜고 Far spread 1.6으로 반짝임 길을 수평선까지 넓혔다. 하늘에 작은 태양 원반이 함께 그려진다. 물 색은 거의 검정에 가깝게 두고, 색은 하늘 반사가 맡는다.
  - Tropical: Facing을 밝은 에메랄드, Grazing을 짙은 청록으로 바로잡았다. 내려다볼 때 모래 바닥이 비치는 얕은 바다 느낌이다. 태양과 환경광은 하늘 보정값 그대로다.

## 과정

| 단계 | 바꾼 것 | 결과 |
|---|---|---|
| before | 기존 프리셋 (`before_presets.txt`) | — |
| step1_initial | 계획한 초기값 전부 | Basic이 잔잔해지고 반짝임 길이 생겼다. Sunset이 확실히 거칠어졌다. 하지만 Sunset 원경에 격자가 보였고, Tropical은 탁하고 어두웠다. |
| step2_tune | Basic 물 색을 조금 밝고 푸르게(반사 0.6 → 0.7). Sunset spread 50 → 40, 태양을 더 주황으로. Tropical Facing·Grazing 밝게, 노출 −0.7 → −0.5, 노멀 세기 0.6 → 0.7. | Sunset 원경 격자가 줄었다. Tropical이 에메랄드로 읽힌다. 하지만 벤치 평면(oblique/top/sunward)에서는 큰 파도 때문에 평면이 천처럼 접혔다. Sunset은 가파른 면 일부가 하늘을 반사해 하얗게 보였다. |
| step3_bench | [ripple-flow-fix](../ripple-flow-fix/NOTES.md)와 [mesh-presets](../mesh-presets/NOTES.md)를 합쳤다. 벤치 버전 파도를 따로 뒀다: 1번 파 기울기 kA는 ocean과 같게 두고, 2×2 평면에 마루가 2개 정도 들어가도록 파장을 1.0~1.6으로 줄였다. | 벤치 샷의 접힘과 하얀 면이 사라졌고, 테마별 성격(잔잔 / 거친 / 중간)은 그대로다. ocean 샷은 값이 같다. Basic·Sunset은 레이어 A 정렬 때문에 흐름 수정으로 무늬 위상만 바뀌었다. |

| (사용자) | 앱에서 ocean의 Sunset·Tropical 값을 직접 다듬어 저장했다. bench Basic의 태양도 (213.3°, 86.2°)로 옮겼다. | 이후 ocean 줄의 원본은 앱에서 저장한 `assets/shader_presets.txt`다. |
| step4_bench_before | 위 상태 그대로 캡처 | 벤치 평면 기준. 아래 "벤치 평면 세팅" 참고 |
| step5_bench_a | bench 줄을 "같은 테마의 ocean 줄 + 평면용 덮어쓰기"로 다시 만들었다(`make_bench_presets.ps1`). 파도 높이·chop을 낮추고, 노멀 타일링을 촘촘하게 했다. | 평면 가장자리 출렁임이 줄어 정사각형이 유지된다. 하지만 top에서 Sunset은 평평한 보라색이고, Tropical은 밝은 민트색 면이었다. |
| step6_bench_b | Tropical: 태양 고도 70 → 62, Facing을 조금 어둡게, 노출 −0.8, 노멀 세기 1.0. Sunset: 태양을 더 세게, 환경광을 약하게, 노멀 세기 1.0. | Tropical top에 에메랄드색과 잔물결, 반짝임이 보인다. Sunset은 여전히 거의 평평하다. |
| step7_bench_c | Sunset만 Facing을 짙은 남색에서 어스름한 보랏빛(0.36, 0.24, 0.40)으로 바꿨다. 태양 2.2, 환경광 0.45, 노멀 세기 1.2. | Sunset top에 해 받는 경사면이 주황 줄무늬로 드러나 거친 물결이 읽힌다. sunward 샷의 노을 느낌도 유지된다. |

비교 시트: `compare/sunward.jpg`, `compare/oblique.jpg`, `compare/ocean_sunward.jpg`, `compare/ocean_wide.jpg`, `compare/ocean_away.jpg`, 벤치 단계는 `compare_bench/top.jpg`, `compare_bench/sunward.jpg`, `compare_bench/oblique.jpg` (local-only).

## 벤치 평면 세팅 (step4~7)

**목표:** 2×2 평면 모양을 해치지 않으면서, top 뷰(바로 위)에서도 테마가 읽히게 한다.

**만드는 방식:** `make_bench_presets.ps1`은 ocean 줄을 읽어 bench 줄을 새로 쓴다. ocean 줄은 그대로 복사하고, 아래 덮어쓰기만 바꾼다. 하늘·물 색·노멀맵 조합 같은 테마 값은 ocean을 따르므로, 앱에서 ocean을 다시 다듬은 뒤 이 스크립트를 돌리면 bench도 따라간다.

| 덮어쓰기 | Basic | Sunset | Tropical |
|---|---|---|---|
| 파도 size / height / chop | 1.0 / 0.012 / 0.20 (spread 25, speed 0.6) | 0.8 / 0.025 / 0.45 | 1.2 / 0.016 / 0.30 |
| normalScale / normalStrength | 1.4 / 0.55 | 1.6 / 1.2 | 1.5 / 1.0 |
| 태양 | (213.3°, 86.2°) 사용자 값 유지 | 세기 2.2 (방향은 ocean과 같은 노을 위치) | 고도 62° |
| 그 외 | — | 환경광 0.45, Facing (0.36, 0.24, 0.40), F0 0.04 | Facing (0.10, 0.62, 0.50), 노출 −0.8 |

**근거:**

- **평면 모양:** Gerstner 파도는 꼭짓점을 수평으로도 움직인다. 크기는 `steepness × λ / 8π` 정도다. 가장자리가 출렁이지 않도록 chop을 0.2~0.45로 낮추고 파장을 0.8~1.2로 줄였다. 높이는 top에서는 보이지 않으니 낮게 두고, 물결 정보는 노멀맵이 맡는다.
- **top 뷰에서 무엇이 보이나:** 바로 위에서 보면 Fresnel 반사가 F0 수준(2~4%)이라 하늘 반사가 거의 없다. 남는 건 두 가지다.
  1. **태양 글린트:** 해가 거의 머리 위에 있을 때만 화면 안에 들어온다. Basic(86°)과 Tropical(62°)은 이것으로 수면이 읽힌다.
  2. **해를 받는 경사면의 밝기(N·L) 차이:** Sunset은 노을 위치(고도 4°)를 바꿀 수 없어 이쪽을 키웠다. 깊은 파랑은 주황 햇빛을 거의 반사하지 않아(선형값으로 R ≈ 0.03) 효과가 없었다. 그래서 물 색을 노을빛을 받는 보랏빛으로 옮기고, 환경광을 줄여 대비를 살렸다.
- **노멀 타일링:** ocean 값(0.8~1.0)은 2×2 평면에 무늬가 한 번 남짓 들어가 top에서 너무 크다. 1.4~1.6으로 촘촘하게 했다.
- **Basic의 태양:** 사용자가 bench Basic에 직접 넣은 머리 위 태양을 그대로 두었다. top 반짝임이 가장 잘 사는 값이다. 대신 sunward/oblique 샷의 반짝임 길은 약해진다.

## 검증 (AI agent)

- **파도 24개 값과 Simple 6값이 일치한다.** `make_presets.ps1`이 `GenerateWaves`와 같은 식으로 계산한다. 반올림은 6자리라 `WavesMatchMacro` 허용오차(상대 3%, sharpness 0.01, 방향 1°)보다 훨씬 작다. 그래서 세 프리셋 모두 Custom이 아니어야 한다. 앱 화면 확인은 사용자 몫이다.
- **흰 픽셀 비율:** 해수면 영역(화면 아래 70%)에서 RGB 모두 > 245인 비율을 쟀다. ocean 샷 3개 기준:

| | before | step2_tune |
|---|---|---|
| Basic sunward / wide / away | 1.35% / 0.70% / 0% | 0.50% / 0.82% / 0% |
| Sunset | 0% / 0% / 0% | 0.03% / 0% / 0% |
| Tropical | 0% / 0% / 0.53% | 0% / 0% / 0.65% |

  흰 픽셀은 모두 글린트 길 안에만 있다. 원경이 하얗게 번지지 않는다.

## 남은 점

- ~~벤치 평면에서 큰 파도가 접힘~~ → step3에서 평면별 프리셋으로 해결.
- ~~Align을 켠 레이어가 Flow 다이얼과 다른 방향으로 흐름~~ → [ripple-flow-fix](../ripple-flow-fix/NOTES.md)에서 셰이더를 수정했다. 원인은 회전한 뒤에 스크롤을 더한 것이었다. 실측으로 확인했다. 흐름 값은 다이얼 기준으로 정했으므로 프리셋은 보정하지 않았다.
- 앱에서 Save Current로 값을 고치면 `make_presets.ps1`의 표와 달라진다. 이후 원본은 `assets/shader_presets.txt`다.
