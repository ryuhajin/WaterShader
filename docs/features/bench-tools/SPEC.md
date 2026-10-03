# bench-tools

> Branch: `feature/bench-tools` (from `feature/water-polish`) · Status: done · Updated: 2026-09-29

## 1. Goal

- **한 문장 요약:** 셰이더 벤치를 디버깅·촬영하기 편하게 만들고(마우스 회전, 카메라 프리셋, 성능 오버레이), 프리셋마다 다른 하늘을 쓰며, 남아 있던 버그 3건을 원인까지 정리한다.
- **스코프 가드 — 만들지 않을 것:** 자유 궤도 카메라(orbit camera), HDR(.hdr/.exr) 파이프라인, 런타임 equirect→cube GPU 변환.

## 2. 항목

| # | 항목 | 접근 |
|---|---|---|
| 1 | 마우스로 판 회전 | ImGui IO의 마우스 드래그 → `modelRotation_` (Ocean Grid에선 비활성, world = identity). X/Y/Z 슬라이더 제거 |
| 2 | 시작 구도 | `kCaptureShots`의 `sunward` 샷을 기본값/Reset 값으로 |
| 3 | 카메라 프리셋 | 고정 샷 버튼(캡처와 같은 정의) + 저장 슬롯 4개(`assets/camera_presets.txt`) |
| 4 | 프리셋별 cube map | Poly Haven CC0 tonemapped JPG → `tools/equirect_to_cube.ps1` → RGBA8 DDS cube. 프리셋 확장 키 `environment` |
| 5 | Sunset 물이 흙색 | 원인 수치 검증 후 환경맵 + 몸체색 수정 → `TROUBLESHOOTING.md` |
| 6 | 성능 오버레이 | 좌상단 접이식 "Stats": Time / CPU ms / GPU ms(timestamp query, 4프레임 링) / FPS, VSync 토글 |
| 7 | C4244 경고 | `WideToUtf8`/`Utf8ToWide` 헬퍼 → `TROUBLESHOOTING.md` |
| 8 | JPG 노멀맵 로드 실패 | COM 미초기화 가설 검증 → `CoInitializeEx` + `WIC_LOADER_IGNORE_SRGB` → `TROUBLESHOOTING.md` |

## 3. Acceptance Criteria

- [x] 빌드 경고 0 (Debug/Release)
- [x] 시작 시 sunward 구도, 좌클릭 드래그로 판 회전(Ocean Grid에선 안 됨) — 드래그는 사용자 수동 확인 필요
- [x] 카메라 고정 샷 5개 + 슬롯 저장/불러오기(재시작 후 유지)
- [x] 프리셋 적용 시 하늘(스카이박스 + 반사) 전환, 캡처에도 반영
- [x] Stats 오버레이 접기/펴기, VSync off에서 FPS/GPU ms 측정값 기록 (`NOTES.md`)
- [x] 5·7·8 문제/원인/해결/검증을 `TROUBLESHOOTING.md`에 기록 (+ 캡처 멈춤 A항목)
