# Wave Occlusion — NOTES

> 7단계 진행 기록. 상태: **에이전트 검증 완료, 사용자 승인 대기**

## 1. 개념 요약

```text
테스트 장면의 물은 장면 깊이를 "읽어야" 해서(2단계) 깊이를 쓰지 못한다 → 파도가 겹치면 나중에 그린 조각이 이김
① 장면 깊이 → 물 깊이 버퍼로 복사
② 물 깊이 프리패스: 물을 깊이만 그림 (LESS, 쓰기) + 이긴 픽셀에 스텐실 1
③ 물 색 패스: 물 깊이 버퍼를 읽기 전용으로, 깊이 EQUAL + 스텐실 1   → 가장 가까운 수면만, 한 번만 셰이딩
   (수심·경로 계산은 원래 장면 깊이를 그대로 읽는다)

수심 = (물 너머 표면 바로 위의 파도 높이) − (표면 높이)
파도 높이 = 옆으로 밀린 점을 거꾸로 찾아(고정점 반복 5번) 그 점의 높이
```

- 파도 함수(`AccumulateGerstnerWave`)를 `Waves.hlsli`로 옮겨 정점 셰이더, 물 픽셀 셰이더, 장면 셰이더(정답지)가 같은 식을 쓴다.
- 물 영역 끝 1m에서 파도를 0으로 줄여(`WaterEdgeFade`) 옆 이동이 가장자리를 끌어당겨 생기던 틈을 막는다.
- 파도 값은 Surface 창(4)의 Waves 그대로. View 창 Lab Waves가 기본 켬이 되었다 (끄면 평평한 물).

## 2. 만든 것

| 파일 | 내용 |
|---|---|
| `shaders/Waves.hlsli` | `WaveParams`, `AccumulateGerstnerWave`(시간·추가 감쇠를 인자로), `WaterEdgeFade`, `WaterSurfaceOffset`, `WaterSurfaceHeightAt`(5회 반복, 옆 오차 반환) |
| `shaders/vertexShader.hlsl` | 공유 함수 사용. `SCENE_INPUTS` 변형에서만 가장자리 감쇠 |
| `shaders/PixelShader.hlsl` | 수심 = 표면 위 파도 높이 − 표면 높이. 표면이 그 자리 수면보다 위면 경로 0 (공기로 봄), 숫자 화면(12)은 "물 위" 표시. 거품은 1cm 넘게 드러난 표면엔 없음. 디버그 25(높이 역산 옆 오차, 흰색 = 1mm) |
| `shaders/scene.hlsl`, `src/SceneShader.*` | 정답지(Scene Debug 1·2)가 같은 파도 높이에서 수심을 잼. 장면 cbuffer에 파도·카메라·시간·물 영역 |
| `shaders/Common.hlsli`, `src/ColorShader.*` | `g_WaterArea`, 장면용 정점 셰이더 변형, 깊이만 그리는 그리기(`depthOnly`: 픽셀 셰이더 없음) |
| `src/D3DClass.*` | 물 깊이 버퍼(장면 깊이와 같은 형식) + 쓰기/읽기 전용 뷰, 깊이 복사, 프리패스 상태(LESS + 스텐실 1 기록), 색 패스 상태(EQUAL + 스텐실 1 요구) |
| `src/Graphics.*` | Lab Waves 기본 켬, Water 창 New: Underwater의 **Water occlusion**(None / Single pass / Depth prepass), CLI `--lab-waves`, `--water-occlusion`, Debug Mode 25, 물 영역 크기 상수 |
| `tools/diff_sets.ps1` | 두 캡처 세트의 차이 픽셀 수·물 위 여부·최대 레벨·좌표 (compare_sets는 같다/다르다만 알려 줌) |

## 3. 에이전트 검증 (2026-11-07)

검증용 큰 파도: 바다 프리셋의 네 파도 진폭을 5배로 바꾼 프리셋 파일(`--preset-file`). Basic 기준 주 파도 진폭 0.25m, 파장 3.6m. 앱에서는 Surface 창 Height 0.25와 같다.

