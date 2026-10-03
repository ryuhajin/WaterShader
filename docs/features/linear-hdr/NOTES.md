# linear-hdr — 리니어 워크플로 + HDR 렌더 타깃 + 톤매핑 (회고 기록)

> 2026-10-03 · feature/linear-hdr

모든 캡처: 1280×720, 시간 `t = 12.0s` 고정, UI 숨김, 고정 샷 5종 × 프리셋 3종, 무인 모드로 실행.
재현: `WaterShader.exe --capture-feature linear-hdr --capture <label> [--debug <mode>]` → `captures/<label>/`
비교 시트: `powershell -File docs/features/water-polish/make_compare.ps1 -Dir ../linear-hdr <단계...>` → `compare/`

---

## 배경 — 왜 감마 공간 계산이 틀렸나

**확인한 사실 (수정 전 코드):**

| 위치 | 상태 |
|---|---|
| 백버퍼 | `R8G8B8A8_UNORM`, 출력 시 인코딩 없음 |
| 큐브맵 (skybox, env_*) | `CreateDDSTextureFromFile` 기본 → `R8G8B8A8_UNORM` / `BC7_UNORM` 그대로 샘플 |
| 색 파라미터 | ImGui에서 sRGB로 고른 값을 그대로 cbuffer로 |
| 셰이더 | sRGB ↔ linear 변환 없음 |

→ 입력부터 출력까지 전부 **sRGB로 인코딩된 숫자**로 빛을 더하고 섞고 있었다.

**sRGB 인코딩은 빛의 양에 비례하지 않는다.** sRGB 0.5는 빛의 양으로 0.214(약 21%)다. 그래서:

| 연산 | 감마 공간에서 계산 | linear에서 계산 → 다시 sRGB | 결과 |
|---|---|---|---|
| 빛 더하기 0.5 + 0.5 | 1.0 (흰색, 클립) | 0.214 + 0.214 = 0.428 → **0.686** | 감마 쪽이 과하게 밝음 |
| 50% 섞기 (검정↔흰색) | 0.5 | 0.5 → **0.735** | 감마 쪽이 어둡고 탁함 |
| 곱하기 0.5 × 0.5 | 0.25 | 0.214 × 0.214 = 0.046 → 0.243 | 거의 같음 |

- **덧셈과 보간(lerp)이 틀어진다.** 이 셰이더에서는 `ambient + diffuse + specular`(덧셈)와 `lerp(body, sky, F)`(Fresnel 보간)가 핵심이다.
- **곱셈은 거의 영향이 없다.** 거듭제곱 곡선은 곱을 보존한다: (ab)^2.2 = a^2.2 · b^2.2. 그래서 `body × ambient`는 원래도 큰 문제가 없었다.
- 출력이 8비트라 1.0 초과 값(태양 글린트)은 잘림 → 물 셰이더 안에서 `HighlightRolloff`(0.8 이상 압축)로 땜질하고 있었다.

**목표 파이프라인:** sRGB 입력 → **linear 디코딩** → linear 계산 → **HDR(float) 누적** → 노출·톤매핑 → **sRGB 인코딩** 출력.

---

## Step 0-a — 무인 실행 모드

캡처를 돌리는 동안 사용자가 다른 작업을 하고 있었으므로, 자동 실행이 포커스를 빼앗거나 입력을 받지 않도록 먼저 고쳤다. 자세한 내용은 `bench-tools/TROUBLESHOOTING.md` F 항목.
- `--capture` / `--no-input`: `WS_EX_NOACTIVATE` + `SW_SHOWNOACTIVATE` + `HWND_BOTTOM`, 키보드·마우스 메시지는 ImGui·Input 전에 버림.
- 검증: 실행 전후 전경 창 동일, 메시지로 W·드래그를 보내도 카메라·판 값 불변.

## Step 0 — before (`captures/before/`, `captures/before_debug5/`)

감마 공간 계산 그대로의 결과. Debug 5(R = Lambert, G = Blinn-Phong, B = Fresnel 반사량)도 함께 기록.

---

## Step 1 — 리니어 워크플로 + HDR 타깃, 톤매핑 없음 (`captures/step1_linear/`)

### 해결 방식

| 경로 | 변경 | 이유 |
|---|---|---|
| 큐브맵 | `CreateDDSTextureFromFileEx(..., DDS_LOADER_FORCE_SRGB)` → `R8G8B8A8_UNORM_SRGB` / `BC7_UNORM_SRGB` SRV | **`_SRGB` SRV로 샘플링하면 GPU가 필터링 *전에* sRGB→linear 디코딩.** 셰이더에서 `pow`로 풀면 이미 필터링(밉·bilinear)된 *인코딩 값*을 섞은 뒤라 부정확하다. |
| 색 파라미터 | ImGui·프리셋은 sRGB 유지, `ColorShader` 업로드 시 `SrgbToLinear` (rgb만, 강도는 그대로) | 사람이 고르는 값은 sRGB가 자연스럽고, 셰이더에는 linear가 필요. 변환 지점을 cbuffer 업로드 한 곳으로 모음. |
| 렌더 타깃 | 장면은 `R16G16B16A16_FLOAT` HDR 타깃에, 깊이 버퍼 공유 | 1.0 초과 값을 그대로 보관. **float 포맷은 `_SRGB`가 없다 — 원래 linear.** |
| 출력 | 새 `tonemap.hlsl` 풀스크린 패스: HDR → (이 단계는 clamp만) → `LinearToSrgb` → 백버퍼 | 아래 "인코딩 위치" 참고 |
| 노멀맵 | 변경 없음 (UNORM) | 데이터 텍스처. 감마 변환하면 노멀이 기운다(water-polish Step 1의 버그와 같은 원인) |

