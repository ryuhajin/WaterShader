# top-view-sun — top 샷 전용 태양 (회고 기록)

> 2026-10-04 · feature/top-view-sun (feature/preset-themes에서 분기)

캡처 조건은 이전 feature와 같다(1280×720, `t = 12s`, 고정 샷 6개, 무인 모드).
재현: `WaterShader.exe --capture-feature top-view-sun --capture <label> [--preset-file <path>]`

---

## 문제

- top 샷은 2×2 평면을 바로 위(높이 2.4)에서 내려다본다. 화면 가장자리까지 시선은 수직에서 약 22° 안이다.
- 그래서 해가 평면 정면, 즉 거의 머리 위에 있어야 해의 정반사(글린트)가 카메라로 들어온다.
- preset-themes의 bench 세팅(step7)은 이 때문에 태양 자체를 옮겼다(Basic 86°, Tropical 62°). 그러자 sunward/oblique 샷의 반짝임 길이 약해졌다. Sunset은 노을 고도 4°를 바꿀 수 없어 물 색을 바꿔 버텼다.
- 사용자 요청: top 뷰만 따로 방향광 세팅을 저장하고 싶다.

## 설계 결정

- **적용 조건 (사용자 결정): top 고정 샷일 때만.**
  - `IsTopShotView()`는 매 프레임 카메라 위치·회전·평면을 top 샷(`kCaptureShots[1]`)과 정확히 비교한다. 상태 플래그가 없어서 top 버튼, 캡처, 카메라 슬롯 어느 경로로 와도 같고, 조금이라도 움직이면 바로 원래 태양으로 돌아간다.
  - FOV는 비교하지 않는다. 줌은 내려다보는 구도를 바꾸지 않는다.
  - `kTopShotIndex`가 다른 샷을 가리키게 되면 `static_assert`(위치 y = 2.4, pitch = 89.9)에서 빌드가 멈춘다.
- **저장 범위 (사용자 결정): 방향(yaw, 고도) + 세기.** 색은 원래 태양 색을 쓴다.
- **셰이더 변경 없음.** CPU에서 방향·세기를 골라 기존 `g_LightDirection`, `g_LightColor.a`로 보낸다. 하늘의 해 원반도 같은 값을 따른다(top에서는 하늘이 거의 안 보인다).
- **프리셋 키:** `topSun` `topSunYawDeg` `topSunElevationDeg` `topSunIntensity`. 기존 key-value 확장이라 파일 버전은 그대로 v4다. 키가 없으면 꺼짐(기본 고도 90°, 세기 1)이다.
- **UI:** Light 창의 Sun 아래에 "Top View Sun (top shot only)" 체크박스와 슬라이더 3개를 둔다. 지금 적용 중인지 표시한다. 적용 중일 때는 Sun 섹션에도 "top 샷에서는 아래 값을 쓴다"는 안내가 뜬다.

## 검증 (AI agent)

- Debug/Release 빌드 경고 0
- 비교는 같은 exe로 찍은 캡처 18장의 파일 해시로 했다.

| 테스트 | 입력 | 결과 |
|---|---|---|
| 키 없는 파일 회귀 | 당시 `assets/shader_presets.txt` | `before`와 18장 동일 |
| top 샷에서만 적용 | 모든 줄에 `topSun 1`(90°, 1.5)을 붙인 `test/topsun_on.txt` | top 3장만 다르고 나머지 15장 동일 |
| 저장 왕복 | 시작 시 Load → Save → Load (임시 코드, 이후 되돌림) | 키 4개가 6줄 모두에 저장되고 18장 동일 |

- 카메라를 움직이면 원래 태양으로 돌아가는지, UI 표시가 맞는지는 화면 조작이 필요해서 사용자가 확인한다.

## 프리셋 적용 (step1_topsun)

`docs/features/preset-themes/make_bench_presets.ps1`의 bench 덮어쓰기를 바꿨다. ocean 줄은 그대로다.

| | Basic | Sunset | Tropical |
|---|---|---|---|
| bench 원래 태양 | (213.3°, 86.2°) → ocean 값 (40°, 30°) | 그대로 (34.5°, 4°) × 2.2 | 고도 62° → ocean 값 (35.2°, 36.9°) |
| top 샷 태양 | 90° × 1.6 | 90° × 1.0 (색은 노을 주황) | 90° × 1.74 |

- **top:** Basic은 이전과 비슷하게 가운데 반짝임이 산다. Tropical은 반짝임이 평면 전체로 퍼진다. Sunset은 주황 반짝임이 물결을 따라 평면 전체에 깔린다.
- **sunward:** Basic·Tropical은 태양이 ocean 위치로 돌아와 반짝임 길이 다시 생겼다. Sunset은 그대로다.
- 비교 시트: `compare/top.jpg`, `compare/sunward.jpg` (local-only)

## step2 — 사용자 확인 후 수정

### Sunset bench를 저장값으로 되돌림 (`step2_sunset_saved`)

- 사용자 요청으로 Sunset bench의 물 색과 노멀을 앱에서 저장한 ocean 값으로 되돌렸다.
- 같은 이유(top 반짝임이 없을 때 top 뷰를 살리려던 것)로 넣었던 강한 태양(2.2), 약한 환경광(0.45), F0 0.04도 함께 뺐다. 이제 Sunset bench는 파도와 top 태양만 ocean과 다르다.
- 결과: top은 짙은 남색 위에 주황 반짝임이 물결을 따라 드러난다. sunward는 저장값의 파란 물 색이다. 바뀐 캡처는 Sunset bench 3장(oblique / sunward / top)뿐이다.

### "top 태양으로 바뀌는지 모르겠다" — 표시 보강

- **원인:** top 태양이 켜져도 Light 창의 Sun 슬라이더는 그대로다. 슬라이더는 원래 태양 값이고, top 태양 값은 아래 섹션에 따로 있다. 적용 중이라는 표시가 회색 작은 글씨 한 줄이라 눈에 띄지 않았다.
- **수정:** 지금 어느 태양을 쓰는지를 강조색(wave "Custom"과 같은 노랑, `kHighlightColor`)으로 표시한다.
  - Stats 창: `Sun:  Top View Sun (top shot)`(노랑) / `Sun:  preset sun`
  - Light 창 Sun 섹션: top 샷에서는 "이 슬라이더는 top 샷에서 쓰이지 않음"
  - Top View Sun 섹션: `IN USE`(노랑) / `Waiting`(켜져 있지만 top 샷이 아님) / `Off`

### 동작 검증 — 카메라를 움직이면 원래 태양으로

- 임시 빌드에서 캡처의 top 샷 카메라만 x로 1 mm 옮겼다(이후 되돌림).
  - top 태양을 켠 파일(현재 프리셋)과 끈 파일(`test/topsun_off.txt`)로 각각 찍었다.
  - 세 테마 모두 top 이미지가 파일 해시까지 같았다. 조금이라도 움직이면 원래 태양으로 돌아간다는 뜻이다.
- 정확히 top 샷일 때 top 태양이 쓰이는 것은 앞의 "top 샷에서만 적용" 테스트(top 3장만 다름)로 확인했다.
- 앱 창 확인: `--no-input --ui light --shot top`으로 띄워 Stats 창에 노란 `Sun: Top View Sun (top shot)`이 뜨는 것을 확인했다.
