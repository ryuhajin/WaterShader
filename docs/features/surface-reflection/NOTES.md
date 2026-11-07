# Surface Reflection — NOTES

> 6단계 진행 기록. 상태: **에이전트 검증 완료, 사용자 승인 대기**

## 1. 개념 요약

```text
R    = Fresnel 비율 × 반사 세기        // 정면 F0(물 0.02), 낮게 볼수록 1에 가까워짐
최종 = 물속 색(5단계) × (1 − R) + (하늘 반사 + 태양 글린트) × R
```

- 수면에서 튕겨 나간 빛(반사)과 수면을 뚫고 들어간 빛(투과)은 합쳐서 100%. 둘을 Fresnel 하나로 나눈다.
- 예전 불투명 물의 **칠한 물색 경로 전체**(Facing/Grazing 2색, 환경광·확산광, 블린-퐁 하이라이트)를 테스트 장면에서 뺐다. 물속 색이 그 자리를 대신한다. 남겨 두면 물색과 태양이 두 번 들어간다.
- 반사는 미리 준비된 하늘 큐브맵에서 온다. 장면 물체(기둥·계단)는 아직 비치지 않는다 (8B단계).
- 기존 바다 모드는 바꾸지 않았다 (사용자 결정 2026-11-02). 새 코드는 `#if SCENE_INPUTS` 안에만 있어 기존 바다 프로그램은 컴파일 결과가 같다.

## 2. 만든 것

| 파일 | 내용 |
|---|---|
| `shaders/PixelShader.hlsl` | `SCENE_INPUTS`: 투과 모드별 물속 색(`underwaterColor`)을 계산만 해 두고 노멀·Fresnel·하늘·글린트 계산으로 이어감. 끝에서 `물속 × (1 − R) + 반사 × R`. 반사 끔이면 물속 색을 바로 반환. 디버그 17(R)·18(반사 몫)·19(물속 몫) |
| `src/ColorShader.*` | `WaterParams::surfaceReflection` → `g_SceneParams.z` |
| `src/Graphics.*` | View 창 Scene 섹션 "Surface reflection" 체크박스(기본 켬, Opaque 모드에서는 비활성), CLI `--reflection on\|off`, Debug Mode 17~19 라벨, 18·19는 HDR 화면(톤 곡선 적용). Water 창 "Water Color (open water)" 표시, Reflection 섹션에 테스트 장면 공식 한 줄 |
| `tools/compare_depth.ps1` | `-Mode mean`: 회색 디버그 화면의 물 픽셀 평균·최소·최대 |

## 3. 에이전트 검증 (2026-11-02)

| 항목 | 방법 | 결과 |
|---|---|---|
| 빌드 | Debug·Release | 경고·오류 0 |
| 기존 장면 회귀 | 기존 샷 21장 vs 0단계 기준 | **21장 바이트 동일** |
| 반사 끔 = 5단계 | 수정 전 빌드로 찍은 5단계 화면 vs 수정 후 `--reflection off`, 5샷 × 3프리셋 PNG (+ Green 2샷 × 3프리셋) | **15장 + 6장 바이트 동일** |
| 두 몫의 합 | `-Mode sum`: 반사 몫(18) + 물속 몫(19) = 최종, 톤 곡선 끔, Tropical 프리셋, Clear·Green × 5샷 | 약 520만 물 픽셀 **허용 초과 0** (최대 오차 = 허용치의 67%) |
| 칠한 물색 경로 제거 | 바다 프리셋의 Facing/Grazing 색을 자홍·초록, 하이라이트 세기 5로 바꾼 프리셋 파일(`--preset-file`) vs 원래 프리셋, 5샷 × 3프리셋 | 13장 동일, 2장이 1픽셀씩 1레벨 차이 → 같은 조건으로 다시 찍자 동일 (아래 설명). 대조: Opaque 모드에서는 3장 모두 달라짐 |
| 낮게 볼수록 반사 증가 | `-Mode mean`: R 화면(17) 물 픽셀 평균, Basic | 아래 표 |
| 디버그 레이어 | Debug: 최종, 17, 18, 19 × 4샷 | 경고·오류 0건 |

**반사 비율 R의 물 픽셀 평균** (Basic: F0 0.04, 반사 세기 0.7 → 정면 최솟값 0.028, 최댓값 0.7)

| 샷 | 평균 | 최소 | 최대 |
|---|---|---|---|
| lab_top (바로 위) | 0.027 | 0.027 | 0.031 |
| lab_stairs | 0.062 | 0.027 | 0.353 |
| lab_pillar | 0.129 | 0.027 | 0.698 |
| front_rails | 0.162 | 0.027 | 0.616 |
| lab_low (낮은 시점) | 0.222 | 0.027 | 0.698 |

- 위에서 볼 때는 물속이 97% 보이고, 낮은 시점에서는 평균 22%, 먼 수면은 70%까지 하늘이 덮는다.
- 최댓값이 1이 아니라 0.7인 이유: Basic 프리셋의 반사 세기 0.7이 곱해진다 (아트 조절값).
- "두 몫의 합"의 no-surface 항목은 이번 검사에서는 "반사 몫이 최종의 몇 %인가"를 뜻한다. 먼 수면(바닥 없음)은 반사가 최대 100%.
- 칠한 물색 검사의 1픽셀 차이: 첫 실행에서만 Sunset·Tropical lab_low에 1픽셀씩 1레벨 차이가 나고, 같은 명령을 다시 찍으면 원래 프리셋과 바이트 동일했다 (같은 프리셋 파일끼리도 첫 실행과 둘째 실행이 달랐음). 셰이더 값이 아니라 실행마다 생기는 GPU 쪽 흔들림으로 판단.

