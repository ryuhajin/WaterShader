# ocean-shots

> Branch: `feature/ocean-shots` · Status: done · Updated: 2026-10-04

## 1. Goal / Visual Target

- **한 문장 요약:** ocean 고정 샷 3개를 높이 · 화각 · 해 기준 방향이 모두 다른 구성(수면 역광 / 항공 부감 / 망원 압축)으로 바꾸고, 이름을 `ocean_surface` / `ocean_aerial` / `ocean_tele`로 바꾼다.
- **문제:** 지금 `ocean_sunward` / `ocean_wide` / `ocean_away`는 셋 다 높이 0.55~1.6, 화각 55~60°이고 수평선이 화면 가운데쯤 와서 비슷해 보인다.
- **시각 목표 키워드:** 수면 높이의 역광과 반짝임 길 / 위에서 본 파도 무늬 / 망원으로 압축된 파도 줄
- **스코프 가드 — 절대로 만들지 않을 것:**
  - 벤치 샷(`oblique` / `top` / `sunward`)과 테마 프리셋 이름은 바꾸지 않는다 (사용자 결정)
  - 셰이더와 프리셋 값은 바꾸지 않는다
  - 샷 개수는 6개 그대로다 (ocean은 인덱스 3~5)

## 2. 접근법

`src/Graphics.cpp`의 `kCaptureShots`에서 ocean 항목 3개만 바꾼다.

| 새 이름 | 의도 | 위치 | pitch / yaw | 화각 |
|---|---|---|---|---|
| `ocean_surface` | 수면 역광 | (0, 0.30, 0) | 3 / 34.5 (해 쪽) | 70° |
| `ocean_aerial` | 항공 부감 | (−5.38, 12, −20.09), 원점을 바라봄 | 30 / 15 | 50° |
| `ocean_tele` | 망원 압축 | (0, 1.8, 0) | 3 / 214.5 (해 반대) | 28° |

- surface: 카메라 높이 0.3이 가장 큰 파도 진폭 합(약 0.19, Sunset ocean)보다 높다.
- aerial: 화면 위쪽 끝이 수평선 아래 5°라서, 그리드 가장자리(수평선 아래 1.7°)가 프레임에 들어오지 않는다.
- `make_compare.ps1`의 샷 목록에 새 이름을 추가하고, 옛 이름도 남겨 둔다.

## 3. Inputs / Outputs

| 종류 | 이름 | 형식 | 범위 / 비고 |
|---|---|---|---|
| Code | `kCaptureShots[3..5]` | CaptureShot | 이름 · 위치 · 회전 · 화각 |
| CLI | `--shot <name>` | string | 새 이름으로 시작 |
| Capture | `<preset>_<shot>.jpg` | jpg | ocean 3장 이름이 바뀜 |

**의존하는 다른 feature:** `bench-tools`(고정 샷·캡처), `mesh-presets`(샷 평면별 프리셋)

## 4. Acceptance Criteria / Test Plan

- [x] Debug/Release 빌드 경고 0
- [x] 벤치 9장이 수정 전과 해시까지 같다
- [x] surface: 카메라가 물에 잠기지 않고, 앞쪽 마루와 반짝임 길이 보인다
- [x] aerial: 그리드 가장자리나 하늘 틈이 없고, 파도 무늬가 원경까지 이어진다
- [x] tele: 메시 → 픽셀 노멀 전환과 모아레가 티 나지 않고, 파도 줄이 압축되어 보인다
- [x] 세 샷이 한눈에 다른 구도로 읽힌다
- [x] NOTES
