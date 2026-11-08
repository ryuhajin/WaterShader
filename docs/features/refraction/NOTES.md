# Refraction — NOTES

> 8단계 진행 기록. 상태: **에이전트 검증 완료, 사용자 승인 대기** (방식 변경 후)

## 1. 개념 요약

물 패스는 물 아래를 "바로 뒤 픽셀"이 아니라 **옮긴 픽셀**에서 읽는다. 옮기는 방법은 두 가지다.

```text
Normal offset (기본, 언리얼 "Pixel Normal Offset" 계열)
  물속 표면을 세계에서 옆으로 민다: 이동(m) = −(수면 노멀의 수평 기울기) × (1 − 1/1.33) × Wobble depth × Distortion × 물가 감쇠
  → 밀린 점을 화면에 투영. 미터 단위라 가까운 물일수록 픽셀 이동이 큼 (원근)
  평평한 물은 이동 0, 잔물결·파도만큼만 일렁임. 착지점이 화면 끝 32px에 들어오면 이동을 서서히 줄임

Physical (비교용)
  꺾인 방향 r = refract(시선, 수면 노멀, 1 / 1.33) → 화면에 투영 → 깊이로 물속 표면 → 거리 갱신 (4회)
  평평한 물도 꺾임: 깊은 바닥이 얕아 보이고 수면선이 꺾임. 대신 낮은 시점에서 화면 밖 정보가 필요

공통: 착지한 표면이 수면 위(전경 누출)거나 화면 밖이면 ½, ¼로 다시 → 그래도 실패면 원래 픽셀
      고른 픽셀 하나로 장면 색, 표면 위치, 수심, 경로, 투과율, 물색, 거품을 모두 다시 계산
```

- 굴절은 수면 노멀이 필요해서, 노멀 계산을 픽셀 셰이더 맨 앞으로 옮겼다 (기존 바다 프로그램은 문장 순서가 같아 그대로).
- 언리얼의 Pixel Normal Offset은 "픽셀 노멀 − **정점** 노멀"을 쓴다. 여기서는 "픽셀 노멀 − **평평한 물(위쪽)**"을 써서 Gerstner 파도도 일렁임에 들어간다 (언리얼 포럼에서도 큰 물 메시는 쓸 만한 정점 노멀이 없다는 지적이 있다).

## 2. 만든 것

| 파일 | 내용 |
|---|---|
| `shaders/PixelShader.hlsl` | 노멀 계산을 앞으로. `SCENE_INPUTS`: 두 착지 방식(Normal offset / Physical), 공통 재시도(1 → ½ → ¼)·실패 판정(수면 위 표면, 화면 밖)·수신 정보 갱신. `FrameInside`, `REFRACTION_STEPS`(4). 디버그 26(이동량), 27(결과), 28(시선 오차, Physical만). Pass-through(투과 모드 1)에는 굴절 없음 |
| `shaders/Common.hlsli`, `src/ColorShader.*` | `g_RefractionParams` (모드별 세기, 굴절률 1.33, 화면 끝 여유 32px Normal offset / 64px Physical, 모드), `g_RefractionParams2` (물가 감쇠 수심, Wobble depth). `WaterParams`: `refractionMode`, `refractionDistortion`, `refractionShoreFade`, `refractionWobbleDepth`, `refractionStrength`, `refractionIndex` |
| `src/Graphics.*` | Water 창 New: Underwater의 **Refraction** 콤보(Normal offset / Physical) + 모드별 슬라이더, 프리셋 키 5개(없으면 Normal offset, Distortion 1, 물가 0.15m, Wobble depth 2m, Physical 세기 1), CLI `--refraction`(두 모드 세기), `--refraction-mode normal\|physical`, Debug Mode 26~28 |
| `tools/diff_sets.ps1` | 마스크 없이 부를 때의 경로 오류 수정 |

### Physical 모드를 남기는 방법 (사용자 결정 2026-11-09)

- **UI:** Water 창 New: Underwater → **Refraction** 콤보에서 고른다. 모드마다 자기 슬라이더만 보인다: Normal offset = Distortion, Shore fade, Wobble depth / Physical = Refraction strength.
- **값은 따로:** 두 모드의 세기를 따로 저장해서, 모드를 바꿔 비교해도 각자 맞춰 둔 값이 남는다 (프리셋 키 `refractionDistortion`, `refractionStrength`, 모드는 `refractionMode`).
- **캡처·비교:** `--refraction-mode physical`로 같은 샷을 찍어 두 방식을 나란히 둔다. 디버그 26·27은 두 모드 공통, 28(시선 오차)은 Physical에만 의미가 있다.
- **13C(언리얼 대응):** 언리얼도 Index of Refraction / Pixel Normal Offset 두 모드를 둔다. 같은 이름으로 1:1 비교한다.

