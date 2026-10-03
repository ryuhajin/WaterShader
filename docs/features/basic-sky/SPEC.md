# basic-sky

> Branch: `feature/basic-sky` (from `feature/hdr-env`) · Status: in progress · Updated: 2026-10-03

## 1. Goal

- **한 문장 요약:** Basic 프리셋의 하늘을 노을(`grasslands_sunset`)에서 **한낮 들판 HDR(`belfast_farmhouse`)**로 바꿔, 바다 반사에 노을의 붉은·분홍빛이 섞여 나오지 않게 한다.
- **문제:** 기존 Basic 하늘은 해가 고도 3.3°에 걸린 노을이라, 측정한 태양색이 (0.42, 0.08, 0.01)로 거의 빨강이고 태양 주변 하늘도 분홍이다. 물은 하늘을 그대로 반사하므로, 노을을 보여주려는 Sunset 프리셋이 아닌 Basic에서도 수면이 붉게 물든다.
- **회고 포인트:** 하늘 하나를 바꾸면 무엇이 같이 바뀌어야 하는지(태양 방향, 태양·ambient 보정값, 노출, 몸체색, 글린트) — hdr-env에서 만든 측정 도구로 수치 그대로 연결되는지 검증.
- **스코프 가드:** LDR 버전은 만들지 않는다(사용자 결정). ImGui UI 정리는 별도 작업.

## 2. 접근

| 단계 | 내용 |
|---|---|
| 0 | before 캡처 (현재 Basic = HDR Meadow dusk) |
| 1 | 후보 선정: Poly Haven CC0에서 "한낮 + 탁 트인 들판" 필터 → 5장 → 태양 고도·하늘색 기준으로 `belfast_farmhouse`(약 23°) 선택 |
| 2 | `tools/equirect_to_cube.ps1`로 BC6H 큐브맵 변환 + 태양/하늘 측정 → `kEnvironments`에 추가, Basic 프리셋을 새 하늘로 + 보정값·노출·몸체색 재튜닝 |

- 원본 `.hdr`은 저장소에 넣지 않음(변환 결과 `assets/textures/env_field_day_hdr.dds`만).

## 3. Acceptance Criteria

- [ ] 새 HDR 큐브맵 + 측정값(태양 방향·색·조도, ambient, key EV) 기록
- [ ] Basic 프리셋이 새 하늘 사용, 반사에 붉은 기운 없음(수치로 확인)
- [ ] before/after 캡처 + 비교 시트 + NOTES (문제/해결/과정/검증/교훈)
- [ ] Debug/Release 경고 0
