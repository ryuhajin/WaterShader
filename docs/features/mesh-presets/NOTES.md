# mesh-presets — 평면별 프리셋 (회고 기록)

> 2026-10-04 · feature/mesh-presets

캡처 조건은 이전 feature와 같다(1280×720, `t = 12s`, 고정 샷 6개, 무인 모드).
재현: `WaterShader.exe --capture-feature mesh-presets --capture <label> [--preset-file <path>]`

---

## 문제

- preset-themes에서 오션 그리드에 맞춘 파도(파장 3~5, 높이 0.1 이상)가 2×2 벤치 평면에서는 천처럼 접혔다. 가파른 면 일부는 하늘을 반사해 하얗게 보였다.
- 반대로 벤치에 맞추면 오션에서 파도가 너무 작다. 프리셋 하나로 두 평면을 모두 만족시킬 수 없다.

## 설계 결정

- **프리셋 전체를 평면별로 저장한다** (사용자 결정). 테마 3개 × 평면 2개 = 6개.
  - 물·파도만 나누는 안도 있었다. 하지만 어느 값이 공유인지 헷갈리지 않는 쪽을 택했다. 대신 조명을 고치면 두 평면에서 각각 저장해야 한다.
- **평면을 바꾸면 그 평면의 버전을 자동으로 적용한다** (사용자 결정).
  - `activeTheme_`: 마지막으로 불러오거나 저장한 테마
  - `presetOceanMesh_`: 지금 값이 어느 평면 것인지
  - `Frame()` 맨 앞에서 `oceanMode_ != presetOceanMesh_`면 `ApplyPreset(activeTheme_)`을 호출한다.
  - 체크박스, 카메라 슬롯 Load, 고정 샷 버튼처럼 `oceanMode_`를 바꾸는 경로가 여럿이다. 각 경로에 훅을 거는 대신 한 곳에서 차이를 보고 처리한다.
  - 캡처는 각 샷 평면의 버전을 직접 적용하고 `presetOceanMesh_`를 맞춘다. 끝나면 캡처 전 값(저장 안 한 조정값 포함)을 복원하고 `presetOceanMesh_ = oceanMode_`로 둔다. 그래서 캡처가 조정값을 덮어쓰지 않는다.
- **파일 포맷 v4:** 줄마다 앞에 `bench` / `ocean`을 붙이고, 같은 평면 안에서는 테마 순서다.
  - 사람이 파일을 열어도 어느 줄이 어느 평면인지 보인다.
  - 모르는 평면 이름이나 네 번째 테마 줄은 건너뛴다.
  - v3 파일은 계속 읽고, 같은 값을 두 평면에 넣는다. 저장은 항상 v4다.
- View 창 Presets에 현재 평면(`Kept per mesh - now: bench plane / ocean grid`)과 현재 테마(`< current`)를 표시한다.

## 검증 (AI agent)

- Debug/Release 빌드 경고 0
- 모든 비교는 같은 exe로 찍은 캡처 18장의 파일 해시로 했다(캡처는 결정적: 같은 입력이면 같은 JPEG).

| 테스트 | 입력 | 결과 |
|---|---|---|
| v3 회귀 | main의 `assets/shader_presets.txt`(v3) | `before`와 18장 동일 |
| v4 파싱 | 같은 값을 bench/ocean 두 벌로 쓴 `test/v4_same.txt` | 18장 동일 |
| 평면별 적용 | ocean 줄만 노출 −1.5 EV인 `test/v4_ocean_darker.txt` | ocean 샷 9장만 다르고, 벤치 샷 9장은 동일 |
| 저장 왕복 (v3 → v4) | 시작 시 Load → Save → Load (임시 코드, 이후 되돌림) | 파일이 v4로 바뀌고 18장 동일 |
| 저장 왕복 (v4 → v4) | 위와 같음 | 저장된 파일 텍스트가 원본과 같고 18장 동일 |

- 평면 전환 자동 적용(체크박스·슬롯 Load·샷 버튼)과 Save Current의 평면 구분은 화면 조작이 필요해서 사용자가 확인한다.

## 남은 점

- preset-themes의 세 테마는 아직 오션 기준 값 하나뿐이다. preset-themes에서 벤치 버전을 따로 튜닝한다.