**캡처 위치 (로컬):** `C:\Users\da171\ws-capture\surface-reflection\captures\`

| 폴더 | 내용 |
|---|---|
| `before_step5` / `after_step6` | 같은 5샷 × 3프리셋, 반사 없음(5단계) vs 반사 (PNG) |
| `dbg_render`, `dbg_fresnel`, `dbg_refl_part`, `dbg_under_part` | Debug 빌드 Basic 4샷 + `d3d_messages.txt` |
| `sum_clear_*`, `fresnel`, `fresnel_mask` | 수치 검사 입력 |

회귀·동일성 캡처는 임시 폴더에서 비교하고 지웠다.

```powershell
.\tools\compare_depth.ps1 -Mode sum -A C:\Users\da171\ws-capture\surface-reflection\captures\sum_clear_refl -B C:\Users\da171\ws-capture\surface-reflection\captures\sum_clear_under -C C:\Users\da171\ws-capture\surface-reflection\captures\sum_clear_full -D C:\Users\da171\ws-capture\surface-reflection\captures\sum_clear_mask
.\tools\compare_depth.ps1 -Mode mean -A C:\Users\da171\ws-capture\surface-reflection\captures\fresnel_mask -B C:\Users\da171\ws-capture\surface-reflection\captures\fresnel
```

### 과정에서 고친 것

| 증상 | 원인 | 해결 |
|---|---|---|
| 없음 | — | — |

### 관찰: 위에서 내려다보면 기둥이 잠겨 보이지 않는다 (사용자 검증 중, 2026-11-07)

- 낮은 시점에서는 기둥이 수면에서 뚜렷하게 끊기는데, 위에서 비스듬히 내려다보면 수면선이 보이지 않는다.
- 원인: 그 각도에서 기둥 근처의 반사 비율 R은 3~5%다. 수면선 바로 아래는 경로 ≈ 0이라 투과율도 1이다. 수면선 위아래 색 차이가 약 3%라 경계가 생기지 않는다. 계산대로 나온 결과이며 버그가 아니다.
- 결론: 게임 카메라 각도에서는 Fresnel 반사만으로 잠김이 읽히지 않는다. 경계에만 생기는 단서가 필요하다. 접촉 거품(6B), 굴절(8), 물체 반사(8B)가 그 역할을 한다.
- 비교 화면(로컬): `C:\Users\da171\ws-capture\surface-reflection\captures\observation_waterline\`. 6B 후 같은 각도로 다시 비교한다.

## 4. 사용자 검증 안내

**실행:** `.\build\vs2022\Debug\WaterShader.exe` → `1` 키 → Scene = **Water Lab** (Clear 물, Transmission = Scattering, Surface reflection 켬이 기본)

| 순서 | 조작 | 정상 | 실패 징후 |
|---|---|---|---|
| 1 | 카메라 **lab_top** | 5단계와 거의 같음. 하늘이 아주 옅게(약 3%) 얹히고 잔물결 무늬가 살짝 보임 | 위에서도 하늘이 진하게 덮음 |
| 2 | 카메라 **lab_low** | 먼 수면은 하늘·풀숲이 비치고 물속이 잘 안 보임. 가까운 수면은 물속이 보임. **기둥이 수면에서 끊겨 보임** (위는 그대로, 아래는 반사가 얹혀 흐려짐) | 먼 곳까지 물속이 그대로 보임 |
| 3 | 2번에서 Surface reflection 끄기/켜기 | 끄면 5단계 화면 (기둥 수면선 사라짐) | 끄고 켜도 같음 |
| 4 | Debug Mode = **Reflection amount** | 가까운 수면 검정, 먼 수면일수록 밝음 | 전체가 같은 밝기 |
| 5 | Debug Mode = **Reflection part only** ↔ **Underwater part only** | 반사 몫은 먼 곳이 밝음, 물속 몫은 가까운 곳이 밝음. 두 화면이 서로 반대 | — |
| 6 | Water 창(`3`) Reflection의 **Reflection strength**를 0 ↔ 1 | 0이면 반사 끔과 같음, 1이면 먼 수면일수록 거울에 가까워짐 | — |
| 7 | Water 창 "Water Color (open water)"의 색을 바꿈 | 테스트 장면은 바뀌지 않음 (Transmission을 Opaque로 바꾸면 바뀜) | 테스트 장면 물색이 바뀜 |

**확인 질문**

- [ ] 위에서 보면 물속이 잘 보이고, 낮은 시점에서는 하늘 반사가 물속을 덮는다
- [ ] 반사 몫과 물속 몫을 디버그로 나눠 보고, 둘을 더하면 최종 화면이 되는 것을 설명할 수 있다
- [ ] 예전 불투명 물(2색 + 확산광)에서 무엇을 빼고 무엇으로 바꿨는지 설명할 수 있다
