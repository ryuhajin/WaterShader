# Water Scatter — NOTES

> 5단계 진행 기록. 상태: **에이전트 검증 완료, 사용자 승인 대기**

## 1. 개념 요약

```text
물 아래 색 = 바닥색 × 투과율 + 물색 × 들어오는 빛 × 세기 × (1 − 투과율)
             └ 4단계: 빠져나감 ┘   └──────── 5단계: 빠진 만큼 채움 ────────┘
들어오는 빛 = 하늘빛(환경광) + 햇빛 × 태양 높이
```

- 얕음(투과율 ≈ 1)은 바닥색, 깊음(투과율 ≈ 0)은 물색. 물색은 밝은 칸·어두운 칸에 같은 양을 더하므로 **남는 체크무늬 대비 = 투과율**.
- 물색에 장면 조명을 곱해, 노을 프리셋에서는 물도 같이 어둡고 따뜻해진다.
- **물리 모드**: 산란색 하나. **램프 모드**: 얕음/중간/깊음 3색 + 경계 깊이 2개, 기준은 수직 수심(기본, 같은 바닥은 늘 같은 색) 또는 시선 경로(낮게 볼수록 깊은 색).

## 2. 만든 것

| 파일 | 내용 |
|---|---|
| `shaders/PixelShader.hlsl` | `SCENE_INPUTS`: 들어오는 빛, 램프, 물색 몫, 투과 모드 3(Scattering), 디버그 15(물색 몫만)·16(램프 색) |
| `shaders/Common.hlsli` | cbuffer에 `g_ScatterColor`, `g_RampShallow/Mid/Deep`, `g_RampParams` |
| `src/ColorShader.*` | `WaterParams`에 물색 모드·산란색·세기·램프 3색·경계 2개·기준, sRGB → 선형 변환해 전달 |
| `src/Graphics.*` | 프리셋 키 17개(소멸 계수와 세기가 모두 없거나 0이면 **Clear로 시작**, 세기만 0이면 4단계 화면), Water 창 "Underwater Color"(물 종류 버튼, 모드, 세기, 색, 램프), Transmission 기본값 = Scattering, CLI `--water-type`, `--scatter`, `--ramp-by` |
| `tools/compare_depth.ps1` | `-Mode sum` (감쇠 + 물색 몫 = 최종, 선형), `-Mode contrast` (깊이별 체크무늬 대비 유지율) |

**물 종류 버튼** (출발점, 측정값 아님)

| 버튼 | 소멸 계수 (R, G, B /m) | 물색 | 결과 |
|---|---|---|---|
| Clear | 0.35, 0.07, 0.03 | 물리, (0.10, 0.35, 0.55) | 바닥이 깊이까지 보이고 깊은 곳은 파랑 |
| Green | 0.80, 0.25, 0.70 | 물리, (0.15, 0.45, 0.25) | 레퍼런스 사진 2처럼 탁한 초록, 계단이 빨리 묻힘 |
| Lagoon (ramp) | 0.60, 0.20, 0.15 | 램프 (0.35, 0.95, 0.90) → (0.05, 0.65, 0.80) → (0.02, 0.20, 0.55), 1m / 4m, 수심 기준 | 얕은 곳 채도 높은 청록, 깊은 곳 파랑 |

## 3. 에이전트 검증 (2026-10-24)

| 항목 | 방법 | 결과 |
|---|---|---|
| 빌드 | Debug·Release | 경고·오류 0 |
| 기존 장면 회귀 | 기존 샷 21장 vs 0단계 기준 | **21장 바이트 동일** |
| 세기 0 = 4단계 | Clear + 세기 0 (산란 모드) vs Clear 감쇠 모드, 5샷 × 3프리셋 PNG | **15장 바이트 동일** |
| 소멸 계수 0 = 물색 없음 | 계수 0 + 빨강 물색 세기 3 vs 물 끔 | **15장 바이트 동일** (1 − 1 = 0) |
| 두 몫의 합 | `-Mode sum`, Clear·Green × 5샷, 톤 곡선 끔 | 약 600만 물 픽셀 **허용 초과 0** (최대 오차 = 허용치의 66%) |
| 바닥 없는 곳 = 물색 | 위 sum의 "no-surface" | Green: 바닥 몫 **0.0%**. Clear: 최대 37% (아래 설명) |
| 대비 유지 | `-Mode contrast`, Clear | 아래 표 |
| 디버그 레이어 | Debug: Lagoon 산란, 물색 몫(15), 램프 색(16) | 경고·오류 0건 |
| 기본값 = Clear (2026-11-01 수정 후) | 옵션 없이 연 테스트 장면 vs `--water-type clear`, 물색 몫(15) × 3샷 × 3프리셋 | **9장 바이트 동일** |
| 수정 후 기존 장면 회귀 | 기존 샷 21장 vs 0단계 기준 | **21장 바이트 동일** |
| 수정 후 계수 0 = 물 끔 | `--extinction 0,0,0` vs `--water off`, 3샷 × 3프리셋 | **9장 바이트 동일** |
| 수정 후 디버그 레이어 | Debug: lab_stairs 기본값 | 경고·오류 0건 |

**체크무늬 대비가 남는 비율** (Clear, 패스스루 대비)

| 깊이 | lab_top | lab_stairs | lab_pillar | lab_low |
|---|---|---|---|---|
| 0.2m | 97% | 90% | 78% | 53% |
| 1m | 87% | 69% | 53% | 40% |
| 3m | 70% | 47% | 42% | 32% |
| 6m | 52% | 34% | 36% | 26% |
| 8m 바닥 | 45% | 38% | 35% | 25% |

