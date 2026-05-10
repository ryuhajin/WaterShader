# WaterShader — 스타일라이즈드 수면 HLSL 포트폴리오

외부 머티리얼 그래프나 엔진 추상에 의존하지 않고 raw HLSL로 직접 작성한 셰이더. ImGui로 모든 파라미터를 실시간 조절 가능하며, hot reload로 셰이더 파일 저장 즉시 결과 확인.

---

## 미리보기

ImGui 패널에서 다음을 실시간 조절:
- Lighting (방향/색/강도) · Camera (FOV/이동/회전속도) · Tint Color
- Water — Fresnel power · Shallow/Deep color · Normal Scale · Layer A/B U/V scroll speed
- Wave 0/1 — 방향/진폭/파장/속도
- Debug View — Sampled normal map / World-space N / UV / (foam mask, crest factor — feature 브랜치)

---

## 구현된 기능 (마감 산출분)

### `feature/water-base`
- **Sine wave 정점 변위** (2-layer) + **analytic normal 재계산** — 마루/골 방향 연속성 유지
- **2-layer scrolling normal map** + plane 가정 TBN(T=worldX, B=worldZ)
- **Fresnel 기반 cubemap reflection** — `lerp(litWater, env, fresnel * strength)`
- **Shallow/Deep color** — NdotV 기반 깊이감
- **양면 그리기 RasterizerState** — 단면 plane 아래에서도 보이도록

### `feature/water-normal-map`
- water_normal.dds 자산 투입 (texconv BC4_UNORM 변환)
- `.dds → .png → .jpg → flat` 다단계 폴백 텍스처 로더 + ImGui 진단 출력
- **Debug 시각화 모드** — Sampled normal map / World-space N / UV 직접 출력으로 진단 시간 0
- ImGui slider 분해 — `Normal Scroll 1/2` (Float2) → `Layer A/B - U/V speed (per sec)` (4개 분리)

### 인프라
- DirectX 11 + Win32 + ImGui + vcpkg manifest 통합
- HLSL hot reload (200ms 폴링, 컴파일 에러는 ImGui 빨간 텍스트로 노출, 기존 셰이더 유지)
- DirectXTK DDS/WIC 텍스처 로더, tinyobjloader OBJ
- Cubemap + 인라인 cube skybox + LessEqual DSS

### 추가 예정
- `feature/foam-mask` — wave-crest whitecap. **코드 90% 완성**, 시각 검증 미진행. 자산 1장 제작 완료. (브랜치 보존)
- `feature/water-specular` — Blinn-Phong specular (`git stash`에 작업분 보존)
- `feature/water-detail-normal` — Macro + Detail 2-layer normal map (사실적 ocean 표준)
- `feature/waterfall` — 폭포 ribbon 메시 + edge foam
- `feature/color-presets` — 분위기 preset 2개
- `feature/ripple-sdf`, `feature/ue5-port`

자세한 우선순위는 [`docs/ROADMAP.md`](docs/ROADMAP.md)의 **Post-deadline** 섹션 참고.

---

## 빌드 & 실행

