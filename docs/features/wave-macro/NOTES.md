# wave-macro NOTES

## 문제

- Water 창의 파도는 Gerstner 파도 4개 × 슬라이더 5개(방향·높이·길이·속도·날카로움), 총 20개였다.
- 자연스러워 보이려면 슬롯끼리 지켜야 하는 관계가 있다. 길이는 약 1.5배 간격, 높이는 길이에 비례, 속도는 √길이에 비례, 방향은 바람 주변 부채꼴이다.
  - 이 관계가 UI에 드러나지 않아서, 처음 보는 사람은 "Wave 1~4가 서로 무슨 관계인지"부터 막혔다.
  - 실제로 직접 고친 Basic 프리셋에는 이 관계가 깨진 부분이 있었다. Wave 3이 Wave 2보다 길었고, 가장 긴 파도가 가장 느렸다.

## 생성 규칙 역산

- 원래 기본 파도(`ColorShader.h`)는 사람이 맞춘 값이다. 슬롯별 비율을 구해 보니 깔끔한 고정 비율표로 떨어졌다.

| | Wave 1 | Wave 2 | Wave 3 | Wave 4 |
|---|---|---|---|---|
| 길이 | 1.60 | 1.05 (×0.656) | 0.62 (×0.388) | 0.41 (×0.256) |
| 높이 | 0.030 | 0.018 (×0.6) | 0.010 (×1/3) | 0.006 (×0.2) |
| 방향 | 20° | −15° (−35°) | 55° (+35°) | −40° (−60°) |
| sharpness | 0.55 | 0.60 | 0.65 | 0.70 |

- 방향 오프셋은 spread 60° 기준으로 0, −7/12, +7/12, −1배다. 작은 파도일수록 바람에서 더 벗어난다.
- 속도는 이미 깊은 물 분산 관계 `속도 ∝ √파장`을 1~2% 이내로 따르고 있었다. 그래서 Speed 슬라이더는 이 규칙에 곱하는 배율로 만들었다.
- 결과적으로 Simple 기본값(바람 20°, spread 60°, size 1.6, height 0.03, chop 0.55, speed 1)이 원래 기본 파도를 그대로 재현한다. `WaveMacroTest`로 확인했다.

## 설계 결정

- **Custom 표시는 저장하지 않고 매 프레임 비교로 판정한다.**
  - "Advanced에서 고쳤음" 플래그를 두면 프리셋 저장, 프리셋 전환, 캡처 복원마다 따라다녀야 한다.
  - 대신 `GenerateWaves(simple)`과 현재 파도를 허용오차로 비교한다. 허용오차는 길이·높이·속도 상대 3%, sharpness 0.01, 방향 1°다.
  - 3%는 원래 기본값 속도가 √ 규칙과 최대 2.2% 차이 나는 것을 흡수하기 위한 값이다.
- **옛 프리셋은 파도에서 Simple 값을 역산한다** (`EstimateWaveMacro`).
  - Wave 1에서 바람·크기·높이·거칠기·속도를 구하고, spread는 Wave 4와의 각도 차로 구한다.
  - 파도 값 자체는 건드리지 않는다. 그래서 렌더 결과는 그대로이고, 역산이 안 맞는 프리셋은 Custom으로 보인다.
  - Sunset/Tropical처럼 기본 파도 높이만 일정 배율로 줄인 프리셋은 Custom이 아니다.
- **프리셋 키 6개** `waveWindDeg waveSpreadDeg waveSize waveHeight waveChop waveSpeed`는 기존 key-value 확장 규칙으로 저장한다. 파일 버전은 그대로 v3이다.
- **슬라이더 범위로 Advanced 범위를 지킨다.**
  - Size 최소 0.8 → Wave 4 길이 ≥ 0.2 (Advanced 최소값)
  - Choppiness 최대 0.85 → Wave 4 sharpness ≤ 1
- **Advanced는 `NoTreePushOnOpen`으로 연다.** 처음엔 일반 TreeNode라 들여쓰기가 한 단계 늘었고, 파도 줄 오른쪽의 방향 설명이 창 밖으로 잘렸다. 들여쓰기 없이 열어서 기존 파도 줄 배치를 그대로 유지했다.

## 검증 (AI agent)

- Debug/Release 빌드 경고 0.
- `WaveMacroTest` 통과 (Debug/Release):
  - 기본값 재현
  - Generate → Estimate 왕복 4케이스
  - 슬라이더 양 끝값에서 NaN 없음, 방향 단위벡터, sharpness 0..1, 파장 ≥ 0.2
  - 높이 배율 프리셋은 Custom 아님, 개별 수정은 Custom으로 판정
- 렌더 회귀: `--capture before/after --capture-feature wave-macro` 15장 비교 → **15장 모두 최대 차이 0**. 프리셋 로드 시 파도 값이 바뀌지 않는다.
- UI 스크린샷 (`--no-input --ui water`, 앱 창 1280×1400, 로컬 imgui.ini의 Water 창 높이만 촬영 동안 늘렸다가 복구):
  - `captures/ui-before.jpg`: 기존 Wave 1~4 줄
  - `captures/ui-after.jpg`: Simple 6개 + 상태 줄, Advanced 접힘. Basic 프리셋은 손으로 맞춘 값이라 Custom(노란색)으로 표시된다.
  - `captures/ui-after-advanced.jpg`: Advanced와 Wave 1을 펼친 상태. 스크린샷용 임시 코드로 열었고, 이후 되돌렸다.
- 테스트 중 `assets/shader_presets.txt`, `camera_presets.txt`가 바뀌지 않았음을 해시로 확인했다.

## 사용자 확인 필요

- Simple 슬라이더 6개 조작감
- Advanced 수정 → Custom 표시 → Simple 조작 시 재생성
- 프리셋 전환 시 Simple 값 변경
- Save 후 재시작해도 유지되는지
