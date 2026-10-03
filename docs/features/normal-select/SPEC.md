# normal-select

> Branch: `feature/normal-select` (from `feature/basic-sky`) · Status: in progress · Updated: 2026-10-03

## 1. Goal

- **한 문장 요약:** 물 노멀맵을 여러 장 중에서 고를 수 있게 하고, **레이어 A(큰 물결)·레이어 B(잔물결)를 각각 다른 노멀맵**으로 지정해 프리셋마다 저장한다(예: Sunset = normal1, Tropical = normal3).
- **현재 상태:** 노멀맵은 `water_normal.dds` 한 장만 로드되고, 두 레이어가 **같은 텍스처**를 타일링(`normalScale` vs `× detailScale`)과 스크롤 방향만 다르게 해서 샘플링한다. 저장소에는 이미 노멀맵 JPG 5장(`water_normal`, `water_normal1~4`)이 있지만 쓰이지 않는다.
- **회고 포인트:** 같은 텍스처를 두 번 겹치면 생기는 반복 패턴, 레이어마다 성격이 다른 노멀(큰 물결 vs 잔물결)을 쓰면 뭐가 달라지는지 before/after로 기록.
- **스코프 가드:** 새 노멀맵 다운로드, 노멀맵 생성/편집, 플로우맵.

## 2. 접근

| 단계 | 내용 |
|---|---|
| 0 | before 캡처 |
| 1 | JPG 4장을 `water_normal.dds`와 같은 형식(RGBA8 UNORM + 밉, `--ignore-srgb`)으로 변환. 평균 RGB ≈ (128,128,255)로 감마 버그 재발 없음을 확인 |
| 2 | `kNormalMaps` 표 + 레이어별 선택 UI(콤보 2개), 셰이더에 t2(레이어 B) 추가, 프리셋 키 `normalMapA`/`normalMapB` |
| 3 | 노멀맵별 비교 캡처 → 프리셋마다 조합 선택·저장, after 캡처 |

## 3. Acceptance Criteria

- [ ] 노멀맵 5장 DDS(밉 포함, 평균 RGB 확인)
- [ ] 레이어 A/B 노멀맵을 따로 선택, 프리셋 저장/불러오기에 포함(키가 없는 옛 프리셋은 기존과 동일하게 0/0)
- [ ] 노멀맵 비교 캡처 + before/after + NOTES
- [ ] Debug/Release 경고 0
