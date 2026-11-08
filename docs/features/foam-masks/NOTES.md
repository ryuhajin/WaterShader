# Foam Masks — NOTES

> 6B+ 진행 기록. 상태: **에이전트 검증 완료, 사용자 승인 대기**

## 1. 개념 요약

```text
굽기 (한 번, 오프라인):  foam_bake.hlsl (컴퓨트 셰이더) → RWTexture2D (UAV) → PNG → texconv → DDS (밉, BC7)
실행:                    노멀맵처럼 DDS를 읽어 GPU로 → Water 창에서 골라 씀

마스크 채널:  R = 셀 거품망 (보로노이 F2 − F1)   G = 기포 (크기가 다른 원)   B = 침식 노이즈 (fbm, 0.5 = 중립)
합성:         문턱 = lerp(부드러움, 1 − 부드러움, 1 − R + (B − 0.5) × 침식)
              몸통 = smoothstep(문턱 ± 부드러움, 가까움 × 기울기)      // R이 밝은 셀 경계부터 거품이 생김
              기포 = smoothstep(0.45, 0.7, G) × 띠 안쪽 × 기포 세기    // 몸통 바깥에 흩어진 기포
```

- **이음매 없음:** 모든 노이즈를 주기 격자로 만든다. 보로노이는 셀 좌표를 셀 수로 나머지 연산(wrap)하고, fbm은 옥타브마다 주기가 정수 칸이다. 텍스처 오른쪽 끝의 이웃이 왼쪽 첫 칸이 된다.
- **재현:** 정수 해시와 고정 시드만 쓰고, 굽기 셰이더는 Debug·Release와 상관없이 같은 컴파일 옵션(IEEE 엄격, 최적화 3)으로 컴파일한다. 같은 시드로 다시 구우면 PNG가 바이트 동일하다.
- 기존 Clouds(0번)는 6B 코드 경로를 그대로 탄다 (거울 반복 읽기).
- 새 마스크는 wrap 샘플러로 그대로 반복하고, 돌리고 크기를 바꾼 두 번째 장을 다른 방향으로 흘려 섞는다(흐름 거품). R은 두 장 중 밝은 쪽, G는 둘 중 하나라도 기포면 기포, B는 첫 장만 쓴다(평균하면 저주파가 납작해짐).

## 2. 만든 것

| 파일 | 내용 |
|---|---|
| `shaders/foam_bake.hlsl` | `cs_5_0`, 8 × 8 스레드. 주기 보로노이(F1·F2·셀별 난수), 셀망, 기포, 주기 값 노이즈 fbm. 마스크 4종 |
| `src/FoamBaker.h/.cpp` | 컴퓨트 셰이더 컴파일 → UAV 텍스처 → `Dispatch(64, 64, 1)` → UAV 해제 → `SaveWICTextureToFile` PNG |
| `src/Graphics.*` | `--bake-foam-masks <폴더>` (굽고 종료, `bake_log.txt`에 결과·디버그 레이어 메시지). `kFoamMaps[]` 목록(파일, 이름, 이음매 없음, 기포, 침식), 모두 로드, 프리셋 키 `foamMap`·`foamBubbles`·`foamErosion`, CLI `--foam-map`, Water 창 Contact Foam에 **Foam texture** 콤보 + Bubbles·Erosion 슬라이더 (그 마스크에 없는 채널이면 회색 비활성) |
| `src/ColorShader.*`, `shaders/Common.hlsli` | `g_FoamMapParams` (이음매 없음, 기포 세기, 침식 세기). 매 프레임 고른 마스크에서 채움 |
| `shaders/PixelShader.hlsl` | `FoamMask`(두 장 흐름, wrap), 거품 블록에 새 경로. Clouds는 이전 그대로 |
| `tools/bake_foam_masks.ps1` | 굽기 → texconv(BC7, 밉, `--ignore-srgb`) → 이음매 검사 (+ `-PreviewDir` 2 × 2 확인 그림) |
| `assets/textures/foam_{cells,bubbles,packed,streaks}.{png,dds}` | 512 × 512 마스크 4종 |

**마스크 목록 (Water 창 Foam texture)**

| # | 이름 | R | G | B |
|---|---|---|---|---|
| 0 | Clouds (old, wave_foam) | 구름 노이즈 | — | — |
| 1 | Cells (foam network) | 셀망 (7칸 + 17칸) | 0 | 0.5 |
| 2 | Bubbles | 큰 기포 (6칸) | 작은 기포 (19칸 + 37칸) | 0.5 |
| 3 | **Cells + bubbles + breakup (기본)** | 셀망 | 기포 | fbm (3·6·12·24칸) |
| 4 | Streaks (stretched cells) | 가로 16 × 세로 5칸으로 늘인 셀망 | 기포 | fbm |

