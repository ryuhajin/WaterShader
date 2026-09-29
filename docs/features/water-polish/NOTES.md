# water-polish — 퀄리티 업 기록 (Before / After)

> 2026-09-29 · feature/water-polish

모든 캡처는 같은 조건: 1280×720, 시간 `t = 12.0s` 고정, UI 숨김, 카메라 3종 고정(`oblique` / `top` / `sunward`).
재현: `build/vs2022/Debug/WaterShader.exe --capture <label> [--debug <mode>]` → `captures/<label>/<preset>_<shot>.jpg`
비교 시트: `powershell -File make_compare.ps1 before step1_bugfix ...` → `compare/<shot>.jpg` (행 = 프리셋, 열 = 단계)

---

## Step 0 — Before 기록 도구

- `Graphics::StartCaptureSet / BeginCaptureFrame / EndCaptureFrame` — 프리셋 × 고정 샷 큐를 한 프레임에 하나씩 렌더하고, ImGui를 끈 채 backbuffer를 `DirectX::SaveWICTextureToFile`(JPEG q95)로 저장.
- 캡처 동안 시간은 `kCaptureTime`으로 고정 → 파도 위상이 같아서 before/after 비교가 공정함.
- `sunward` 샷은 스카이박스 태양 방향(yaw ≈ 34.5°)을 낮은 각도로 바라봄 → 반사광 확인용.
- 결과: `captures/before/`

## Step 1 — 버그 2개 수정 (`captures/step1_bugfix/`)

before 캡처를 분석하다 반사광이 없는 원인을 추적하니, 튜닝 문제가 아니라 **버그**였다.

### 1-a. 노멀맵이 감마 디코딩되어 있었음 (가장 큰 원인)

- **증상:** 반사광·확산광이 태양을 수면 *아래*에 둘 때만 보임. `--debug 2`(world normal)가 순수 초록(0,1,0)이 아니라 `(80,240,60)` ≈ **N = (−0.37, 0.88, −0.53)** — 수면 전체가 한쪽으로 기울어 있음.
- **원인:** `water_normal.dds`의 평균 RGB가 **(55, 57, 246)**. 평평한 탄젠트 노멀맵은 (128,128,255)여야 한다. `128`을 sRGB→linear로 변환하면 정확히 `55`. 원본 `water_normal.jpg`는 평균 (127.7, 127.7, 251)로 정상 → **texconv 변환 시 JPG의 sRGB 메타데이터(Photoshop ICC) 때문에 감마 디코딩**이 적용됨. 구 DDS와 `srgb_to_linear(jpg)`의 오차 0.2/255로 확인.
- **수정:** `texconv -f R8G8B8A8_UNORM --ignore-srgb -m 0 water_normal.jpg` 로 재생성.
- **교훈:** 노멀맵·마스크 등 *데이터 텍스처*는 색 공간 변환을 절대 거치면 안 된다. 텍스처 평균값 한 번 찍어보는 것이 가장 빠른 검증.

### 1-b. 광원 방향 규약이 뒤집혀 있었음

- 기존: `lightDir = (sin yaw·cos p, −sin p, cos yaw·cos p)`, 셰이더는 `−g_LightDirection`을 "광원을 향하는 방향"으로 사용 → 프리셋의 음수 pitch(−12°, −39.75°)는 태양이 **수면 아래**.
  1-a의 기울어진 노멀과 우연히 맞물려 "그럭저럭 밝게" 보였기 때문에 발견이 늦었다.
- 수정: `Sun Yaw / Sun Elevation`(고도 > 0 = 수평선 위)으로 재정의, `g_LightDirection = −toSun`.
- 스카이박스 태양 위치 측정: cubemap +Z 면 밝은 픽셀 추적 + `sunward` 캡처로 보정 → **yaw ≈ 34.5°, elevation ≈ 4°**. ImGui `Match Skybox Sun` 버튼 추가.
- 프리셋 파일 v2: 두 번째 필드 의미가 바뀌어 버전 업. 고정 필드 뒤에 `key value` 확장 필드를 붙일 수 있게 해서 이후 파라미터 추가 시 포맷 변경 불필요.
- 기존 튜닝 값(색·Fresnel·스크롤 등)은 그대로 두고 태양만 재배치: Basic 20°, Sunset 5°, Tropical 32° (yaw는 모두 스카이박스 태양에 맞춤).

### 1-c. 디버그 뷰 추가

- `--debug 5` / Debug Mode 5 = **Lighting terms** — R = Lambert, G = Blinn-Phong, B = Fresnel 반사량. 이번 버그를 이 뷰로 확정했다.