- 변환 함수는 `pow(2.2)` 근사가 아니라 **정확한 sRGB 구간 함수**(0.04045 / 0.0031308 경계의 선형 구간 포함). HLSL `shaders/Color.hlsli`, C++ `src/ColorSpace.h`.
- `HighlightRolloff`는 이 단계에선 **일부러 유지** — 변화가 순수하게 "linear 변환" 때문인지 보기 위해.

### 인코딩을 어디서 하나 — `_SRGB` 백버퍼 vs 셰이더

- `_SRGB` RTV로 백버퍼를 만들면 GPU가 쓰기 시점에 자동 인코딩해 준다.
- 하지만 **ImGui가 같은 백버퍼에 sRGB 색 그대로 그린다.** `_SRGB` 타깃이면 UI 색이 한 번 더 인코딩되어 허옇게 뜬다.
- 그래서 백버퍼는 UNORM 그대로 두고, **톤매핑 패스의 마지막 줄에서 직접 `LinearToSrgb`**. 그 위에 ImGui가 그대로 그려진다. 스왑체인(DISCARD) 설정도 바꿀 필요가 없다.

### 과정에서 걸린 것

- **디버그 뷰가 망가지는 문제:** 노멀/UV/라이팅 항 디버그 값은 "빛"이 아니라 데이터다. 그대로 두면 마지막 sRGB 인코딩 때문에 밝게 변한다. 디버그일 때 인코딩을 통째로 끄면, 뒤의 스카이박스가 linear 값 그대로(어둡게) 보인다.
  → 물 셰이더에서 디버그 값을 **미리 `SrgbToLinear`로 디코딩**해서 내보내고, 디버그 중엔 톤매핑 패스가 노출 1·곡선 없음으로 동작. 인코딩·디코딩이 정확히 상쇄되어 원래 값이 그대로 나온다.
- **풀스크린 삼각형이 컬링됨(실행 전에 발견):** `SV_VertexID`로 만든 삼각형 (−1,1)→(3,1)→(−1,−3)은 화면에서 시계 방향인데, 기본 래스터라이저는 `FrontCounterClockwise = TRUE` + 후면 컬링이라 그대로면 화면이 까맣게 나온다. 패스 동안 양면 래스터라이저 사용.
- `std::max`가 `<windows.h>`의 `max` 매크로와 충돌 → `(std::max)(...)`.

### 검증

**1) 스카이박스 왕복 오차** (하늘 영역, before vs step1, 0~255):

| 샷 | 평균 | 최대 |
|---|---|---|
| basic_sunward | 0.15 | 4 |
| sunset_ocean_sunward | 0.36 | 6 |
| tropical_ocean_wide | 0.26 | 8 |

→ 평균 0.4 미만 = JPEG 압축 노이즈 수준. **sRGB SRV 디코딩 → linear → `LinearToSrgb` 인코딩이 정확히 상쇄**된다는 증거. (최대값은 JPEG 블록 경계와 float16 반올림)

**2) 디버그 뷰 5 (물 영역):** 평균 차이 0.20~0.21 → before와 동일.

**3) 물이 어떻게 달라졌나** (영역 평균, HSB):

| 프리셋 | 영역 | before | step1 (linear) |
|---|---|---|---|
| Basic | 위에서 본 몸체 | S 0.48 B 0.19 | **S 0.29 B 0.25** |
| Basic | 오션 앞쪽 | S 0.24 B 0.34 | **S 0.10 B 0.44** |
| Sunset | 위에서 본 몸체 | S 0.24 B 0.06 | **S 0.14 B 0.16** |
| Sunset | 오션 앞쪽 | S 0.04 B 0.19 | **S 0.07 B 0.33** |
| Tropical | 위에서 본 몸체 | S 0.82 B 0.32 | S 0.78 B 0.30 |
| Tropical | 오션 앞쪽 | S 0.61 B 0.39 | S 0.55 B 0.39 |

- **어둡던 물(Basic, Sunset)이 밝아지고 채도가 빠졌다** = 하늘 반사가 더 강하게 보인다. 이유: `lerp(body, sky, F)`를 linear에서 하면 밝은 하늘의 실제 빛 양이 반영된다. F = 0.05여도 하늘(linear ~0.6)의 5%는 어두운 몸체(linear ~0.01~0.07)와 같은 크기다. 감마 공간에서는 이 반사 비중이 과소평가되어 물이 "칠한 색"처럼 보였다.
- **밝은 몸체(Tropical)는 거의 그대로** — 몸체 자체가 밝아서 반사 비중 차이가 덜 드러난다.
- 결론: 프리셋 값은 감마 공간 기준으로 맞춘 것이라 linear에서는 그대로 옮겨지지 않는다 → Step 3에서 재튜닝.

### 교훈

- "sRGB 텍스처를 linear로 읽는 것"은 셰이더 `pow`가 아니라 **`_SRGB` 뷰 포맷**으로 해야 필터링까지 정확하다. 반대로 데이터 텍스처는 절대 `_SRGB`로 읽으면 안 된다 — 같은 스위치가 색엔 정답, 노멀엔 버그.
- 변환을 추가할 땐 **왕복 테스트**(디코드→인코드가 원본을 돌려주는지)를 먼저 측정하면, 이후 생기는 변화가 전부 "계산 공간이 바뀐 효과"라는 걸 분리해서 말할 수 있다.
- 감마 공간 오류는 주로 **덧셈과 보간**에서 생긴다. 곱셈만 있는 부분은 거의 그대로다.