## 3. 에이전트 검증 (2026-11-09)

| 항목 | 방법 | 결과 |
|---|---|---|
| 빌드 | Debug·Release | 경고·오류 0 |
| 기존 장면 회귀 | 기존 샷 21장 vs 0단계 기준 | **21장 바이트 동일** |
| 굽기 재현 | 같은 시드로 두 번 굽기, Debug 굽기 vs Release 굽기 | 4장 모두 PNG 바이트 동일 |
| 이음매 | `bake_foam_masks.ps1`: 끝 열↔첫 열(끝 행↔첫 행) 차이를 안쪽 이웃 쌍들의 분포에서 순위로 | 4장 × 3채널 모두 **안쪽 최댓값 이하** (예: cells R 열 19.4, 안쪽 중앙값 11.9·최대 28.7). 2 × 2 확인 그림에서도 끊김 없음 |
| Clouds(0) = 8단계 | main(8단계)으로 찍은 6샷 × 3프리셋 vs `--foam-map 0` | 16장 동일, 1장 1픽셀 1레벨. tropical lab_low 10픽셀은 단독으로 다시 찍자 동일 (긴 세트 뒤쪽 샷의 흔들림, 6·7단계 NOTES와 같은 현상) |
| 거품 위치 | `compare_depth.ps1 -Mode foam`, 굴절 끔, 마스크 0·2·3·4 × 6샷 | 모두 가장 깊은 거품 0.245~0.249m (폭 0.25m), 폭 초과 0, 측정 안 된 곳(하늘) 0 |
| 디버그 레이어 | Debug: 굽기 모드, 일반 실행 2샷 | 경고·오류 0건 |

**거품 양** (물 픽셀 중 거품 비율, Basic, 기울기 필터 켬): Clouds 대비 새 마스크는 절반 정도로 줄었다 (예: front_rails 3.0% → 기본 마스크 2.1%, Streaks 1.5%). 띠가 꽉 찬 흰 면이 아니라 셀 경계를 따라 레이스처럼 끊기기 때문이다. 레일 옆면의 흰 선(6B 관찰)도 같이 옅어졌다.

