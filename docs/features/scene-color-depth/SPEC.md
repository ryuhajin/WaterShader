# Scene Color & Depth — 2단계 물 없는 장면의 색·깊이와 기본 투과

> Branch: `feature/scene-color-depth` · Status: in-progress · Updated: 2026-10-24
> waterfront 업그레이드 2단계. 앞 단계: [../water-lab/NOTES.md](../water-lab/NOTES.md)

## 1. Goal / Visual Target

- **한 문장 요약:** 물이 "물 없는 장면"의 색과 깊이를 읽을 수 있게 패스를 나누고, 아무 효과 없이 그대로 내보내서 **물을 켜도 물 끈 화면과 똑같이** 만든다.
- 이번 단계 결과는 "투명한 유리 같은 물"이다. 예뻐지는 단계가 아니라 **영점 조정**이다. 여기서 틀리면 이후 감쇠·굴절이 전부 틀린 값 위에 쌓인다.
- 적용 범위: Water Lab / Waterfront 장면. 기존 벤치 평면·오션 그리드는 지금 경로 그대로.
- **스코프 가드 — 만들지 않을 것:**
  - 감쇠·물색·굴절·반사 (4단계 이후)
  - 깊이로 위치·거리 복원 (3단계)
  - 파도 (Lab Waves는 꺼 둔 채로 검증)

## 2. HLSL 접근법 / 수식 / 의사코드

**패스 순서 (테스트 장면)**

```text
1. 하늘 + 불투명 물체 → HDR 색 / 장면 깊이          (깊이: 쓰기 가능)
2. CopyResource: HDR 색 → 장면 색 복사본             (물 없는 장면 색)
3. 깊이를 "읽기 전용"으로 다시 묶음
4. 물: 깊이 테스트만 하고 깊이는 쓰지 않음
       셰이더가 장면 색 복사본·장면 깊이를 읽음
       → 기존 HDR 색 타깃에 최종 색 기록 (알파 블렌딩 없음)
5. 장면 색 복사본·깊이 SRV 바인딩 해제 → 톤매핑
```

**리소스 형식**

| 리소스 | 지금 | 바꾼 뒤 |
|---|---|---|
| 깊이 텍스처 | `D24_UNORM_S8_UINT`, DSV만 | `R24G8_TYPELESS`, DSV + SRV |
| 깊이 쓰기 뷰 | `D24_UNORM_S8_UINT` DSV | 그대로 |
| 깊이 읽기 전용 뷰 | 없음 | 같은 형식 DSV + `D3D11_DSV_READ_ONLY_DEPTH \| READ_ONLY_STENCIL` |
| 깊이 셰이더 뷰 | 없음 | `R24_UNORM_X8_TYPELESS` SRV |
| 장면 색 복사본 | 없음 | `R16G16B16A16_FLOAT`, SRV (HDR 타깃과 같은 크기) |

**물 픽셀 셰이더 — 투과 모드**

```hlsl
// 화면 픽셀 좌표 그대로 한 텍셀을 읽는다 (보간 없음 → 값이 정확히 같다)
int2 pixel = int2(input.position.xy);
float3 background = g_SceneColor.Load(int3(pixel, 0)).rgb;
float  sceneDepth = g_SceneDepth.Load(int3(pixel, 0)).r;   // 0~1, 비선형 (3단계에서 거리로 바꿈)

if (transmissionMode == 1) return float4(background, 1.0); // 효과 없음 = 물 없는 화면과 같아야 함
```

**디버그 뷰 (테스트 장면, 물 디버그 모드에 추가)**

| 모드 | 내용 | 보여 주려는 것 |
|---|---|---|
| Scene color copy | 복사본 그대로 | 물이 무엇을 읽는가 |
| Depth buffer | 깊이 버퍼 값(0~1)을 그대로 회색으로 | 깊이는 읽힌다. 그런데 0~1 값이 거리와 비례하지 않는다 → 3단계 |
| Water coverage | 장면 색 위에 물이 그려진 픽셀만 파랗게 | 물 밖 기둥·계단 윗부분은 물이 안 그려짐 (깊이 테스트) |

**참고 자료**

- D3D11 DSV 읽기 전용 플래그 — https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_dsv_flag
- 깊이 텍스처를 SRV로 읽기 (TYPELESS 형식) — https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-depth-stencil

## 3. Inputs / Outputs

| 종류 | 이름 | 형식 | 범위 / 비고 |
|---|---|---|---|
| Texture (PS t3) | `g_SceneColor` | RGBA16F SRV | 물 없는 장면 색 |
| Texture (PS t4) | `g_SceneDepth` | R24 SRV | 물 없는 장면 깊이, `Load`로 읽음 |
| CBuffer | `g_TransmissionParams.x` | float | 0 = 기존 불투명 물, 1 = 투과(효과 없음) |
| DepthStencilState | 물 전용 | LessEqual, 쓰기 끔 | 읽기 전용 DSV와 함께 |
| ImGui | Transmission | combo | Opaque (old) / Pass-through. 테스트 장면 기본값 Pass-through |
| ImGui | Debug Mode | combo | 위 디버그 뷰 3개 추가 |
| CLI | `--transmission <0\|1>` | | 캡처용 |
| Output | HDR 타깃 rgb | linear | 물이 그린 최종 색 |

**의존하는 다른 feature:** `water-lab` (테스트 장면, `--water`, `--scene-debug`)

## 4. Acceptance Criteria / Test Plan

**에이전트 검증**

- [ ] Debug·Release 빌드 경고 0, D3D 디버그 레이어 RTV/SRV/DSV 충돌 경고 0 (Debug 빌드 출력 확인)
- [ ] 기존 샷 21장이 0단계 기준과 바이트까지 같음 (벤치·오션 경로 회귀 없음)
- [ ] 테스트 장면 5샷 × 프리셋 3: **Pass-through 물 켬 == 물 끔** (PNG 바이트 비교)
- [ ] Water coverage 디버그에서 물 밖 기둥·계단 윗부분·+5cm 받침대에 물이 그려지지 않음
- [ ] 창 리사이즈 후에도 같은 결과 (복사본·깊이 SRV 재생성)

**사용자 검증**

- [ ] Transmission을 Pass-through ↔ Opaque로 바꾸며 "물이 아래를 읽는다"는 차이를 확인
- [ ] Depth buffer 화면을 보고 "깊이 값이 거리와 비례하지 않는다"를 확인
- [ ] 왜 복사본이 필요한지, 왜 깊이를 읽기 전용으로 묶어야 하는지 설명할 수 있다

## 5. Notes

- 언리얼 대응: Single Layer Water의 `SceneColorWithoutWater` / `SceneDepthWithoutWater`가 같은 역할이다 (15단계에서 추적).