| 항목 | 방법 | 결과 |
|---|---|---|
| 빌드 | Debug·Release | 경고·오류 0 |
| 기존 장면 회귀 | 기존 샷 21장 vs 0단계 기준 (정점 셰이더를 공유 함수로 바꾼 뒤) | **21장 바이트 동일** |
| 파도 끔 = 6C | main(6C)으로 찍은 6샷 × 3프리셋 vs `--lab-waves off` (세 가림 모드 각각) | 11장 동일, 7장이 1~4픽셀 차이 (아래). **세 가림 모드끼리는 서로 같음** |
| 가림: Prepass = Single pass | 큰 파도 6샷 | 5장 동일, 1장 3픽셀 (아래) |
| 가림: None과의 차이 | 큰 파도 6샷 | 샷당 3.5만~13만 픽셀 (먼 파도·파도 뒷면이 앞을 덮던 곳). `compare_sheets/occlusion_*.png` |
| 높이 역산 | 디버그 25, `-Mode mean` | 옆 오차 평균 0.001~0.002mm, **최대 0.20mm** (목표 1mm) |
| 파도 있을 때 수심 | 복원(12) vs 정답지(Scene Debug 2), `-Mode depth` | 6샷 모두 허용 초과 0, 최대 오차 0.0039m (인코딩 1칸) |
| 거품 | `-Mode foam` (정답지 = 파도 기준 수심) | 가장 깊은 거품 0.249m, 폭 초과 0 |
| 가장자리 틈 | `before_bigwaves` vs `after_bigwaves` (lab_pillar_near 위쪽 경계) | 들쭉날쭉하던 물 영역 끝이 곧게 |
| 디버그 레이어 | Debug: 세 가림 모드 × 2샷 (큰 파도) | 경고·오류 0건 |

- **파도 끔에서 남은 1~4픽셀:** 물체가 수면과 만나는 수면선 픽셀이다. 깊이 정밀도 때문에 물 너머 표면이 수면보다 살짝 위(+0~몇 cm)로 복원되는 곳으로, 이번 단계부터 "그 자리 수면보다 위의 표면 = 공기로 봄"(경로 0)과 "1cm 넘게 드러난 표면엔 거품 없음"이 적용되어 바뀌었다. 세 가림 모드에서 똑같이 나와 가림과는 무관하다.
- **Prepass vs Single 3픽셀 (front_rails):** 두 물 조각이 정확히 같은 깊이인 곳으로 보인다. Single pass는 LESS라 먼저 그린 조각이, Prepass는 EQUAL이라 마지막 조각이 남는다.
- **처음 Prepass 구현의 결함 (아래 표):** 깊이 EQUAL만 쓰면 물과 장면 깊이가 같은 수면선 픽셀에서 물이 통과해 버렸다.
- 정답지의 "not measured in B" 0~113픽셀: 정답지는 수면 아래인데 복원은 수면 위로 판정한 수면선 픽셀.

**큰 파도 화면 비교** (`compare_sheets/occlusion_<샷>.png`: None | Depth prepass | 바뀐 픽셀 빨강): 바뀐 곳은 거의 전부 먼 쪽 수면이다. None에서는 뒤쪽 파도 마루와 파도 뒷면이 앞 파도 위에 겹쳐 그려져 수면에 투명한 층이 생긴다.

