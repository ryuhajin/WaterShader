# ocean-shots — ocean 고정 샷 3개 재구성 (회고 기록)

> 2026-10-04 · feature/ocean-shots (feature/top-view-sun에서 분기)

캡처 조건은 이전 feature와 같다(1280×720, `t = 12s`, 무인 모드). 프리셋은 당시 `assets/shader_presets.txt`다.
재현: `WaterShader.exe --capture-feature ocean-shots --capture <label>` → `captures/<label>/`, 비교 시트 `compare/` (`make_compare.ps1 -Dir ../ocean-shots step1_initial step2_aerial step3_aerial`).

---

## 문제

ocean 고정 샷 세 개가 서로 비슷해 보였다(`captures/before`).

| 옛 이름 | 위치 | pitch / yaw | 화각 |
|---|---|---|---|
| `ocean_sunward` | (−0.9, 0.55, −1.4) | 6 / 34.5 | 55° |
| `ocean_wide` | (0, 1.6, −3) | 14 / 10 | 60° |
| `ocean_away` | (0, 1.6, 0) | 8 / 214.5 | 60° |

셋 다 높이 0.55~1.6, 화각 55~60°이고, 수평선이 화면 가운데쯤 오는 구도였다. 방향만 조금씩 달랐다.

## 설계 — 높이 · 렌즈 · 해 기준 방향을 모두 다르게

사용자 결정: 구성은 수면 역광 / 항공 부감 / 망원 압축이다. 이름은 ocean 샷만 바꾸고, 벤치 샷과 테마 이름은 그대로 둔다.

| 새 이름 | 높이 | 렌즈 | 해 기준 | 보여 주는 것 |
|---|---|---|---|---|
| `ocean_surface` | 0.3 (수면) | 광각 70° | 정면 (yaw 34.5) | 앞쪽 마루, 수평선까지 반짝임 길 |
| `ocean_aerial` | 12 (부감) | 50° | 오른쪽 20° (yaw 15) | 파도 무늬 전체, 원경 페이드, 넓은 글리터 |
| `ocean_tele` | 1.8 | 망원 28° | 반대 (yaw 214.5) | 압축된 파도 줄, 해안·건물 반사 |

- 모든 테마의 태양이 yaw 34.5~40 근처라(Basic 40, Sunset 34.5, Tropical 35.2) 한 세트로 세 테마 모두 같은 구도가 된다.
- **surface 높이 0.3:** 가장 높은 테마의 파도 진폭 합(약 0.19, Sunset ocean)보다 높아 카메라가 물에 잠기지 않는다.
- **aerial 화면 위쪽 끝:** `pitch − 수직화각/2`가 수평선 아래에 있어야 한다. 높이 12에서 그리드 가장자리(±400)는 수평선 아래 1.7°에 있고, 이게 프레임에 들어오면 그리드 너머 틈이 보인다. 최종값은 30 − 25 = 5°다(`XMMatrixPerspectiveFovLH`라 화각은 세로 기준).
- **aerial 조준점:** 원점을 바라본다. 그리드 정점이 원점에서 가장 촘촘하다(`InitializeGrid(…, 1024, 400, 6)`).

## 과정

| 단계 | 바꾼 것 | 결과 |
|---|---|---|
| before | 옛 샷 3개 | 비슷한 수평선 구도 세 장 |
| step1_initial | 계획값. aerial은 (−5.8, 12, −16.1), pitch 35, yaw 20, 55° | surface와 tele는 의도대로 나왔다. aerial은 카메라가 파도 진행 방향을 거의 정면으로 마주 봐서, 파도 줄이 화면에 수평으로 반복되는 줄무늬처럼 보였다. |
| step2_aerial | aerial yaw 0, pitch 30, 50°, 위치 (0, 12, −20.8) | 파도 줄이 대각선이 되어 역동적이고 원근감이 커졌다. 하지만 반짝임이 오른쪽 가장자리로 밀려 일부 잘렸다. |
| step3_aerial | aerial yaw 15, 위치 (−5.38, 12, −20.09) | 반짝임이 화면 안 오른쪽에 들어오고, 파도 줄도 비스듬하다. **최종값** |

비교 시트: `compare/ocean_surface.jpg`, `compare/ocean_aerial.jpg`, `compare/ocean_tele.jpg` (local-only)

## 검증 (AI agent)

- Debug/Release 빌드 경고 0
- 벤치 9장(oblique / top / sunward)이 `before`와 해시까지 같다.
- 육안 확인:
  - surface: 카메라가 물에 잠기지 않고, 마루와 반짝임 길이 보인다.
  - aerial: 그리드 틈이 없고, 파도 무늬가 원경까지 이어진다. 위쪽 가장자리에 보이는 나무와 산은 하늘 반사다.
  - tele: 메시 → 픽셀 노멀 전환이 티 나지 않는다.
- `make_compare.ps1`의 샷 목록에 새 이름을 추가했다. 옛 이름도 남겨 두어 이전 feature 캡처로 비교 시트를 다시 만들 수 있다.

## 남은 점

- aerial Sunset의 왼쪽 원경에 파도 4개가 교차하며 생기는 마름모 무늬가 약하게 보인다. 높은 곳에서 넓게 보면 Gerstner 4파의 반복이 드러난다. 샷 문제가 아니라 파도 구성(spread)의 특성이다.
- 이전 feature NOTES에 나오는 옛 샷 이름(`ocean_sunward` / `ocean_wide` / `ocean_away`)은 당시 기록이라 그대로 두었다.