### 필요 도구
- Windows 10/11
- Visual Studio 2022 (Desktop development with C++)
- CMake 3.24+
- [vcpkg](https://github.com/microsoft/vcpkg) (manifest mode)

### 의존성 (vcpkg manifest)
- [DirectXTK](https://github.com/microsoft/DirectXTK) — DDS/WIC 텍스처 로더, math
- [Dear ImGui](https://github.com/ocornut/imgui) — `win32-binding`, `dx11-binding`
- [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) — OBJ 파서

### 빌드 절차

```powershell
# vcpkg 환경 변수 (필요 시)
$env:VCPKG_ROOT = "C:\path\to\vcpkg"

# Configure (Debug 또는 Release)
cmake -S . -B build/vs2022 -G "Visual Studio 17 2022" `
      -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"

# Build
cmake --build build/vs2022 --config Debug

# Run
./build/vs2022/Debug/WaterShader.exe
```

### Hot reload

`shaders/*.hlsl` 또는 `*.hlsli` 파일을 저장하면 200ms 안에 자동 재컴파일 후 결과 즉시 반영. 컴파일 에러는 ImGui 패널 상단에 빨간 텍스트로 표시되며 기존 셰이더는 유지(검은 화면 안 됨).

---

## 컨트롤

| 키 | 동작 |
|---|---|
| **W / S** | 카메라 전후 이동 |
| **A / D** | 카메라 좌우 이동 |
| **Q / E** | 카메라 상하 이동 |
| **← → ↑ ↓** | 카메라 회전 (yaw / pitch) |
| ImGui | 모든 셰이더 파라미터 실시간 조절 |

---

## 아키텍처 한눈에

```text
[ImGui] → Graphics::water_ → ColorShader::PerFrameCB → cbuffer b0
                                                         │
                                                ┌────────┴────────┐
                                                ▼                 ▼
                                        [VSMain]          [PSMain]
                                  SineDisplace +     SampleWaterNormal +
                                  analytic normal    Lambert + Fresnel +
                                                     Cubemap reflection

자산:  assets/textures/skybox.dds (Cubemap)
       assets/textures/water_normal.dds (Normal map, BC4_UNORM)
       assets/models/32x32Plane.obj (33×35 grid plane)

셰이더 포함관계:
       Common.hlsli (cbuffer + PSInput)
       ├── vertexShader.hlsl
       └── PixelShader.hlsl
            ├── Lighting.hlsli (Lambert, Fresnel)
            └── Cubemap.hlsli  (TextureCube, SampleEnv)

별도 셰이더: skybox.hlsl (자체 cbuffer b1)
```

---

## 폴더 구조

```text
.
├── shaders/            # HLSL (hot reload 지원)
│   ├── Common.hlsli       # cbuffer + VSInput/PSInput 정의
│   ├── Lighting.hlsli     # Lambert, FresnelSchlick
│   ├── Cubemap.hlsli      # TextureCube + SampleEnv
│   ├── vertexShader.hlsl  # SineDisplace + VSMain
│   ├── PixelShader.hlsl   # SampleWaterNormal + PSMain
│   └── skybox.hlsl        # 별도 skybox 셰이더
├── src/                # C++ 엔진 (DirectX 11 + ImGui + 텍스처/메시 로드)
├── assets/
│   ├── models/         # OBJ
│   └── textures/       # DDS/JPG/PNG
├── docs/
│   ├── CONVENTIONS.md  # 문서/브랜치 워크플로 규칙
│   ├── OVERVIEW.md     # 프로젝트 목적/평가 포인트
│   ├── ROADMAP.md      # 일자별 일정 + Post-deadline backlog
│   └── features/       # feature 단위 SPEC + NOTES
│       ├── dx11-setup/
│       ├── shader-hot-reload/
│       ├── asset-pipeline/
│       ├── scene-lighting/
│       ├── env-cubemap/
│       ├── water-base/
│       ├── water-normal-map/
│       └── foam-mask/   # deferred — 코드는 feature/foam-mask 브랜치
└── CMakeLists.txt
```

---

## 워크플로

본 프로젝트는 **기능(feature) 단위 SPEC + 브랜치 워크플로**로 운영됩니다:

1. 새 기능 시작 시 `docs/features/_TEMPLATE/SPEC.md`를 복사해 `docs/features/<name>/SPEC.md` 작성 (4 필수 섹션)
2. SPEC을 main에 doc-only commit
3. `feature/<name>` 브랜치에서 구현
4. 머지 시 `--no-ff`로 브랜치 흔적 보존, ROADMAP 항목 체크
5. 폐기/연기 기능도 SPEC + NOTES + 브랜치 commit으로 시도 기록 보존

자세한 규칙은 [`docs/CONVENTIONS.md`](docs/CONVENTIONS.md) 참고.

---

## 참고 자료

- Schlick approximation (Fresnel) — Real-Time Rendering 4th, Ch. 9
- [Catlike Coding — Waves](https://catlikecoding.com/unity/tutorials/flow/waves/)
- [GPU Gems Ch.1 — Effective Water Simulation](https://developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-1-effective-water-simulation-physical-models)
- [DirectXTK Wiki](https://github.com/microsoft/DirectXTK/wiki)

---

## 기술 스택

`HLSL` · `DirectX 11` · `Win32` · `C++20` · `CMake` · `vcpkg` · `DirectXTK` · `Dear ImGui` · `tinyobjloader`