**캡처 위치 (로컬):** `C:\Users\da171\ws-capture\wave-occlusion\captures\`

| 폴더 | 내용 |
|---|---|
| `before_waves` / `after_waves` | 프리셋 파도(5~9cm), 5샷 × 3프리셋, 7단계 전(가림 None, 가장자리 감쇠 없음) / 후 |
| `before_bigwaves` / `after_bigwaves` | 큰 파도, Basic 5샷 |
| `big_none`, `big_single`, `big_prepass`, `compare_sheets` | 가림 모드 비교 |
| `big_key`, `big_key_bands`, `big_mask`, `big_depth_bands`, `big_foam`, `big_side` | 정답지·복원 수심, 거품, 높이 역산 오차 |
| `dbg_*` | Debug 빌드 + `d3d_messages.txt` |

```powershell
.\tools\diff_sets.ps1 -A C:\Users\da171\ws-capture\wave-occlusion\captures\big_single -B C:\Users\da171\ws-capture\wave-occlusion\captures\big_prepass -Mask C:\Users\da171\ws-capture\wave-occlusion\captures\big_mask
.\tools\compare_depth.ps1 -Mode mean -A C:\Users\da171\ws-capture\wave-occlusion\captures\big_mask -B C:\Users\da171\ws-capture\wave-occlusion\captures\big_side
.\tools\compare_depth.ps1 -Mode depth -A C:\Users\da171\ws-capture\wave-occlusion\captures\big_key -B C:\Users\da171\ws-capture\wave-occlusion\captures\big_mask
```

### 과정에서 고친 것

| 증상 | 원인 | 해결 |
|---|---|---|
| 파도를 끈 평평한 물에서 Prepass만 수면선 몇 픽셀이 크게 다름 (최대 196레벨, 물 마스크 밖) | 깊이 EQUAL만으로는 물 깊이와 장면 깊이가 같은 픽셀(수면선)에서 물이 통과한다. None의 LESS에서는 장면이 이기던 곳 | 프리패스가 물이 이긴 픽셀에 스텐실 1을 남기고, 색 패스는 EQUAL + 스텐실 1을 함께 요구. 이후 세 모드가 같은 화면 |
| 큰 파도에서 높이 역산 옆 오차가 1mm 넘는 픽셀 (흰색 포화) | 반복 1번마다 오차가 합친 가파름(약 0.4)배로 줄어, 옆 이동 약 8cm가 3번 뒤에도 약 4mm 남음 | 5번 반복 → 최대 0.20mm |
| PowerShell에서 매번 만든 픽셀 차이 분석이 다음 호출에서 사라짐 | 호출마다 새 세션 | `tools/diff_sets.ps1`로 정리 |

## 4. 사용자 검증 안내

**실행:** `.\build\vs2022\Debug\WaterShader.exe` → `1` 키 → Scene = **Water Lab** (Lab Waves 기본 켬) → `4` 키 Surface 창 **Waves → Height**를 0.2~0.25로 (프리셋 5cm로는 겹침이 거의 안 보임) → `3` 키 Water 창 New: Underwater의 **Water occlusion**

| 순서 | 조작 | 정상 | 실패 징후 |
|---|---|---|---|
| 1 | 카메라 **lab_low**, Water occlusion **None ↔ Depth prepass** | None에서는 먼 수면에 뒤쪽 파도·파도 뒷면이 겹쳐 투명한 층이 보이고, Prepass에서는 앞 파도가 뒤를 가림 | 두 모드가 같음 |
| 2 | **Single pass** | Prepass와 같은 화면 | 다름 |
| 3 | 카메라 **lab_pillar_near**, 잠시 지켜보기 | 기둥의 거품 띠가 파도를 따라 오르내림 | 띠가 한 높이에 고정 |
| 4 | 같은 카메라에서 물 영역 끝(화면 위쪽) | 경계가 곧고 풀밭이 틈으로 보이지 않음 | 경계가 들쭉날쭉, 틈 |
| 5 | Debug Mode **Water depth (from depth buffer)** → `X` → Scene Debug **Water depth (WP, m)** (정답지) | 두 화면의 띠가 거의 같음 (둘 다 파도 높이 기준) | 띠가 크게 어긋남 |
| 6 | Debug Mode **Wave height lookup error** | 거의 검정 (흰색 = 1mm) | 밝은 곳이 넓음 |
| 7 | View 창 **Lab Waves** 끄기 | 6C와 같은 평평한 물 | — |

**확인 질문**

- [ ] 낮은 시점에서 먼 파도가 가까운 파도를 덮지 않는다 (None ↔ Prepass)
- [ ] 기둥의 거품 띠가 파도를 따라 오르내린다
- [ ] 물 깊이 프리패스가 왜 필요한지(같은 깊이를 읽고 쓰지 못함, 셰이딩 한 번, 스텐실로 동점 처리) 설명할 수 있다
