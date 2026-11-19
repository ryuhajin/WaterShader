# Contact Foam Polish — NOTES

> 6B++ 진행 기록. 상태: **구현·에이전트 검증 완료, 사용자 검증 대기** (2026-11-20)

## 1. 왜 다시 손댔나

- 데브로그 04를 쓰면서 남은 문제를 다시 봤다. 거품이 벽에 칠한 띠처럼 보이고, 직각 모서리 바깥과 기둥 옆·뒤 수면에는 거품이 없다.
- 원인: 거품 자리가 "물 픽셀 뒤에 보이는 면"의 수심으로 정해진다. 수면 위 각 점에서 물체까지의 수평 거리는 쓰지 않는다.
- 레퍼런스(https://fire-face.com/water/)도 같은 깊이 기반이다. 직각 모서리를 "주된 단점"으로 받아들이고, 대신 아래 세 가지로 거품을 다듬었다. 사용자는 이번에 다듬기만 하고, 거리장(고리)은 나중에 하기로 했다(2026-11-19).

## 2. 두 측정 방식의 차이

| | 수직 수심 (기존, 기본) | 시선 경로 (레퍼런스: 장면 깊이 − 수면 깊이) |
|---|---|---|
| 재는 것 | 뒤 표면이 수면에서 수직으로 몇 m 아래인지 | 시선이 수면에 들어가 뒤 표면에 닿기까지의 거리 |
| 카메라 | 받는 면 기준 폭이 늘 0~폭 m | 비스듬할수록 같은 수심을 길게 재서 띠가 줄어듦 |
| 얕은 평평한 면 | 기울기 필터로 거름 | 위에서 보면 덮이고 비스듬하면 사라짐 |
| 모서리·고리 | 없음 | 없음 |

## 3. 만든 것

| 항목 | 계산 | 기본값 |
|---|---|---|
| Edge bias | 가까움 = saturate(1 − d ÷ 폭) + 바이어스, 0~1로 자름. 띠가 꽉 차고 폭에서 끊김 | 0.2 |
| Leading edge | d < 폭 × L에서 lead = lerp(1, d ÷ (폭 × L), 기울기 마스크). 최종 색 = lerp(장면 색, 물+거품, lead) | 0.15 |
| Foam rides the waves | 무늬 좌표 = 변위 전 위치(`restPosLocal` → 월드) | 켬 |
| Depth measure | d = 수직 수심 / 시선 경로 (비교용, 저장 안 함) | 수직 |

| 파일 | 내용 |
|---|---|
| `shaders/Common.hlsli` | `g_FoamParams3` (바이어스, leading, ride, 측정) |
| `shaders/PixelShader.hlsl` | `foamDepth` 선택, 바이어스, 무늬 좌표, leading edge(최종 합성 두 곳), Debug **30** = lead |
| `src/ColorShader.*` | WaterParams 4개, cbuffer |
| `src/Graphics.*` | Water 창 Contact Foam에 슬라이더 2, 체크박스, 측정 목록. 프리셋 키 `foamEdgeBias`·`foamLeadingEdge`·`foamRidesWaves`(없으면 기본값). CLI `--foam-bias`, `--foam-leading`, `--foam-ride on\|off`, `--foam-measure vertical\|view` |

- 폼 마스크(05)와 옛 구름 노이즈는 둘 다 `band`를 쓰므로 그대로 함께 동작한다(`after_current`).
- 코스틱은 바닥 셰이더에 있어서 무관하다.

## 4. 에이전트 검증 (2026-11-20)

| 항목 | 결과 |
|---|---|
| 빌드 | Debug·Release 경고·오류 0 |
| 기존 바다 회귀 | 21장 바이트 동일 |
| 새 항목 끔 = 이전 | 6샷 × 3프리셋 **18장 바이트 동일** (기본값을 켠 뒤에도 `--foam-bias 0 --foam-leading 0 --foam-ride off`로 다시 확인) |
| 바이어스 0 → 0.2 | 거품 픽셀 slot3 12,003 → 13,745 (꽉 찬 비율 92 → 98%), stair_pillar_near 5,038 → 7,329 (82 → 93%), rail_line 55,789 → 64,997 |
| leading edge 0.15 | 기둥 접점, 열마다 가장 큰 한 픽셀 밝기 차이 평균 163 → 73레벨 |
| 측정 방식 | 거품 픽셀(수직 / 시선): stair_angle 3,766 / 1,947, 낮은 시점 14,759 / 3,670. 기울기 필터 0이면 낮은 시점 22,723 / 3,677 (얕은 면이 시선 방식에서 사라짐) |
| 파도 따라가기 | 12초 켬/끔 차이 1,332 px(slot3), 2,677 px(stair_pillar_near). 정지 화면에선 미세 |
| 디버그 레이어 | 경고·오류 0 |
| 성능 | 측정하지 않음 (요청 시만) |

캡처와 데브로그 추천 컷: `C:\Users\da171\ws-capture\contact-foam-polish\captures\` (README.md에 폴더·플래그·수치 정리).

### 과정에서 고친 것

- **leading edge가 평평한 얕은 면 물까지 흐림:** 데브로그 받침대 샷(Debug 30)에서 발견. 처음에는 lead가 수심만 봤다. 그래서 leading 0.3(폭 × 0.3 = 7.5 cm)에서 −5 cm 받침대의 평평한 윗면 물 전체가 장면 색 쪽으로 흐려졌다. 지금은 lead = lerp(1, 수심 ÷ (폭 × L), 기울기 마스크)로, 띠와 같은 기울기 필터를 따른다. 꺼짐 = 이전(18장)과 회귀(21장)는 고친 뒤 다시 확인했다.

### 데브로그용 받침대 비교

데브로그 04의 기존 받침대 샷과 같은 카메라로, 바이어스와 leading edge를 하나씩 바꿔 찍었다. 각 변형마다 화면, 거품 양(Debug 20), leading 값(Debug 30) 세 가지를 남겼다. 위치는 `ws-capture/contact-foam-polish/captures/devlog_pads/`이고, 시트와 설명은 README에 있다.

## 5. 남는 한계

- 직각 모서리 바깥과 기둥 옆·뒤의 수면 고리. 깊이 기반이라 "그 물 픽셀 뒤에 보이는 면"만 안다. 위에서 본 거리장(점프 플러딩)으로 해결할 예정이며, 언리얼에서는 `DistanceToNearestSurface`에 대응한다.
- leading edge는 기울기 필터를 따르므로, 필터를 0으로 내리면 얕은 평평한 면의 접점(파도 골에 드러난 곳)도 흐려진다(얕은 물가가 투명해지는 효과와 같음).

## 6. 사용자 검증 안내

**실행:** `.\build\vs2022\Debug\WaterShader.exe` → `1` 키 View 창 Scene = **Deep Stair** → `3` 키 Water 창 **Contact Foam**

- 04 룩으로 보려면 이 플래그를 붙여 실행한다: `--foam-map 0 --caustics off --shadows off --downwelling off`

| 순서 | 조작 | 정상 |
|---|---|---|
| 1 | 카메라 stair_pillar_near, Edge bias 0 ↔ 0.2 ↔ 0.6 | 띠가 꽉 차고 아래 끝이 단단해짐 |
| 2 | Leading edge 0 ↔ 0.15 ↔ 0.4 | 기둥과 물이 만나는 선이 부드러운 경사로 |
| 3 | Debug Mode **Contact foam leading edge** (30) | 접점에만 검은 띠 |
| 4 | Foam rides the waves 끔 ↔ 켬, 실행 중 관찰 | 켜면 무늬가 파도를 따라 앞뒤로 출렁임 |
| 5 | Depth measure 두 가지, 카메라를 낮췄다 올렸다 | 시선 경로는 카메라 각도에 따라 띠가 변하고, 수직 수심은 그대로 |
| 6 | 받침대 모서리, 기둥 둘레를 돌아보기 | 모서리 바깥 수면과 기둥 옆·뒤는 여전히 비어 있음 (남는 한계) |

**확인 질문**

- [ ] 접점 선이 부드러워지고 띠 두께를 조절할 수 있다
- [ ] 거품 무늬가 파도를 따라 출렁인다
- [ ] 두 측정 방식의 차이와, 모서리·고리가 남는 이유를 설명할 수 있다

## 7. 승인 기록

- (대기)
