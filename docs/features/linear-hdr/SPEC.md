# linear-hdr

> Branch: `feature/linear-hdr` (from `feature/bench-tools`) · Status: in-progress · Updated: 2026-10-03

## 1. Goal

- **한 문장 요약:** 감마(sRGB) 공간에서 하던 빛 계산을 **sRGB 입력 → linear 계산 → HDR 누적 → 톤매핑 → sRGB 출력**의 정석 파이프라인으로 바꾼다.
- **회고 포인트:** 단계마다 before/after 캡처와 "문제 원인 → 해결 방식 → 과정 → 검증 → 교훈"을 `NOTES.md`에 남긴다.
- **스코프 가드 — 만들지 않을 것:** HDR 큐브맵(.hdr/BC6H), 블룸, 자동 노출(다음 후보로 기록만).

## 2. 현재 상태 (문제)

| 위치 | 지금 | 문제 |
|---|---|---|
| 백버퍼 | `R8G8B8A8_UNORM`, 인코딩 없음 | 1.0 초과 값이 잘림 → 물 셰이더 안 `HighlightRolloff`로 임시 처리 |
| 큐브맵 (skybox, env_*) | UNORM / BC7_UNORM 그대로 샘플 | sRGB로 인코딩된 값을 linear처럼 사용 |
| 색 파라미터 (ImGui/프리셋) | sRGB로 고른 값을 그대로 cbuffer로 | 위와 동일 |
| 셰이더 | 변환 없음 | `ambient + diffuse`, `lerp(body, sky, F)`가 물리적으로 틀림 |

## 3. 접근

| 단계 | 내용 |
|---|---|
| 0-a | 무인 실행 모드: `--capture` / `--no-input`이면 포커스를 빼앗지 않고 키보드·마우스 입력 무시 |
| 0 | before 캡처 |
| 1 | `R16G16B16A16_FLOAT` HDR 타깃, 큐브맵 `_SRGB` SRV(`DDS_LOADER_FORCE_SRGB`), 색 파라미터 sRGB→linear 업로드, 최종 패스에서 clamp + `LinearToSrgb` (톤매핑 없음) |
| 2 | 노출 + 톤매핑(None / Reinhard / ACES), `HighlightRolloff` 제거 |
| 3 | linear 기준 프리셋 재튜닝 |

- 노멀맵은 데이터 텍스처라 UNORM 유지.
- 출력 sRGB 인코딩은 `_SRGB` RTV가 아니라 셰이더에서 — ImGui가 sRGB 값 그대로 같은 백버퍼에 그려지기 때문.
- 디버그 뷰(Mode ≠ 0)는 톤매핑·인코딩을 건너뛰는 패스스루.

## 4. Acceptance Criteria

- [ ] 무인 모드에서 포커스 유지 + 입력 무시 확인
- [ ] Step 1: 스카이박스가 before와 거의 동일(sRGB 왕복 오차 측정)
- [ ] 단계별 캡처 + 비교 시트 + NOTES (문제/해결/과정/검증/교훈)
- [ ] 디버그 뷰 1~5가 before와 동일
- [ ] 리사이즈 시 HDR 타깃 재생성, Debug/Release 경고 0
- [ ] 톤매핑 패스 GPU 비용 기록
