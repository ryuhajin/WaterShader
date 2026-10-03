# hdr-env

> Branch: `feature/hdr-env` (from `feature/linear-hdr`) · Status: done · Updated: 2026-10-03

## 1. Goal

- **한 문장 요약:** LDR(이미 톤매핑된 JPG) 하늘을 **HDR 원본(.hdr, 장면 휘도 그대로)** 큐브맵으로 바꿔, 하늘·태양·물이 같은 물리 단위로 계산되고 톤매핑은 화면 끝에서 한 번만 일어나게 한다.
- **회고 포인트:** linear-hdr에서 남은 "LDR 사진의 이중 톤매핑" 문제(노출 피팅, 채널별 ACES의 하늘 표백)가 HDR 원본에서 어떻게 사라지는지, 단계별 before/after와 수치로 기록.
- **스코프 가드:** 블룸, 자동 노출, IBL 확산(irradiance 컨볼루션)·프리필터 스펙큘러.

## 2. 접근

| 단계 | 내용 |
|---|---|
| 0 | Capture Set 버튼 안전장치(확인 + 기존 폴더 덮어쓰기 방지), before 캡처 |
| 1 | `tools/equirect_to_cube.ps1`에 Radiance `.hdr`(RGBE, RLE) 입력 → float 큐브 → texconv `BC6H_UF16` + 밉. HDR 큐브맵은 `_SRGB` 없이(float = linear) 로드. 하늘마다 노출 기준값 측정 |
| 2 | HDR에서 **태양 방향·색·조도, 하늘 ambient**를 측정해 조명 파라미터를 하늘에 맞춤(이미지 기반 조명 보정), 글린트 강도 재정의, 프리셋 재튜닝 |

- 소스: Poly Haven CC0 4k `.hdr` — `grasslands_sunset`(Basic), `the_sky_is_on_fire`(Sunset), `spiaggia_di_mondello`(Tropical). 원본은 저장소에 넣지 않음.

## 3. Acceptance Criteria

- [x] Capture Set 버튼: 확인 단계 + 기존 폴더를 덮어쓰지 않음 (팝업은 수동 확인 필요)
- [x] HDR 변환 도구 + BC6H 큐브맵 3종, 로더가 HDR은 sRGB 변환 없이 읽음
- [x] 단계별 캡처 + 비교 시트 + NOTES (문제/해결/과정/검증/교훈)
- [x] 조명 보정값(태양·ambient)을 HDR에서 측정해 기록 + 태양 분리(중복 계산 제거)
- [x] Debug/Release 경고 0, 성능 기록