## Step 2 — HDR 태양 글린트 + 하이라이트 롤오프 (`captures/step2_sun_glint/`)

```
R        = reflect(−V, N)
glint    = pow(saturate(dot(R, L)), glintPower) · glintIntensity · F
color   += glint · lightColor
out      = HighlightRolloff(color)   // 0.8 이하는 그대로, 초과분은 1 − exp 곡선으로 압축
```

- Blinn-Phong(넓은 광택)과 별개로, **태양 원반의 거울 반사**를 따로 둔다. 잔물결 면 하나하나가 태양 방향과 맞을 때 번쩍이고, 이것들이 모여 태양 쪽으로 뻗는 "빛의 길(glitter path)"이 된다.
- `glintIntensity`는 1보다 훨씬 크게(기본 12) 둔 HDR 값. 그대로 출력하면 1.0에서 잘려 평평한 흰 덩어리가 되므로 셰이더 끝에서 롤오프.
- 롤오프를 0.8 이하에서는 항등으로 둔 이유: 스카이박스는 톤매핑 없이 LDR로 그려지므로, 물에만 ACES 같은 전체 톤커브를 걸면 하늘 반사가 실제 하늘보다 어두워져 이질감이 생긴다.
- 파라미터: `g_SpecularParams.zw` (Sun Glint Power / Intensity), 프리셋 확장 필드 `sunGlintPower`, `sunGlintIntensity`.

### 남은 문제 (→ Step 3)

- 파도 뒷면에서 반사 벡터가 수평선 아래를 향해 **초원(초록)이 비침**.
- Fresnel `F0 = 0.5` 고정 → 정면에서도 하늘이 반쯤 비쳐 물 색이 뿌옇다. Sunset은 Grazing 색이 순수 빨강(1,0,0)이라 "빨간 비닐" 느낌.
- 노멀맵 두 레이어가 같은 스케일 → 반복 패턴. 비스듬한 각도에서 블러(anisotropic 미사용).

## Step 3 — 잔물결 디테일 (`captures/step3_ripple_detail/`)

```
uv2      = uv · normalScale · detailScale          // 레이어 B = 잔물결 (A의 3배 타일링)
N_ts     = normalize( (n1.xy + n2.xy) · strength,  n1.z · n2.z )   // whiteout blend
envDir   = (R.x, |R.y|, R.z)                        // 수평선 아래 반사 → 하늘로 접기
```

- **Whiteout blend:** 기존 `normalize(n1 + n2)`는 두 레이어의 기울기를 평균 내서 각각의 디테일이 절반으로 줄어든다. xy(기울기)는 더하고 z는 곱하면 두 레이어가 모두 살아 있다.
- **레이어 B 스케일 분리** (`detailScale = 3`) → 같은 텍스처지만 반복 주기가 달라 타일링이 덜 보임. `normalStrength = 0.8`.
- **Anisotropic 16x** (`D3DClass::CreateSampler` wrap 샘플러) — 수면은 거의 항상 비스듬히 보이므로 trilinear만으로는 잔물결이 뭉개진다.
- **수평선 아래 반사 접기:** 스카이박스 아래쪽은 초원이라 파도 뒷면에 초록이 비쳤다. 실제 물 반사에 땅이 보일 일은 거의 없으므로 `|R.y|`로 하늘을 샘플링. 태양 글린트 계산은 원래 R을 그대로 사용.
- 이 단계에서는 F0를 파라미터화만 하고 값은 0.5로 유지(색 변화와 분리해서 비교하려고).

## Step 4 — 물의 몸체 색 (`captures/step4_water_body/`)

- `F0 0.5 → 0.05`, `reflectionStrength → 1`, `fresnelPower → 5`(Schlick). 정면은 물 색, 수평 쪽은 하늘 반사가 지배.
- **1차 시도 F0 = 0.02 + 어두운 deep 색 → 실패:** 위에서 보면 거의 검은 비닐. 이 셰이더는 물속 산란광(물 속에서 다시 올라오는 빛)을 모델링하지 않기 때문에, 물리적으로 맞는 F0만 쓰면 몸체가 지나치게 어둡다. → 몸체 색을 "산란광 색"으로 보고 밝게 올리고 F0를 0.05로 살짝 스타일라이즈.
- Blinn-Phong 강도를 0.2~0.25로 낮춤 — 넓은 광택이 "은박지" 느낌의 주원인. 태양 하이라이트는 Step 2 글린트가 담당.
- 프리셋 재정의 (노을 색은 물 색이 아니라 **하늘 반사**로 나오게):

