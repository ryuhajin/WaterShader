# water-normal-map — 구현 메모

> 2026-05-10 · feature/water-normal-map

## 1. 핵심 학습 — Photoshop JPG가 WIC로 안 풀리는 이슈

**현상:** 사용자가 Photoshop으로 만든 JPG normal map을 `assets/textures/water_normal.jpg`에 두었는데 `CreateWICTextureFromFile` 호출이 실패. ImGui에 표시한 multi-line loader 상태에서 `[FAIL] JPG HRESULT 0x...`로 확인.

**원인 추정:** Photoshop은 JPG에 **Adobe APP14 마커** + **ICC 색상 프로파일**을 자동으로 박는다. WIC의 기본 JPG 디코더는 이 조합을 거부할 때가 있음. `xxd` 헤더 확인 시 `ff d8 ff ee 00 0e 41 64 6f 62 65` (= `\xFF\xD8 ... Adobe`)가 보였음.

**해결:** texconv.exe로 JPG → DDS(BC5_UNORM 또는 BGRA8)로 변환. DirectXTK의 `CreateDDSTextureFromFile`은 WIC를 거치지 않고 직접 DDS 헤더를 파싱하므로 영향 없음.

**대안:**
- (a) Photoshop에서 PNG로 다시 저장 (ICC 프로필/EXIF 체크 해제) — 가장 빠름
- (b) `Texture.cpp::Initialize`에서 DirectXTK의 WIC loader flag(`WIC_LOADER_IGNORE_SRGB`, `WIC_LOADER_FORCE_RGBA32`) 사용 — 코드 변경 필요
- (c) **본 feature가 채택한 방법:** texconv 변환 후 DDS 사용 — 변경 없이 즉시 잡힘

## 2. Loader 진단 도구 추가 (재사용 가능)

`Graphics.cpp` 텍스처 로드 블록을 `attempts[]` 루프로 재구성하면서 다음을 같이 박음:

- `normalMapStatus_` (std::string) — multi-line 상태 누적
- ImGui Debug View 섹션에 `TextWrapped`로 표시
- `OutputDebugStringA`로 디버거 출력 동시 노출

각 시도마다 `[OK] / [FAIL] / [FALLBACK]` prefix와 resolved path 함께 출력. 향후 다른 텍스처(skybox, foam, …) 로드 디버깅에도 동일 패턴 재사용 가능.

## 3. Debug 시각화 모드 (cbuffer 기반)

`g_DebugParams.x` (int via float) cbuffer 필드 추가, ImGui Combo로 4가지 모드:

| 모드 | 출력 | 용도 |
|---|---|---|
| 0 render | 정상 lit water | 비교 기준 |
| 1 Sampled normal map | `g_NormalMap.Sample(uv1).rgb` | 텍스처 로드 + UV scroll/scale 즉시 검증 |
| 2 World-space N | `N * 0.5 + 0.5` | TBN 합성 동작 확인 |
| 3 UV | `frac(input.uv).rg` | 메시 UV 정상 여부 |

cbuffer 끝에 `float4 g_DebugParams` 추가 (16-byte alignment 유지, 총 cbuffer 384 → 400 bytes).

**진단 가치 큼.** "노멀이 안 보인다"는 보고를 받았을 때 모드 1로 전환하면 텍스처 자체가 로드됐는지 즉시 확인 가능. 본 feature 진행 중 폴백 분기 진입을 이 모드로 분리해냈고, DDS 변환 후 정상 패턴 등장 확인.

## 4. ImGui 슬라이더 분해

기존 `SliderFloat2("Normal Scroll 1", ...)` 같은 2-component 슬라이더는 어떤 채널이 U인지 V인지 안 보임. 다음으로 분해:

```
Normal Scale (tile)
Normal map UV scroll velocity (2 layers blended)
  Layer A - U speed (per sec)
  Layer A - V speed (per sec)
  Layer B - U speed (per sec)
  Layer B - V speed (per sec)
```

Debug View 섹션은 항상 패널 최하단 (새 컨트롤이 위에 추가되도록).

## 5. 떠 있는 specular 작업

본 feature 시작 시점에 main에 떠 있던 Blinn-Phong specular 변경(`Lighting.hlsli::BlinnPhongSpecular`, `g_SpecularParams`, ImGui 슬라이더 2개 등)은 `git stash`로 분리. 본 feature 머지 후 `git stash pop`으로 복원 → `feature/water-specular`에서 SPEC + 정식 머지 예정. 단, 사용자 결정으로 다음 단계는 foam-mask가 우선이라 stash는 그 이후로 미뤄둠.

## 6. 다음 feature 인계

- **`feature/foam-mask` (Day 5):** 흰색 물보라/whitecap. wave crest(`displacedY * k`) + foam mask 텍스처 + threshold/softness blending. 자체 제작 텍스처 카운트 +1 더 가능.
- **`feature/water-specular` (보류):** stash 복원 후 SPEC 작성 + 머지. 현재 noise 디테일이 normal map만으로는 부족한 이유 중 하나가 specular highlight 부재.
- **추후 고려:** layer A/B에 서로 다른 normal map(macro + detail) 사용 — 표준 ocean 셰이더 패턴. 현재는 단일 텍스처 2회 샘플링이라 fine detail 한계 존재.

## 7. Sweet spot 시작값 (참고)

- Normal Scale = 1.0 (기본 1×1 타일)
- Layer A = (0.03, 0.02) — 느리고 가로 우세
- Layer B = (-0.02, 0.04) — 반대 방향, 세로 우세

ImGui 슬라이더로 즉시 변경 가능. 데모 캡처 시 타임라인 변화 추적해두면 영상 편집에 유용.
