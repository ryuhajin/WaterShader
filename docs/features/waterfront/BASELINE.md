# 0단계 — 기준 상태 기록

> 기록 2026-10-23 · 상태: **에이전트 기록 완료, 사용자 확인 대기**
> 계획: [PLAN.md](PLAN.md)

## 기준 리비전

- `main` = `fce510f` (README 정리 머지). 셰이더·C++ 마지막 변경은 `ae2dc0f` (ocean-hero).
- 미커밋 변경: `assets/camera_presets.txt` 슬롯 1 카메라 값 1줄. 고정 샷 캡처에는 쓰이지 않는다. → 처리 방법은 사용자 결정.

## 빌드·실행

```powershell
cmake --build build/vs2022 --config Release --target WaterShader
.\build\vs2022\Release\WaterShader.exe
```

- 기준 캡처 명령 (Release, 1280×720, 고정 시간 12.0초, UI 숨김):

```powershell
.\build\vs2022\Release\WaterShader.exe --capture baseline --capture-feature waterfront
```

- 캡처는 저장소 옆 `ws-capture/waterfront/captures/<label>/`에 저장된다 (Debug·Release 공통, 2026-10-23 변경). 이 기준 캡처는 당시 Release가 exe 옆에 저장해서 손으로 옮긴 것이다.

## 계측 조건

| 항목 | 값 |
|---|---|
| GPU | NVIDIA GeForce RTX 4080 SUPER (내장 AMD Radeon도 있음) |
| 해상도 | 1280×720 |
| 시간 | 캡처 고정 12.0초 (`kCaptureTime`) |
| 노출·톤매핑 | 프리셋 값 (hue-preserving ACES) |
| MSAA | 없음 |

## 현재 렌더 흐름 (`Graphics::Render`)

```text
HDR 타깃(RGBA16F) + 깊이(D24S8) 지우기
    ↓
하늘 큐브맵 + 해 원반              → HDR 타깃 (깊이 LessEqual)
    ↓
물 메시 1개 (작은 평면 또는 오션 그리드) → HDR 타깃에 덮어쓰기, 알파 1, 깊이 기록
    ↓
톤매핑 (전체 화면 삼각형)           → sRGB 백버퍼
    ↓
ImGui
```

- **패스는 사실상 하나다.** 물 아래에 그려진 것이 없고, 물이 앞 결과(색·깊이)를 읽지도 않는다.
- 물색 (`shaders/PixelShader.hlsl` 270~294행):
  - 기본색 = 시선이 수직이면 `FacingColor`, 비스듬하면 `GrazingColor` 쪽으로 섞기
  - 여기에 환경광 + 태양 Lambert 확산광 + 스펙큘러를 더함 → "칠한 표면"처럼 계산
  - Fresnel 비율로 하늘 큐브맵 반사와 섞고, 태양 글린트를 더함
- 깊이 버퍼는 `D24_UNORM_S8_UINT` 텍스처에 DSV만 있다 → **셰이더가 읽을 수 없다** (2단계에서 TYPELESS + SRV로 바꾸는 이유).
- 핫 리로드 대상: 물(`ColorShader`), 하늘(`SkyboxShader`), 톤매핑(`TonemapShader`) 셰이더 3개.

## 기존 프리셋·샷

- 프리셋 3개: Basic / Sunset / Tropical. 각각 작은 평면용과 오션 그리드용이 따로 있다 (`assets/shader_presets.txt`).
- 고정 샷 7개: `oblique`, `top`, `sunward` (작은 평면), `ocean_surface`, `ocean_aerial`, `ocean_tele`, `ocean_hero` (오션 그리드).
- 기준 캡처 21장 = 프리셋 3 × 샷 7: [ws-capture/waterfront/captures/baseline/](../../../../ws-capture/waterfront/captures/baseline/) (로컬)

## 이번 업그레이드로 바뀔 부분

| 지금 | 업그레이드 후 |
|---|---|
| 물 아래에 아무것도 없음 | 계단·바닥·기둥이 먼저 그려짐 (1·2단계) |
| 깊이를 셰이더가 못 읽음 | 물 패스가 "물 없는 장면"의 색·깊이를 읽음 (2단계) |
| 물색 = 고정 2색 + 확산광 | 물색 = 바닥색 × 투과율 + 물색(또는 깊이 색 램프) × (1 − 투과율) (4·5단계) |
| 반사 = 하늘 큐브맵만 | 같은 Fresnel로 반사·투과를 나눔 (6단계), 선택적으로 물체 반사 SSR (8B단계) |
| 거품 없음 | 기둥·계단 둘레 접촉 거품 (6B단계) |
| 그림자·코스틱 없음 | 9·10단계 |

## 사용자 확인 항목

- [ ] 기준 캡처가 지금 알고 있는 물 모습과 같은가
- [ ] "패스가 하나뿐이라 물 아래가 안 보인다"는 현재 구조를 설명할 수 있는가
- [ ] 위 "바뀔 부분" 표가 이해되는가
