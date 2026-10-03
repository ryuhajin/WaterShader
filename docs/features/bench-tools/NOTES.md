# bench-tools — 구현 메모

> 2026-09-29 · feature/bench-tools · 트러블슈팅(5·7·8번, 캡처 멈춤)은 `TROUBLESHOOTING.md`

## 조작 / UI

| 기능 | 방법 |
|---|---|
| 판 회전 | 뷰포트 좌클릭 드래그 (좌우 = yaw, 상하 = pitch, ±89°). Ocean Grid에선 판 회전 없음 + world = identity |
| 시점 회전 (마우스 룩) | Bench: 우클릭 드래그 / Ocean Grid: 좌·우클릭 드래그 (pitch ±89°). 화살표 키도 동일 |
| 시작 구도 | `sunward` 샷 (step2_sun_glint 구도). `Reset Camera`도 동일 |
| 카메라 프리셋 | Camera Presets: 고정 샷 5개(캡처와 같은 정의) + 슬롯 4개 Save/Load → `assets/camera_presets.txt` |
| 하늘 | Environment → `Sky / Reflection` 드롭다운. 셰이더 프리셋에 `environment`로 저장 |
| 성능 | 좌상단 `Stats` (타이틀 화살표로 접기). VSync 토글 |
| 카메라 이동 | WASD / Q·E / 화살표 — ImGui 버튼을 누른 뒤에도 바로 동작 (텍스트 입력 중에만 막힘) |

## 명령줄

```
WaterShader.exe [--shot <name>] [--no-vsync]
                [--capture <label>] [--capture-feature <feature>] [--debug <mode>]
                [--normal-map <path relative to assets>]
```

- 캡처 결과: `docs/features/<feature>/captures/<label>/` (기본 feature = water-polish)
- 비교 시트: `powershell -File docs/features/water-polish/make_compare.ps1 [-Dir ../<feature>] <step> <step> ...`

## 환경맵 (4번)

| 프리셋 | 파일 | 출처 (CC0) | 태양 |
|---|---|---|---|
| Basic | `skybox.dds` | 기존 | yaw 34.5, 고도 4 |
| Sunset | `env_sunset_fire.dds` | Poly Haven [The Sky Is On Fire](https://polyhaven.com/a/the_sky_is_on_fire) — Greg Zaal | yaw 34.5 (원본 103.8에서 −69.3° 회전), 고도 6.4 |
| Tropical | `env_beach_day.dds` | Poly Haven [Spiaggia di Mondello](https://polyhaven.com/a/spiaggia_di_mondello) — Andreas Mischok | yaw 236.4 (−160° 회전), 고도 25.3 |

- 원본: Poly Haven "Tonemapped JPG" 8192×4096 (LDR — 현재 감마 공간 파이프라인과 맞음). 원본 JPG는 저장소에 넣지 않음(위 링크).
- 변환: `tools/equirect_to_cube.ps1`
  ```
  powershell -File tools/equirect_to_cube.ps1 -In the_sky_is_on_fire.jpg -Out assets/textures/env_sunset_fire.dds -AlignSunYaw 34.5
  powershell -File tools/equirect_to_cube.ps1 -In spiaggia_di_mondello.jpg -Out assets/textures/env_beach_day.dds -RotateYaw -160
  ```
  equirect → 6면 1024² (2×2 슈퍼샘플 bilinear, D3D 면 규약은 skybox.dds와 동일) → RGBA8 DDS → texconv `-m 0 -f BC7_UNORM` (8 MB, 밉 포함). 가장 밝은 텍셀 가중 중심으로 태양 방향도 출력.
- **회전 기준:**
  - 노을 하늘은 태양을 yaw 34.5에 맞춤 → 고정 `sunward` 샷이 모든 프리셋에서 태양을 향함.
  - 해변은 태양이 나무 뒤(원본 yaw 36)이고 바다가 정반대(±180)라서, 태양을 맞추면 샷이 나무·건물을 바라봐 "물에 잠긴 공원"이 됨(`captures/env/tropical_*`). → 바다가 샷 쪽을 보게 −160° 회전, 태양은 카메라 뒤. 열대 바다 사진처럼 순광.

## 성능 (Release, 1280×720, VSync off)

| 샷 | FPS | CPU ms | GPU ms |
|---|---|---|---|
| sunward (bench 32² plane) | 6049 | 0.06 | 0.02 |
| ocean_sunward (1024² grid) | 4837 | 0.05 | 0.18 |
| ocean_wide (1024² grid) | 4588 | 0.05 | 0.19 |

- GPU 시간: `D3D11_QUERY_TIMESTAMP_DISJOINT` + 시작/끝 `TIMESTAMP`, 4프레임 링버퍼, `GetData(DONOTFLUSH)` → CPU가 GPU를 기다리지 않음. 표시값은 0.5초 평균.
- CPU 시간: `Render()` 시작 ~ Present 직전 (vsync 대기 제외).
- 1024² 격자(정점 약 105만)도 GPU 0.2 ms 수준 — 버텍스 셰이더 파도 4개 + 거리 LOD로 충분히 가벼움.

## 캡처 비교

`compare/*.jpg` — before(초원 하늘) → env(환경맵만) → final(환경맵 + Sunset 몸체색 + 해변 회전).
