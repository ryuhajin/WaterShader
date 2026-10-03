# ui-panels

> Branch: `feature/ui-panels` (from `feature/normal-select`) · Status: done · Updated: 2026-10-03

## 1. Goal

- **한 문장 요약:** 한 창에 다 들어 있던 ImGui(`Shader Bench`)를 **View / Light / Water** 세 창으로 나누고 숫자 키 1/2/3으로 켜고 끄며, Water 설정을 처음 보는 사람도 이해할 수 있는 이름·분류로 다시 짠다.
- **문제:** 카메라·프리셋·라이팅·환경·물·톤매핑·캡처·디버그가 한 창에 세로로 이어져 화면을 가리고 원하는 항목을 찾기 어렵다. Water 항목은 내부 용어(`Layer A - U speed`, `Wave 0~3`, `Detail Layer Scale (B / A)`)라 의미를 알 수 없다.
- **회고 포인트:** "누가 쓰는 UI인가" 기준으로 분류·이름을 바꾼 과정과, 보이는 값(방향+속도)과 저장 값(u, v)을 분리한 방법.
- **스코프 가드:** 셰이더·렌더 결과 변경 없음(렌더 캡처 픽셀 동일), 프리셋 파일 포맷 변경 없음.

## 2. 접근

| 창 (키) | 내용 |
|---|---|
| View Settings (1) | 프리셋 Apply/Save, 카메라, 카메라 프리셋, 씬(Ocean Grid·Skybox·모델 회전), 캡처, 디버그 뷰 |
| Light Settings (2) | 하늘/환경 + 하늘에서 보정, 태양, 태양 글린트, ambient, 톤매핑 |
| Water Settings (3) | 물 색, 반사, 노멀맵(레이어 A/B: 텍스처·크기·흐름 방향/속도), 파도 4개(크기별 이름 + 나침반 + 시점 기준 설명) |

- 제거: Specular Strength/Sharpness(0으로 꺼 둔 중복 태양), Reset Light(HDR 이전 기본값), Time 텍스트(Stats와 중복).
- 기본은 세 창 모두 닫힘, Stats에 `[1] View [2] Light [3] Water` 안내. 텍스트 입력 중에는 단축키 무시.

## 3. Acceptance Criteria

- [x] 1/2/3 키(숫자패드 포함)로 창 토글, X 버튼과 동기화, 텍스트 입력 중 무시
- [x] Water 창: 대분류 + 슬라이더 위 라벨, 흐름은 방향/속도, 파도는 역할 이름 + 나침반 + 시점 기준 설명
- [x] 렌더 결과 픽셀 동일(회귀), 프리셋 저장 값 왕복 동일
- [x] UI before/after 스크린샷 + NOTES
- [x] Debug/Release 경고 0
