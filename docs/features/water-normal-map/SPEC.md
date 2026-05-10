# water-normal-map

> Branch: `feature/water-normal-map` · Status: in-progress · Updated: 2026-05-10

## 1. Goal / Visual Target

- **한 문장 요약:** 자체 제작한 물 노멀 맵을 투입해 표면에 잔물결(fine ripple)이 보이도록 하고, 기존 폴백(평면 노멀)에서 정상 경로로 복귀시킨다.
- **시각 목표 키워드:** fine ripple, surface flow, no-more-flat-shading.
- **참고 이미지/영상:** 비교용 `references/water-normal-map-20260510.png` (Phase 3 캡처).
- **스코프 가드 — 절대로 만들지 않을 것:**
  - Blinn-Phong specular 추가 — `feature/water-specular` (다음)
  - 4-layer sine wave 확장 — `feature/water-multi-wave`
  - FBM procedural detail normal — `feature/water-detail-fbm`
  - Foam mask 렌더링/자산 — `feature/foam-mask` (Day 5)
  - Depth fade / SSR / caustics — 마감 시간 외

## 2. HLSL 접근법 / 의사코드

**셰이더 변경 없음.** Day 4 `feature/water-base`에서 이미 구현된 2-layer scrolling normal map 합성 + plane TBN 그대로 사용.

기존 `shaders/PixelShader.hlsl::SampleWaterNormal`:

```hlsl
float2 uv1 = uv * g_NormalScale + g_NormalScroll.xy * g_Time;
float2 uv2 = uv * g_NormalScale + g_NormalScroll.zw * g_Time;
float3 n1 = g_NormalMap.Sample(g_NormalSampler, uv1).xyz * 2 - 1;
float3 n2 = g_NormalMap.Sample(g_NormalSampler, uv2).xyz * 2 - 1;
float3 nTan = normalize(n1 + n2);
// plane TBN: T=worldX, B=worldZ
float3 N = normalize(nTan.x*T + nTan.y*B + nTan.z*N0);
```

폴백 픽셀 (128, 128, 255) → tangent space (0, 0, 1)이라 위 식에서 `nTan = (0, 0, 1)` → `N = N0` (정점 노멀과 동일) → 잔물결 0. 정상 텍스처가 들어오면 `nTan`이 의미 있는 방향을 가져 표면이 흔들림.

## 3. Inputs / Outputs

| 종류 | 이름 | 형식 | 범위/비고 |
|---|---|---|---|
| Asset | `assets/textures/water_normal.jpg` | 2D normal map | 자체 제작, 후보 5장 중 기본 픽 (water_normal1~4.jpg, Water_normal2.jpg는 비교용) |
| Loader | `Graphics.cpp` 텍스처 체인 | C++ | `.dds → .png → .jpg → flat` 순으로 시도 (`.jpg` 분기 신규) |
| CBuffer | `g_NormalScale` | float | 0~3, 기본 1.0 (water-base 그대로) |
| CBuffer | `g_NormalScroll` | float4 (xy/zw) | water-base 기본값 그대로 |
| ImGui | Normal Scale slider | — | water-base에서 노출됨 (변경 없음) |

**의존하는 다른 feature:** `water-base` (모든 셰이더 코드 + cbuffer + ImGui), `asset-pipeline` (Texture::Initialize WIC 분기 — JPG 자동 처리).

## 4. Acceptance Criteria / Test Plan

- [ ] 빌드 성공, C++/HLSL 경고 0
- [ ] **로더 검증:** 디버거 또는 출력으로 `Texture::InitializeFlat` 폴백이 호출되지 않음 확인 (`.jpg` 경로 성공)
- [ ] **Normal Scale 0** → 화면이 sine wave 변위만 + 평면 셰이딩 (잔물결 없음, 회귀 검증)
- [ ] **Normal Scale 1.0** → 잔물결이 표면에 명확히 보임. 환경 반사가 노멀에 따라 흔들림
- [ ] **Normal Scroll 흔들기** → UV 흐름 시각화, 시간이 흐르며 두 layer가 다른 속도로 스크롤
- [ ] **카메라 줌인/줌아웃** — 카메라 멀리서도 평균색이 푸른빛 유지, NaN/black pixel 없음
- [ ] 회귀: Fresnel/Cubemap 반사, Wave 변위, Light, Tint, Skybox toggle 모두 정상
- [ ] `references/water-normal-map-20260510.png` 캡처 1장 저장
- [x] 자체 제작 텍스처 사용 — `assets/textures/water_normal.jpg` (포트폴리오 self-made 카운트 +1)

## 5. Notes (선택)

- **JPG 한계:** JPG는 8×8 블록 압축이라 노멀 맵에 미세 단차가 생길 수 있음. 시간 남으면 PNG 재저장 또는 texconv `-f BC5_UNORM` 변환 권장. NOTES.md에 기록.
- **로더 수정 위치:** `src/Graphics.cpp:52-68` 텍스처 로드 블록. 기존 `.dds → .png` 두 시도 사이/뒤에 `.jpg` 시도 추가. WICTextureLoader는 JPG 자동 처리하므로 코드 패턴 동일.
- **후보 4장 보관:** `water_normal1~4.jpg`, `Water_normal2.jpg`는 비교용으로 보관. 이번 feature에선 기본 픽 `water_normal.jpg`만 사용. 시각 검증 후 기본 픽 교체 가능.
