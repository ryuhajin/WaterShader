# normal-select — 레이어별 노멀맵 선택 + 프리셋 저장 (회고 기록)

> 2026-10-03 · feature/normal-select

캡처 조건은 이전 feature와 같음(1280×720, `t = 12s`, 고정 샷 5 × 프리셋 3, 무인 모드).
재현: `WaterShader.exe --capture-feature normal-select --capture <label> [--normal-a <i> --normal-b <i>]`.

---

## 배경 — 노멀맵이 한 장뿐이었다

- **질문:** "표면 노멀과 스크롤되는 얕은 노멀이 같은 텍스처인가?" → **같은 텍스처였다.**
  픽셀 셰이더가 `g_NormalMap`(t1) 하나를 두 번 샘플링하고, 차이는 UV뿐이었다.
  - 레이어 A: `uv × normalScale + scrollA × t` (큰 물결)
  - 레이어 B: `uv × normalScale × detailScale(3) + scrollB × t` (잔물결, 3배 촘촘)
  - 두 노멀은 whiteout 블렌드(기울기 xy는 더하고 z는 곱함)로 합친다.
- 같은 무늬를 크기만 바꿔 겹치면 B가 A의 축소판이라 **반복이 눈에 띄고, 레이어마다 성격(큰 물결 / 잔물결)을 따로 줄 수 없다.**
- 저장소에는 노멀맵 JPG가 5장(`water_normal`, `water_normal1~4`) 있었지만 로더는 `water_normal.dds` 하나만 시도했다. 나머지 4장은 쓰이지 않던 상태.

---

## Step 1 — 노멀맵 5장 DDS 변환

- `water_normal.dds`와 같은 형식: `texconv -f R8G8B8A8_UNORM -m 0 --ignore-srgb` (512², 밉 10단계, 1.4 MB).
- **`--ignore-srgb`가 핵심:** water-polish에서 고친 버그 — Photoshop JPG의 sRGB 메타데이터 때문에 변환 시 감마 디코딩되어 평평한 노멀 128이 55가 됐었다. 재발 확인으로 최상위 밉 평균 RGB를 측정:

| 맵 | 평균 RGB | 기울기 RMS x / y | 이웃 텍셀 차이 | 성격 |
|---|---|---|---|---|
| 0 `water_normal` | (127.5, 127.6, 251.1) | 0.108 / 0.189 | 18.3 | 대각선 물결, 중간 주파수 (기존 기본값) |
| 1 `water_normal1` | (127.5, 127.5, 253.6) | 0.058 / 0.090 | 10.2 | 부드러운 너울 |
| 2 `water_normal2` | (127.5, 127.5, 253.9) | 0.050 / 0.064 | 8.4 | 가장 잔잔함 |
| 3 `water_normal3` | (127.5, 127.4, 252.9) | 0.053 / 0.125 | 11.8 | 한 방향 긴 줄무늬 |
| 4 `water_normal4` | (127.6, 127.5, 250.7) | 0.137 / 0.180 | 31.0 | 가장 거칠고 촘촘 |

전부 (128, 128, ~252)라 감마 문제 없음. 기울기 RMS = 표면 거칠기(클수록 반사·글린트가 넓게 흩어짐), 이웃 텍셀 차이 = 디테일의 촘촘함.
- JPG도 그대로 남김(원본). JPG를 WIC로 직접 읽으면 **밉이 없어** 멀리서 노멀이 깜빡이므로(ocean 그리드) DDS를 쓴다.

## Step 2 — 레이어별 선택 + 프리셋 저장

| 위치 | 변경 |
|---|---|
| `Graphics.cpp` `kNormalMaps` | 노멀맵 표(파일 + 이름). 프리셋은 **인덱스**를 저장하므로 항목은 뒤에만 추가 |
| `Graphics` | `normalMaps_`(전부 미리 로드), `normalMapA_` / `normalMapB_`. 로드 실패한 항목은 1×1 평평한 노멀로 대체해 인덱스 유지 |
| 프리셋 | 확장 키 `normalMapA`, `normalMapB` — **키가 없는 옛 파일은 0/0 = 기존과 동일**, 포맷 버전 변경 없음 |
| `ColorShader` | SRV 3개 바인딩 (t0 큐브맵, t1 레이어 A, t2 레이어 B) |
| `PixelShader.hlsl` | `g_NormalMapB : register(t2)`, 레이어 B만 그걸로 샘플링 |
| UI | Water 섹션에 `Normal Map A (broad)` / `Normal Map B (detail)` 콤보. `Save Current`가 함께 저장 |
| 명령줄 | `--normal-a <i>`, `--normal-b <i>`(비교 캡처용, 프리셋보다 우선). `--normal-map <path>`는 목록 끝에 추가되고 두 레이어에 적용 |
| 로더 상태 텍스트 | 항상 표시하던 `Normal Map Loader: [OK] ...` → **실패했을 때만** 표시 |

### 회귀 검증

0/0(기존과 같은 조합)으로 15장을 다시 찍어 before와 픽셀 비교 → **15장 모두 최대 차이 0**. 바인딩 구조를 바꿨지만 결과는 비트 단위로 같다.

---

## Step 3 — 노멀맵 비교 + 프리셋 조합 (`compare_maps/`, `captures/step1_preset_maps/`)

`--normal-a i --normal-b i`로 맵마다 15장씩 찍어 시트로 비교(`compare_maps/<shot>.jpg`, 열 = nm0~nm4). 원본 캡처 75장은 시트로 충분해 저장소에서 뺌.

관찰:
- nm1·nm2(부드러움)는 노멀 기울기가 작아 **Gerstner 파도 모양이 그대로** 드러나고, 글린트가 굵은 덩어리로 모인다.
- nm4(거침)는 반짝임이 잘게 부서져 **바람이 부는 수면**처럼 보인다.
- nm3(줄무늬)는 한 방향 결이 있어 바람 방향이 느껴진다.

프리셋 조합(A = 큰 물결 성격, B = 잔물결 디테일):

| 프리셋 | A | B | 의도 |
|---|---|---|---|
| Basic (한낮 들판) | 3 Long streaks | 4 Fine chop | 바람 결 + 햇빛에 잘게 부서지는 반짝임 |
| Sunset | 1 Soft swell | 2 Soft chop | 저녁의 잔잔한 수면, 하늘 반사가 덜 부서지게 |
| Tropical | 0 Diagonal ripples | 1 Soft swell | 기존 물결 유지, 디테일만 부드럽게 해 맑은 물 느낌 |

조합은 UI에서 바꾸고 `Save Current`로 덮어쓰면 된다.

## 검증

- 프리셋 저장/불러오기에 `normalMapA/B` 포함 확인(파일에 키 기록 → 재실행 캡처에 반영).
- 0/0 회귀 픽셀 차이 0, Debug/Release 경고 0.

## 교훈

- "디테일 레이어"가 같은 텍스처의 축소판이면 진짜 다른 주파수 성분이 아니라 **같은 무늬의 반복**일 뿐이다. 레이어마다 맵을 분리하자 텍스처 슬롯 하나(t2)로 표현 폭이 크게 늘었다.
- 이미 있는 에셋도 측정해 보면 성격이 수치로 갈린다(기울기 RMS 0.05 ~ 0.18, 3배 차이). 감으로 고르기 전에 숫자로 분류하면 조합을 설명하기 쉽다.
- 저장 포맷을 확장 키로 만들어 둔 덕분에(bench-tools) 새 파라미터를 넣어도 옛 프리셋 파일이 깨지지 않았다.