**캡처 위치 (로컬):** `C:\Users\da171\ws-capture\foam-masks\captures\`

| 폴더 | 내용 |
|---|---|
| `compare_sheets/masks_<샷>.png` | 마스크 5종을 한 장에 (lab_pillar_near, lab_stairs, front_rails) |
| `compare_sheets/pillar_waterline_crops.png` | 기둥 수면선 부분만 확대해 5종 나란히 |
| `mask0` ~ `mask4` | 원본 캡처 (PNG) |
| `tiled_previews` | 마스크마다 R·G·B를 2 × 2로 이어 붙인 확인 그림 |
| `dbg_layer` | Debug 빌드 + `d3d_messages.txt` |

```powershell
.\tools\bake_foam_masks.ps1 -PreviewDir C:\Users\da171\ws-capture\foam-masks\captures\tiled_previews
```

### 과정에서 고친 것

| 증상 | 원인 | 해결 |
|---|---|---|
| PNG 저장 실패 (0x80004005) | `SaveWICTextureToFile`에 출력 픽셀 형식을 `32bppRGBA`로 지정하면 PNG 인코더가 받지 않음 | 형식을 지정하지 않고 ScreenGrab이 고르게 함 (8비트 그대로, BGRA) |
| 이음매 검사가 "끝 차이 19.4 vs 안쪽 평균 12.3"이라 이음매처럼 보임 | 얇은 셀 경계가 지나는 위치에 따라 열 쌍마다 차이가 0~29로 크게 달라, 평균 하나와 비교하면 오해 | 끝 쌍을 안쪽 쌍들의 분포에서 순위(중앙값·최댓값·몇 % 위)로 보고, 안쪽 최댓값을 넘을 때만 이음매 의심 |
| Water 창이 세로로 너무 길고, 대분류 글씨가 슬라이더 글씨와 같은 크기라 구분이 안 됨 (사용자 지적) | 6B·6B+·7·8단계 설정이 Water 창 하나에 쌓였고, 대분류가 모두 `SeparatorText`(일반 글씨 크기) | 아래 "UI 정리" |

### UI 정리 (사용자 요청, 렌더링 변화 없음)

| 바뀐 것 | 내용 |
|---|---|
| 대분류 헤더 | `SectionHeader()` 하나로 통일: 글씨 1.3배(ImGui 1.92 `PushFont(nullptr, 크기)`), 파란 막대, 눌러서 접기·펴기(`CollapsingHeader`). 대분류 안의 작은 구분(Light loss, Water color, 노멀맵 레이어 A/B)은 그대로 작은 글씨 |
| 처음 접힘 | Camera Presets, Capture, Top View Sun, Tonemapping, Ocean Detail (자주 안 쓰는 것). 접힌 상태는 실행 중에만 기억 |
| 물속 빛 → Light 창 | 빛 감쇠(색별 소멸 계수, Zero / Pure water)와 물색(모드·세기·색·램프)을 Light 창 **Underwater Light (test scenes)** 로 옮김 (`DrawUnderwaterLightSettings`). Clear / Green / Lagoon 버튼이 둘을 함께 바꾸므로 맨 위에 둠. Old 물에서는 안내 한 줄만 |
| Water 창에 남은 것 | Underwater (New water) (투과·반사 켬·가림·굴절·최대 거리), Contact Foam, Reflection. 끝에 "Light loss and water color: Light window (2)." |
| 이름 줄임 | 큰 글씨에서 잘리는 헤더: Sun Glint, Ambient, Top View Sun (괄호 설명은 헤더 아래 회색 줄로) |
| ID 충돌 고침 (사용자 발견) | View 창 Scene 목록을 고를 때 "2 visible items with conflicting ID" 오류. 대분류 헤더는 접히는 클릭 위젯이라 ImGui ID를 가지는데, 헤더 "Scene"과 그 안의 콤보 "Scene"이 같은 이름 → 같은 ID. `SectionHeader`가 헤더를 `PushID("section")` 범위 안에서 그려, 이름이 같은 위젯이 있어도 ID가 겹치지 않음. Debug 빌드에서 헤더·콤보에 마우스를 올려 오류가 안 뜨는 것 확인 |

| 검증 | 결과 |
|---|---|
| 빌드 | Debug·Release 경고 0 |
| 렌더링 불변 | 수정 전 빌드와 비교: 기존 바다 21장 + 테스트 장면 6샷 × 3프리셋 18장 **바이트 동일** |
| 디버그 레이어 | Debug 2샷 경고·오류 0건 |
| 화면 | `--ui all` / `light` / `water`로 띄운 창 화면 확인: 대분류가 크고 파란 막대, 접힘 화살표, 기본 접힘 5개 |

## 4. 사용자 검증 안내

**실행:** `.\build\vs2022\Debug\WaterShader.exe` → `1` 키 → Scene = **Water Lab** → `3` 키 Water 창 → **Contact Foam** 아래 **Foam texture**

| 순서 | 조작 | 정상 |
|---|---|---|
| 1 | 카메라 lab_pillar_near, Foam texture를 0 ↔ 3 | 0은 흰 띠, 3은 셀 경계를 따라 레이스처럼 끊긴 거품 + 바깥에 흩어진 작은 기포 |
| 2 | 1·2·4번도 차례로 | 셀망만 / 기포 위주 / 한 방향으로 늘어난 줄무늬 |
| 3 | Bubbles 0 ↔ 1, Erosion 0 ↔ 1 (3번 마스크) | 기포가 사라지고 생김 / 띠가 꽉 찬 모양 ↔ 섬처럼 끊긴 모양. 0·1번 마스크에서는 해당 슬라이더가 회색 |
| 4 | Scene = Waterfront, 승강장 가장자리·전주 밑동 | 같은 마스크로 다른 물체에도 일관된 거품 |
| 5 | 잠시 지켜보기 | 두 장이 다른 방향으로 흘러 거품 모양이 계속 변함 |
| 6 | `tiled_previews` 그림 | 2 × 2로 이어도 이음매가 안 보임 |
| 7 | (선택) `.\tools\bake_foam_masks.ps1` | 다시 구워도 결과가 같음 (git에서 PNG 변경 없음) |
| 8 | 네 창의 대분류 (UI 정리) | 슬라이더보다 큰 글씨의 파란 막대. 누르면 접히고 펴짐. Camera Presets·Capture·Tonemapping·Ocean Detail은 처음에 접혀 있음 |
| 9 | `2` 키 Light 창 → **Underwater Light (test scenes)** → Clear / Green / Lagoon | 빛 감쇠 값과 물색이 함께 바뀌고 화면 물색도 바뀜. Water 창에는 감쇠·물색이 없고 짧아짐 |

기본 마스크로 띠가 너무 옅게 느껴지면 Width를 올리거나 Erosion을 낮추면 된다.

**확인 질문**

- [ ] 기본 마스크(Cells + bubbles + breakup)가 Clouds보다 자연스럽고 청량하게 보인다
- [ ] Water 창에서 마스크를 바꿔 가며 고를 수 있다
- [ ] 2 × 2 확인 그림에서 이음매가 안 보인다
- [ ] 대분류가 잘 보이고 접을 수 있으며, 감쇠·물색이 Light 창에 있다 (UI 정리)