## 3. 에이전트 검증 (2026-11-09)

| 항목 | 방법 | 결과 |
|---|---|---|
| 빌드 | Debug·Release | 경고·오류 0 |
| 기존 장면 회귀 | 기존 샷 21장 vs 0단계 기준 | **21장 바이트 동일** |
| 세기 0 = 7단계 | 7단계 빌드의 `before_step7` vs `--refraction 0`, 6샷 × 3프리셋 | 17장 동일, 1장(tropical lab_low) 10픽셀 1레벨 (화면 고정 이동 버전에서는 18장 동일. 코드 변화에 따른 컴파일 차이) |
| Pass-through = 물 끔 | `--transmission 1 --reflection off` vs `--water off` | 6장 동일 |
| 전경 누출 0 | 굴절 켠 숫자 수심(12)에서 "물 위 표면" 표지 수 | 두 모드 모두 **6샷 0** |
| 결과 분포 (Normal offset) | 디버그 27, 물 픽셀 비율 | 성공 100%, 화면 밖 0%, 전경 누출 → 원래 0~0.01% |
| 이동량 (Normal offset) | 디버그 26 평균 (720p) | 안쪽 0.5~4.4px (위에서 내려다보는 lab_top이 가장 작음: 원근), 화면 끝 4px 안 0~1.2px |
| 가까운 쪽 이동 (front_rails 1080p 줄별) | 디버그 26 | 화면 고정 이동: 먼 쪽 9.3px / 가까운 쪽 3px / 맨 아래 1.4px → 세계 이동: 8.1 / 4.4~8.2 / 8.0px |
| Physical 모드 유지 | 변경 전 Physical 1080p 캡처 vs `--refraction-mode physical` | 3장이 1~3픽셀 차이 (코드 재배치. lab_stairs의 1픽셀은 100레벨: 재시도가 갈리는 경계 픽셀로 추정, 원인 미확정) |
| 디버그 레이어 | Debug: Normal offset 6샷 / Physical 큰 파도 2샷 | 경고·오류 0건 |

**Physical 모드 기록** (방식 변경 전 측정, 같은 셰이더 경로)

| 샷 | 성공 | ½·¼로 성공 | 화면 밖 → 원래 (화면 끝 처리 전 → 후) |
|---|---|---|---|
| lab_low | 99.6% | 0.4% | 20.7% → 0% |
| lab_pillar | 99.8% | 0.2% | 6.1% → 0% |
| front_rails | 100% | 0% | 3.1% → 0% |

- 시선 오차(디버그 28, Physical): 거리 갱신 2회 → 4회로 lab_stairs 평균 3.0cm → 1.6cm.

