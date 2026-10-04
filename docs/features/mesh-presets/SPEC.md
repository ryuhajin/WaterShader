# mesh-presets

> Branch: `feature/mesh-presets` · Status: in-progress · Updated: 2026-10-04

## 1. Goal / Visual Target

- **한 문장 요약:** Basic / Sunset / Tropical 프리셋을 벤치 평면(2×2)과 오션 그리드에 각각 따로 저장하고, 평면을 바꾸면 그 평면의 버전이 자동으로 적용되게 한다.
- **문제:**
  - 오션 그리드에 맞춘 큰 파도(파장 3~5, 높이 0.1 이상)는 2×2 벤치 평면에서 천처럼 접힌다. 가파른 면 일부가 하늘을 반사해 하얗게 보인다(preset-themes NOTES).
  - 반대로 벤치에 맞추면 오션에서 파도가 너무 작다. 프리셋 하나로 두 평면을 모두 만족시킬 수 없다.
- **시각 목표 키워드:** 평면 크기에 맞는 파도
- **스코프 가드 — 절대로 만들지 않을 것:**
  - 프리셋 개수(3테마) 변경 없음. 테마 × 평면 = 6개
  - 셰이더 변경 없음
  - 프리셋 값 자체의 튜닝은 이 feature에서 하지 않음 (`preset-themes`에서 함)

## 2. 접근법

**저장 범위:** 프리셋 전체(하늘·조명·물·파도·노멀맵)를 평면별로 따로 저장한다. 사용자 결정(2026-10-04).

**동작:**

- 프리셋 버튼(Basic/Sunset/Tropical)은 **현재 평면**의 버전을 적용한다. 마지막으로 누른 테마를 기억한다.
- Save Current는 **현재 평면**의 해당 테마 슬롯에 저장한다.
- Ocean Grid 체크박스를 바꾸면 기억해 둔 테마의 새 평면 버전을 자동으로 적용한다. 저장하지 않은 조정값은 사라진다.
  - 카메라 슬롯 Load, 고정 샷 버튼처럼 평면이 바뀌는 다른 경로도 같다.
- 캡처: 샷마다 그 샷 평면의 버전으로 찍는다(oblique/top/sunward = bench, ocean_* = ocean).
- View 창 Presets에 현재 평면(bench / ocean)을 표시한다.

**파일 포맷 v4** (`assets/shader_presets.txt`):

```
WaterShaderPresets 4
bench <v3와 같은 한 줄>     # Basic
bench ...                   # Sunset
bench ...                   # Tropical
ocean ...                   # Basic
ocean ...                   # Sunset
ocean ...                   # Tropical
```

- 줄 앞 토큰으로 평면을 표시한다. 같은 평면 안에서는 테마 순서대로다.
- v3 파일을 읽으면 같은 값을 두 평면 모두에 넣는다. 지금 렌더 결과가 그대로 유지된다.
- 저장은 항상 v4로 한다.

## 3. Inputs / Outputs

| 종류 | 이름 | 형식 | 범위 / 비고 |
|---|---|---|---|
| Preset | `bench` / `ocean` 줄 3개씩 | text | v4. v3는 읽기만 (두 평면에 복제) |
| ImGui | Presets 섹션 | button | 현재 평면 표시, 버튼·Save는 현재 평면 대상 |
| ImGui | Ocean Grid | checkbox | 바뀌면 현재 테마의 해당 평면 버전 적용 |
| Capture | 샷별 평면 | — | `CaptureShot::ocean`으로 버전 선택 |

**의존하는 다른 feature:** `bench-tools`(프리셋·카메라 슬롯·캡처), `preset-themes`(값 튜닝)

## 4. Acceptance Criteria / Test Plan

- [ ] Debug/Release 빌드 경고 0
- [ ] v3 파일 로드 회귀: 캡처 18장이 수정 전과 픽셀 차이 0
- [ ] v4 저장 → 다시 로드 왕복 시 값 동일 (캡처 픽셀 차이 0)
- [ ] 평면 전환 시 해당 평면 버전 적용, Save Current는 현재 평면 슬롯만 바꿈
- [ ] 캡처가 샷의 평면 버전으로 찍힘
- [ ] NOTES
