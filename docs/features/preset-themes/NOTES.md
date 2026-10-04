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

## 최종 값 (step2_tune)

값의 원본은 `make_presets.ps1` 맨 위 표다. 이 스크립트가 `assets/shader_presets.txt`를 쓴다.

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
| 파도 size / height / chop / speed | 3.6 / 0.05 / 0.30 / 0.6 | 3.0 / 0.12 / 0.75 / 1.35 | 4.8 / 0.10 / 0.50 / 0.85 |
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
| step2_tune | Basic 물 색을 조금 밝고 푸르게(반사 0.6 → 0.7). Sunset spread 50 → 40, 태양을 더 주황으로. Tropical Facing·Grazing 밝게, 노출 −0.7 → −0.5, 노멀 세기 0.6 → 0.7. | Sunset 원경 격자가 줄었다. Tropical이 에메랄드로 읽힌다. |

비교 시트: `compare/ocean_sunward.jpg`, `compare/ocean_wide.jpg`, `compare/ocean_away.jpg` (local-only).

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

- **벤치 평면(oblique / top / sunward 샷):** 2×2 평면은 Sunset의 파장 3, 높이 0.12 파도를 담기에 작다. 천처럼 접혀 보이고, 가파른 면 일부가 하늘을 반사해 하얗게 보인다. 포트폴리오 샷은 ocean 그리드를 쓰므로 그대로 둔다.
- **Align을 켠 레이어의 흐름 방향 (코드 문제, 이번 범위 밖):**
  - `PixelShader.hlsl:122-124` 주석은 "스크롤은 평면 uv 기준이라 Flow direction 의미가 유지된다"고 한다.
  - 하지만 `uv = Rotate(uv × scale) + scroll × t` 구조에서는 무늬가 월드에서 `−R⁻¹ × scroll` 방향으로 움직인다. 정렬 회전각만큼 돌아간 방향이다(수식상 확인, 화면 측정은 하지 않음).
  - 이번 프리셋에서 회전각은 Basic A −14°, Sunset A −49°다. 실제 흐름은 UI에 표시된 방향보다 그만큼 돌아가 있다.
  - 프리셋 값으로 보정하지 않았다. 셰이더를 고치면 보정값이 틀어지기 때문이다. 수정하려면 스크롤을 회전 전에 더하면 된다. `Rotate(uv × scale + scroll × t)`
- 앱에서 Save Current로 값을 고치면 `make_presets.ps1`의 표와 달라진다. 이후 원본은 `assets/shader_presets.txt`다.