- 0.2 → 6m는 모든 샷에서 줄어든다. 같은 깊이도 낮게 볼수록 덜 남는다 (경로가 길어 투과율이 낮음).
- 6m 받침대 윗면과 8m 바닥은 화면에서 보이는 픽셀 위치가 달라 경로 분포도 달라서, 일부 샷(계단)에서는 순서가 바뀐다.
- Clear의 "바닥 없는 곳"에 바닥 몫이 남는 이유: 파랑 소멸 계수 0.03 × 최대 거리 50m = 1.5 → 투과율 22%. 맑은 물은 50m로도 다 막히지 않는다. 바닥 없는 곳은 랩의 물 영역이 바닥보다 넓어 생기는 부분이라, 최종 장면(해저가 물 영역 전체를 덮음)에서는 생기지 않는다.

**캡처 위치 (로컬):** `C:\Users\da171\ws-capture\water-scatter\captures\`

| 폴더 | 내용 |
|---|---|
| `look_atten_clear` / `look_clear` | 4단계(감쇠만) vs 5단계, 같은 Clear 계수 |
| `look_green`, `look_lagoon`, `look_lagoon_path` | 물 종류, 램프 수심 기준 vs 경로 기준 |
| `sum_*`, `contrast_pass` | 수치 검사 입력 |
| `dbg_scatter`, `dbg_part`, `dbg_ramp` | Debug 빌드 캡처 + `d3d_messages.txt` |
| `dbg_part_default` | 수정 후 옵션 없이 연 테스트 장면의 물색 몫(15) = Clear |

회귀·동일성 캡처는 임시 폴더에서 비교하고 지웠다.

```powershell
.\tools\compare_depth.ps1 -Mode sum -A C:\Users\da171\ws-capture\water-scatter\captures\sum_clear_atten -B C:\Users\da171\ws-capture\water-scatter\captures\sum_clear_part -C C:\Users\da171\ws-capture\water-scatter\captures\sum_clear_full -D C:\Users\da171\ws-capture\water-scatter\captures\sum_clear_mask
.\tools\compare_depth.ps1 -Mode contrast -A C:\Users\da171\ws-capture\water-scatter\captures\sum_clear_mask -B C:\Users\da171\ws-capture\water-scatter\captures\contrast_pass -C C:\Users\da171\ws-capture\water-scatter\captures\sum_clear_full
```

### 과정에서 고친 것

| 증상 | 원인 | 해결 |
|---|---|---|
| 앱을 켜고 바로 Debug Mode = Water color part only를 고르면 물 전체가 검정 (사용자 보고) | 저장된 프리셋에 물속 키가 없어 소멸 계수 0·세기 0으로 시작 → 투과율 1, 물색 몫 0. 검증 안내 7번도 물 종류를 누른 상태라는 조건을 빠뜨림 | 프리셋을 적용할 때 소멸 계수와 세기가 모두 0이면 **Clear**로 시작. 이 값은 테스트 장면 셰이더만 읽어 기존 바다 화면은 그대로 (21장 바이트 동일). 안내 6·7번 문구 보강 |

## 4. 사용자 검증 안내

**실행:** `.\build\vs2022\Debug\WaterShader.exe` → `1` 키 → Scene = **Water Lab**, Transmission = **Scattering** (기본값) → `3` 키로 Water 창 → "Underwater Color". 처음에는 **Clear**가 적용된 상태로 열린다.

| 순서 | 조작 | 정상 | 실패 징후 |
|---|---|---|---|
| 1 | **Clear** 버튼, 카메라 lab_stairs | 얕은 체크무늬는 또렷, 깊은 계단·바닥은 파랑으로 묻힘. 4단계의 검정이 사라짐 | 깊은 곳이 여전히 검음, 얕은 곳까지 물색으로 덮임 |
| 2 | Strength를 0 ↔ 1로 | 0이면 4단계(검정), 1이면 물색이 채움 | 0에서도 색이 남음 |
| 3 | **Green** | 탁한 초록, 계단이 몇 단 만에 사라짐 (레퍼런스 사진 2) | — |
| 4 | **Lagoon (ramp)**, lab_low | 받침대 윗면(얕음) 청록, 깊은 곳 파랑 | — |
| 5 | 4번에서 "Ramp by view path" 체크 | 낮은 시점에서 받침대 윗면까지 파랑으로 바뀜 | 바뀌지 않음 |
| 6 | View 창(`1`) Presets에서 **Sunset** (물은 Clear로 돌아간다. 다른 물 종류를 보려면 다시 누른다) | 물색도 어둡고 따뜻해짐 | 물만 한낮처럼 밝음 |
| 7 | 물 종류(Clear 등)가 적용된 상태에서 Debug Mode = **Water color part only**, 카메라 lab_stairs | 바닥 몫을 지우고 물이 더한 색만 보인다: 수면 바로 아래 계단은 검정에 가깝고, 내려갈수록·멀수록 물색이 짙어진다. 어떤 곳이 어두울 때 바닥 탓인지(여기서 밝음) 물색 탓인지(여기서도 어두움) 가르는 화면 | 물 전체가 검정 (세기 0 또는 소멸 계수 0) |

**확인 질문**

- [ ] 깊은 계단은 물색에 섞이지만 얕은 체크무늬는 유지된다
- [ ] 산란 OFF(4단계)와 ON의 차이를 설명할 수 있다
- [ ] 물리 모드와 램프 모드의 차이, 램프 기준(수심 / 경로)의 차이를 설명할 수 있다