**캡처 위치 (로컬):** `C:\Users\da171\ws-capture\refraction\captures\`

| 폴더 | 내용 |
|---|---|
| `before_step7*` | 굴절 전 (프리셋 파도 6샷 × 3프리셋, 평평한 물, 큰 파도) |
| `after_step8*` | **Normal offset** (기본, 세계 이동): 프리셋 파도, 평평한 물, 큰 파도, Distortion 2, 1080p 3샷 |
| `normal_offset_screen_constant/` | 화면 고정 이동 버전 (가까운 쪽이 굴절 없어 보이던 1080p 렌더·이동량 + 사용자 보고 화면 `user_report_3_near_water_looks_unrefracted.webp`, 아래 "과정에서 고친 것") |
| `dbg_offset`, `dbg_offset_1080p`, `dbg_result`, `mask`, `dbg_layer*` | Normal offset 디버그 |
| `physical_*` | **Physical** 모드 같은 세트 (방식 변경 전 캡처) |
| `physical_edge_issue/` | 방식을 바꾸게 된 화면 끝 문제 기록 (아래 4절) |

회귀·동일성 캡처는 임시 폴더에서 비교하고 지웠다.

## 4. 방식 변경: 물리 굴절의 화면 끝 문제 (사용자 검증, 2026-11-09)

**증상과 원인**

1. 첫 구현(Physical): 착지점이 화면 밖이면 세기를 ½ → ¼ → 0으로 낮춤 → 화면 좌우 아래에 **계단식 띠** (사용자 보고 1).
2. 고친 구현: 착지점 거리를 화면 끝 64px 안에서 부드럽게 압축 → 띠는 줄었지만, 넓은 영역의 무늬가 가장자리 띠 안으로 **몰려 보임** (사용자 보고 2, 이동량 디버그의 올리브색 가장자리).
3. 근본 원인: 물리적으로 꺾인 시선은 평평한 수면에서도 아래로 꺾여, 낮은 시점·화면 아래쪽에서는 **보여 줘야 할 물속이 화면 밖**이다. 화면 밖 정보는 없으니 "포기"하든 "눌러 담든" 흔적이 남는다.

**조사 (실무 방식)**

| 방식 | 출처 |
|---|---|
| 물리 굴절(굴절률)은 장면 색 텍스처 밖을 읽게 되어 넓은 물에 맞지 않음 → **Pixel Normal Offset** 권장 | Epic 문서 (Pixel Normal Offset, Using Refraction) |
| 화면 끝 몇 %에서 반사·굴절을 서서히 줄임 | Blender EEVEE, Godot 물 셰이더 |
| 굴절 이동을 수심으로 줄여 물가에서 0 | OpenMW 물 셰이더 |
| 화면 공간 실패 시 반사 프로브 → 하늘로 단계별 대체 | Unity HDRP 반사·굴절 계층 |

**결정:** Normal offset을 기본으로 + 화면 끝 페이드 + 물가 감쇠. Physical은 비교 모드로 유지. 잃는 것: 평평한 물의 겉보기 깊이(깊은 바닥 압축, 파도 없는 수면선 꺾임). 남는 것: 잔물결·파도에 따른 일렁임 — 레퍼런스의 스타일라이즈드 물에 더 가깝다.

**결과:** 화면 밖 → 원래 픽셀 0%, 화면 끝 4px 안 이동 0~1.2px. 가장자리 몰림·계단 띠 없음. 이어서 가까운 물이 굴절 없어 보이던 문제를 세계 이동으로 고침 ("과정에서 고친 것") (`after_step8_front_1080p` vs `physical_edge_issue/`).

**참고 화면**

| 파일 | 내용 |
|---|---|
| `physical_edge_issue/user_report_1_stepped_retry_render.webp` | 사용자 보고 1: 계단식 재시도의 좌우 아래 띠 (Waterfront) |
| `physical_edge_issue/user_report_2_soft_compress_offset_debug.webp` | 사용자 보고 2: 부드러운 압축 후 이동량 디버그 (가장자리 올리브색 = 눌러 담은 구역) |
| `physical_edge_issue/physical_soft_render/` | 부드러운 압축 Physical, 1080p 렌더 (front_rails, lab_low, lab_stairs) |
| `physical_edge_issue/physical_soft_offset/` | 같은 화면의 이동량 디버그 (26) |
| `physical_edge_issue/physical_soft_result/` | 같은 화면의 결과 디버그 (27) |
| `after_step8_front_1080p/`, `dbg_offset_1080p/` | 변경 후 Normal offset, 같은 3샷 1080p |

### 과정에서 고친 것

| 증상 | 원인 | 해결 |
|---|---|---|
| 낮은 시점(front_rails)에서 물의 절반가량이 "전경 누출"로 판정되어 굴절이 꺼짐 | 찾은 픽셀의 장면 깊이가 이 물 조각보다 앞이면 전경으로 봤는데, 카메라 가까운 물속 바닥은 먼 수면보다 깊이 값이 작은 게 정상 | 전경 누출 = "찾은 표면이 그 자리 수면보다 위". 물 밖에 솟은 물체만 걸러짐 |
| 화면 아래쪽 넓은 띠가 굴절 없이 남음 (Physical) | 화면 밖이면 바로 원래 픽셀 | 세기 ½, ¼ 재시도 → 계단식 띠로 바뀜 (다음 줄) |
| 화면 좌우 아래에 계단식 띠, 무늬 몰림 (사용자 보고 1·2) | 4절 | 부드러운 압축(64px) → 몰림은 남음 → **Normal offset 방식으로 기본 변경**, Physical은 비교 모드 |
| Normal offset으로 바꾼 뒤 철로 장면 아래쪽(카메라 가까운 물)이 굴절 없는 것처럼 보임 (사용자 보고) | 이동을 "기울기 × 64px"로 **화면 픽셀에 고정**했다. 가까운 물은 크게 보여(자갈 하나가 수십 px) 같은 3px 이동이 거의 안 보이고, 먼 물은 같은 이동이 크게 보임. 실제 굴절은 반대(세계에서 밀린 거리를 투영하니 가까울수록 픽셀 이동이 큼). 맨 아래는 화면 끝 페이드(64px)까지 겹쳐 1.4px | 이동을 **세계 공간**에서 계산해 투영. 첫 시도는 실제 수심에 비례시켰는데, 철로 둑은 수심 0.08~0.35m라 오히려 1px로 줄었다 (물리적으로는 맞음: 얕은 물은 바닥이 덜 밀림). 레퍼런스의 얕은 철로는 강하게 일렁이므로 **Wobble depth(기본 2m)를 어디서나 같게** 쓰는 스타일라이즈드 선택. 화면 끝 여유도 32px로 줄임. 가까운 쪽 4~8px |
| 시선 오차가 큼 (Physical, lab_stairs 평균 3cm) | 거리 갱신 2회로는 깊은 바닥에서 덜 수렴 | 4회 (`REFRACTION_STEPS`) |
| 4개 텍셀 Gather로 선형 보간 여부를 판정하던 첫 구현 | 판정 기준을 "수면 위 표면"으로 바꾸면 4개 모두 높이 역산이 필요해 비쌈 | 가장 가까운 텍셀을 그대로 읽음 (Load) |

### 관찰

- **Physical의 깊은 물:** 8m 바닥이 크게 압축되어 보이고(겉보기 깊이), 잔물결 기울기 × 긴 물속 거리 때문에 깊은 곳 무늬가 날카롭게 조각난다. 물리적으로는 맞는 방향이지만 화면 공간에서는 정보가 모자라 거칠다.
- **Normal offset의 세기:** 세계 이동이라 위에서 멀리 내려다보는 화면(lab_top, 카메라 높이 20m)은 일렁임이 작고, 가까운 물은 크다. 더 강한 일렁임은 Distortion 2 (`after_step8_distortion2`) 또는 Wobble depth를 올린다.
- **화면 공간의 한계:** 화면 밖, 다른 물체 뒤에 가려진 물속은 알 수 없다. Normal offset은 이동이 작아 이 한계에 거의 닿지 않는다.

## 5. 사용자 검증 안내

**실행:** `.\build\vs2022\Debug\WaterShader.exe` → `1` 키 → Scene = **Water Lab** → `3` 키 Water 창 New: Underwater의 **Refraction** (기본 Normal offset)

| 순서 | 조작 | 정상 | 실패 징후 |
|---|---|---|---|
| 1 | 카메라 **lab_stairs**, Distortion 0 ↔ 1 ↔ 3 | 물속 계단·체크무늬가 잔물결에 일렁이고, 클수록 강해짐 | 0과 1이 같음 |
| 2 | View 창 Lab Waves 끄기 | 일렁임은 잔물결만큼 줄어듦 (평평한 물 자체는 이동 0) | — |
| 3 | Scene = **Waterfront**, front_rails, 창을 넓게 | 화면 좌우 아래 가장자리로 무늬가 몰리거나 줄진 띠 없음 | 가장자리 띠 |
| 4 | Debug Mode **Refraction offset** | 화면 전체에 작은 이동이 고르게, 가장자리는 0으로 | 가장자리 올리브색 넓은 띠 |
| 5 | Shore fade 0 ↔ 0.5, Wobble depth 0.5 ↔ 4 | Shore fade는 물체가 물에 닿는 곳 근처만, Wobble depth는 화면 전체의 일렁임 크기를 바꿈 | — |
| 5-1 | front_rails에서 화면 아래쪽(카메라 가까운 물) | 가까운 침목·자갈도 일렁임 (먼 물보다 오히려 크게) | 가까운 쪽만 굴절이 없어 보임 |
| 6 | 수면 위 계단과 기둥 근처 수면 | 물 밖 부분이 물속에 끌려 들어가지 않음 | 수면 아래에 물 밖 계단 조각 |
| 7 | Refraction → **Physical** | 깊은 바닥이 얕아 보이고 수면선이 꺾임, 낮은 시점 가장자리에 압축 흔적 (비교용) | — |

**확인 질문**

- [ ] 잔물결·파도에 따라 물속 체크무늬가 일렁인다
- [ ] 수면 위 계단·기둥이 물속으로 끌려 들어가 비치지 않는다
- [ ] 화면 공간 굴절의 한계와, 물리 굴절 대신 노멀 차이 방식을 고른 이유를 설명할 수 있다