| 프리셋 | 성격 | Facing / Grazing | 태양 고도 | 글린트 |
|---|---|---|---|---|
| Basic | 맑은 호수 | 청록 (0.10,0.30,0.34) / (0.16,0.40,0.42) | 20° | 800 / 14 |
| Sunset | 해 질 녘 | 어두운 보라 (0.14,0.12,0.20) / (0.22,0.17,0.24) | 5° | 500 / 22 (넓고 강한 빛의 길) |
| Tropical | 얕은 열대 바다 | 터쿼이즈 (0.05,0.55,0.55) / (0.20,0.70,0.65) | 32° | 1000 / 14 |

## Step 5 — Gerstner 파도 4개 (`captures/step5_gerstner/`)

```
P.xz += Q·A·D·cos θ,   P.y += A·sin θ,   θ = k(D·xz) − ωt
N     = (−Σ D.x·kA·cos θ,  1 − Σ Q·kA·sin θ,  −Σ D.y·kA·cos θ)      // GPU Gems 1, ch.1
Q_i   = steepness_i / (k_i · A_i · N_waves)                          // 루프(겹침) 방지
```

- 기존: 2×2 plane 위에 파장 2짜리 sine 2개 → 판 전체가 텐트 하나처럼 휨.
- 변경: 파장 1.6 / 1.05 / 0.62 / 0.41 (≈1.5배 간격), 방향을 바람 방향 주변 ±40°로 분산, steepness 0.55~0.7. 정점이 마루 쪽으로 모여 **마루는 뾰족, 골은 넓고 평평**해진다.
- cbuffer `WaveParams`의 padding 자리에 `steepness`를 넣어 레지스터 2개(32B) 레이아웃 유지, `static_assert`로 C++ 쪽 크기 고정. 프리셋 v3.

## Step 6 — 수평선까지 이어지는 수면 (`captures/step6_ocean_grid/`)

- **문제:** 2×2 판이 초원 위에 떠 있어서 셰이더가 좋아져도 "물"이 아니라 "소품"으로 보인다. 또 태양 고도가 4°라 거울 반사 지점이 카메라에서 ~6 유닛 밖 → 판 위에는 빛의 길이 생길 자리가 없음.
- **Ocean grid** (`Model::InitializeGrid`): 1024² quad, 좌표를 `x = sign(t)·R·(e^{g|t|} − 1)/(e^g − 1)`로 매핑(R = 400, g = 6) → 중심 간격 ≈ 0.012, 20 유닛에서 ≈ 0.25. UV는 OBJ 판과 같은 밀도(2 유닛당 1).
- **파도 거리 LOD:** 멀수록 격자가 성겨지므로 파도마다 `1 − smoothstep(8λ, 14λ, dist)`로 페이드 → 파장당 정점 ≥ 5개 유지. 처음 512² / 10λ~18λ로 했을 때 중거리에서 모아레가 생겨 계산으로 조건을 맞춤. 먼 수면은 잔잔한 거울이 되어 수평선의 나무·언덕이 자연스럽게 비친다.
- 스카이박스의 초원이 수면에 가려지고 먼 나무 줄이 "건너편 호숫가"가 됨 → 에셋 교체 없이 장면이 성립.
- ImGui `Ocean Grid` 토글, 캡처 샷 `ocean_sunward`(태양 쪽 저각) / `ocean_wide` 추가.

## 결과 정리

- 비교 시트: `compare/oblique.jpg`, `compare/sunward.jpg`, `compare/top.jpg` (before → step2 → step4 → step6), `compare/ocean_sunward.jpg`, `compare/ocean_wide.jpg` (최종).
- Breakdown용 디버그 뷰 (Tropical): `captures/final_breakdown/mode{0,1,2,3,5}_tropical_{oblique,ocean_wide}.jpg` — 0 렌더, 1 노멀맵 샘플, 2 월드 노멀, 3 UV, 5 라이팅 항.

## 다음 후보

- 마루 산란광(fake SSS): `worldPos.y` 기반 crest mask × 역광 각도 → 역광 샷에서 파도 마루가 청록으로 비침.
- `feature/foam-mask` 재개: Gerstner 마루(수평 수렴 = Jacobian < 1)를 foam 마스크로 쓰면 SPEC의 높이 기반보다 정확.
- 거리 기반 노멀 강도 감쇠 / 두 번째 노멀맵(현재 한 장을 두 스케일로 재사용).
- sRGB 스왑체인 + 선형 공간 라이팅 (현재는 감마 공간 합산).

