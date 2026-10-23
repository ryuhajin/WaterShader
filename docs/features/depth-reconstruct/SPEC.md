# Depth Reconstruct — 3단계 위치·거리 복원

> Branch: `feature/depth-reconstruct` · Status: draft · Updated: 2026-10-24
> waterfront 업그레이드 3단계. 앞 단계: [../scene-color-depth/NOTES.md](../scene-color-depth/NOTES.md)

## 1. Goal / Visual Target

- **한 문장 요약:** 물 픽셀에서 깊이 버퍼 값을 거꾸로 계산해 **물 아래 표면의 월드 위치**를 되찾고, 거기서 두 거리를 만든다.
  - **수직 수심** — 표면이 수면보다 얼마나 아래인가 (1단계 정답지 `Water depth (WP, m)`와 같아야 함)
  - **시선 경로** — 시선이 물속에서 실제로 지나간 길이 (4단계 감쇠가 쓸 값)
- 화면은 아직 바뀌지 않는다 (투과 모드 그대로). 이 단계의 결과물은 **디버그 화면과 수치 비교**다.
- **스코프 가드 — 만들지 않을 것:**
  - 감쇠·물색 (4단계)
  - 파도 수면 기준 수심 (7단계, 지금은 평평한 수면)
  - 깊이 보간으로 만든 가짜 중간 표면, 하늘을 바닥처럼 복원하는 것

## 2. HLSL 접근법 / 수식 / 의사코드

**위치 복원** — 그릴 때 거친 변환(월드 → 카메라 → 투영 → 원근 나눗셈)을 역행렬로 되돌린다.

```hlsl
float2 uv  = (pixel + 0.5) / screenSize;
float4 ndc = float4(uv.x * 2 - 1, 1 - uv.y * 2, sceneDepth, 1);   // D3D: 화면 y는 아래로, NDC y는 위로
float4 p   = mul(ndc, g_InvViewProjection);                       // 역행렬
float3 receiverWS = p.xyz / p.w;                                  // 원근 나눗셈 되돌리기
```

**두 거리**

```text
수직 수심 = 수면 높이 − 표면 높이       = waterPos.y − receiver.y
시선 경로 = 수면 점에서 표면까지의 거리 = length(receiver − waterPos)
```

- `waterPos` = 지금 그리는 물 픽셀의 월드 위치 (평평한 수면에서는 y = 0).
- 공기 중 거리(카메라 → 수면)는 경로에 넣지 않는다.

**정보가 없는 픽셀**

- 깊이 = 1.0(아무것도 안 그려진 곳, 하늘) → 복원하지 않는다. 시선 경로 = **최대 광학 거리**(기본 50m), 디버그에서 자홍색으로 표시.
- 깊이는 `Load`로 한 텍셀만 읽는다. 경계에서 두 표면의 깊이를 섞으면 존재하지 않는 중간 표면이 생긴다.

**디버그 뷰**

| 이름 | 어디 | 내용 |
|---|---|---|
| Water depth (from depth buffer) | 물 디버그 | 복원한 수직 수심을 **정답지와 같은 색 띠**로 |
| Underwater view path | 물 디버그 | 시선 경로를 같은 색 띠로 (같은 받침대도 비스듬히 보면 더 깊은 색) + 하늘 자홍 |
| Depth numeric | 물 디버그 / Scene Debug | 수심을 R = 소수부, G = 정수부/16으로 담은 비교용 화면. 정답지(Scene Debug)와 복원(물 디버그)을 PNG로 찍어 픽셀별 비교 |

- 띠 색은 `DepthBands.hlsli` 하나로 두 셰이더가 같이 쓴다 (`scene.hlsl`에서 옮김).

## 3. Inputs / Outputs

| 종류 | 이름 | 형식 | 범위 / 비고 |
|---|---|---|---|
| CBuffer | `g_InvViewProjection` | float4x4 | (view × projection)의 역행렬, 매 프레임 |
| CBuffer | `g_SceneParams.y` | float | 최대 광학 거리 (m), 기본 50 |
| ImGui | Max underwater distance | slider | 5 ~ 200m |
| ImGui | Debug Mode | combo | 위 물 디버그 3개 추가 |
| ImGui | Scene Debug | combo | Depth numeric (WP) 추가 |
| 도구 | `tools/compare_depth.ps1` | PowerShell | 정답지 PNG vs 복원 PNG: 픽셀 수, 최대·평균 오차, 허용 초과 픽셀 |

**의존하는 다른 feature:** `scene-color-depth` (장면 깊이 SRV, `SCENE_INPUTS` 변형), `water-lab` (정답지)

## 4. Acceptance Criteria / Test Plan

**에이전트 검증**

- [ ] 빌드 경고 0, 디버그 레이어 경고 0
- [ ] 기존 샷 21장, 투과 15장이 이전과 바이트 동일
- [ ] 복원 수심 vs 정답지: 테스트 샷 5개에서 오차 `max(1cm, 1%)` 초과 픽셀 0 (하늘·화면 밖 제외, 제외 수 보고)
- [ ] 시선 경로 ≥ 수직 수심 (모든 물 픽셀), 위에서 내려다본 샷에서 둘이 거의 같음
- [ ] 같은 받침대를 낮은 각도로 볼수록 경로가 길어짐 (`lab_top` vs `lab_low` 수치)

**사용자 검증**

- [ ] 정답지(`Water depth (WP, m)`)와 복원(`Water depth (from depth buffer)`)이 같은 색으로 보인다
- [ ] 시선 경로 화면에서 카메라를 낮추면 같은 받침대 색이 깊은 쪽으로 바뀐다
- [ ] 수직 수심과 시선 경로의 차이, 4단계가 왜 시선 경로를 쓰는지 설명할 수 있다

## 5. Notes

- 언리얼 대응: 머티리얼의 `SceneDepth` → `WorldPosition` 복원, Single Layer Water의 물속 경로 계산이 같은 일을 한다.
