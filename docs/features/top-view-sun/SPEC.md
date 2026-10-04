# top-view-sun

> Branch: `feature/top-view-sun` · Status: in-progress · Updated: 2026-10-04

## 1. Goal / Visual Target

- **한 문장 요약:** 프리셋마다 "top 고정 샷 전용 태양"(방향 + 세기)을 따로 저장하고, top 샷에서만 그 태양을 쓴다.
- **문제:**
  - top 샷은 평면을 바로 위에서 내려다본다. 그래서 정반사(sun glint)가 화면에 들어오려면 해가 평면 정면(거의 머리 위)에 있어야 한다.
  - 하지만 같은 프리셋의 oblique / sunward 샷과 테마(노을 고도 4° 등)에는 원래 태양 위치가 맞다. 태양 하나로는 둘 다 만족시킬 수 없다.
  - preset-themes에서는 이 때문에 bench 태양 자체를 옮기거나(Basic 86°, Tropical 62°), 물 색을 바꿨다(Sunset).
- **적용 조건 (사용자 결정):** View 창의 top 버튼이나 캡처의 top 샷과 **카메라가 정확히 같을 때만** 쓴다. 카메라를 조금이라도 움직이면 원래 태양으로 돌아간다.
- **저장 범위 (사용자 결정):** yaw, 고도, 세기. 색은 원래 태양과 같다.
- **스코프 가드 — 절대로 만들지 않을 것:**
  - 셰이더 변경 없음 (CPU에서 고른 방향·세기를 기존 cbuffer로 보냄)
  - 프리셋 파일 버전업 없음 (key-value 확장)
  - top 외 다른 샷별 태양 없음

## 2. 접근법

```
topSunActive = preset.topSun.enabled && 카메라 == kCaptureShots["top"] (위치·회전·평면)
sunDir       = topSunActive ? (topSun.yaw, topSun.elevation) : (sunYaw, sunElevation)
sunIntensity = topSunActive ? topSun.intensity : lightIntensity
```

- 판정은 상태 플래그 없이 매 프레임 카메라와 top 샷을 비교한다. 버튼·캡처·카메라 슬롯 어느 경로로 와도 같다.
- 하늘의 해 원반도 같은 방향·세기를 따른다(top에서는 하늘이 거의 안 보임).
- 프리셋 키: `topSun`(0/1), `topSunYawDeg`, `topSunElevationDeg`, `topSunIntensity`. 키가 없으면 꺼짐이라 기존 파일의 결과는 그대로다.

## 3. Inputs / Outputs

| 종류 | 이름 | 형식 | 범위 / 비고 |
|---|---|---|---|
| Preset | `topSun` | key value | 0 / 1, 기본 0 |
| Preset | `topSunYawDeg` / `topSunElevationDeg` | key value | 0..360 / 0..90, 기본 0 / 90 |
| Preset | `topSunIntensity` | key value | 0..5, 기본 1 |
| ImGui | Light 창 "Top View Sun" | checkbox + slider 3개 | 지금 적용 중인지 표시 |
| CBuffer | `g_LightDirection`, `g_LightColor.a` | float4 | 기존 그대로, 값만 top 샷에서 바뀜 |

**의존하는 다른 feature:** `mesh-presets`(평면별 프리셋), `bench-tools`(고정 샷·캡처)

## 4. Acceptance Criteria / Test Plan

- [x] Debug/Release 빌드 경고 0
- [x] `topSun` 키가 없는 파일: 캡처 18장이 수정 전과 같음
- [x] top 샷에서만 바뀜: `topSun`을 켠 파일로 top 3장만 다르고 나머지 15장은 같음
- [x] 저장 왕복: Save Current 후 다시 읽어도 같음
- [x] 카메라를 움직이면 원래 태양으로 돌아감 (1 mm 이동 캡처로 확인, NOTES step2)
- [x] bench 세 테마에 top 태양 세팅 + 캡처 + NOTES
