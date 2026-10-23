# Depth Reconstruct — NOTES

> 3단계 진행 기록. 상태: **에이전트 검증 완료, 사용자 승인 대기**

## 1. 개념 요약

- 깊이 버퍼 값은 대략 `1 − 0.1 ÷ 거리`라서 1m~1000m가 0.9~1.0에 몰려 있다. 그대로는 미터로 못 쓴다.
- 물 픽셀은 **화면 위치 + 깊이 값**을 안다. 그릴 때 거친 변환(카메라 → 투영)의 **역행렬**을 곱하고 `w`로 나누면 물 아래 표면의 월드 위치가 돌아온다.
- 거리는 두 가지다.
  - **수직 수심** = 수면 높이 − 표면 높이. 1단계 정답지와 같은 값이라 채점에 쓴다.
  - **시선 경로** = 수면 점 → 표면 거리. 비스듬히 볼수록 길다. **4단계 감쇠가 쓸 값.**
- 하늘이 보이는 픽셀(깊이 1.0)은 꾸며내지 않고 "최대 광학 거리"(기본 50m)로 처리한다.

## 2. 만든 것

| 파일 | 내용 |
|---|---|
| `shaders/DepthBands.hlsli` | 띠 색(`DepthBandColor`), 1m 등고선, 미터를 색에 담는 `EncodeMeters`. 정답지와 복원이 같이 씀 |
| `shaders/scene.hlsl` | 띠 색을 공유 파일에서 가져옴. Scene Debug 2 = 수심 숫자(정답지) |
| `shaders/Common.hlsli` | cbuffer에 `g_InvViewProjection`, `g_SceneParams.y`(최대 광학 거리) |
| `shaders/PixelShader.hlsl` | `SCENE_INPUTS`: 위치 복원, 수직 수심, 시선 경로, 디버그 10~13 |
| `src/ColorShader.*`, `src/Graphics.*` | 역행렬·최대 거리 전달, UI(Max underwater distance, 디버그 이름), 숫자 정답지에서는 하늘 숨김 |
| `tools/compare_depth.ps1` | 숫자 PNG 비교: 정답지 vs 복원(오차), 수심 vs 경로(경로 ≥ 수심, 깊이별 평균 경로) |
| `CMakeLists.txt` | `DepthBands.hlsli` 빌드 후 복사 |

**디버그 이름**

| 어디 | 번호 | 이름 | 내용 |
|---|---|---|---|
| Scene Debug | 1 | Water depth (WP, m) | 정답지: 월드 위치로 잰 수심 (1단계) |
| Scene Debug | 2 | Water depth numeric (WP) | 정답지를 숫자로 |
| Debug Mode | 10 | Water depth (from depth buffer) | 복원 수심, 정답지와 같은 띠 색 |
| Debug Mode | 11 | Underwater view path | 시선 경로, 같은 띠 색 (자홍 = 물 아래 표면 없음) |
| Debug Mode | 12 / 13 | ... numeric (compare tool) | 복원 수심 / 시선 경로를 숫자로 |

**숫자 인코딩:** R = 미터의 소수부(1칸 ≈ 4mm), G = 정수 미터 × 4 (0~63m), B = 표지(128 측정값, 64 수면 위, 255 표면 없음). 표지는 8비트 값에 정확히 맞춰 PNG 왕복에도 변하지 않는다.

## 3. 에이전트 검증 (2026-10-24)

### 복원 수심 vs 정답지 (프리셋 Basic, PNG)

| 샷 | 비교 픽셀 | 최대 오차 | 평균 오차 | 허용 초과 | 정답지엔 있고 복원엔 없음 |
|---|---|---|---|---|---|
| lab_top | 305,355 | 0.0039m | 0.0000m | 0 | 0 |
| lab_stairs | 499,443 | 0.0039m | 0.0000m | 0 | 1 |
| lab_low | 362,280 | 0.0039m | 0.0000m | 0 | 0 |
| lab_pillar | 643,494 | 0.0039m | 0.0000m | 0 | 0 |
| front_rails | 498,172 | 0.0039m | 0.0000m | 0 | 0 |

- 허용 오차는 `max(1cm, 1%)`. 최대 오차 0.0039m는 **숫자 인코딩 한 칸**(1/255m)이다. 복원 자체의 오차는 그보다 작다.
- `lab_stairs` 1픽셀: 정답지에서는 수면 아래 표면인데 복원에서는 물이 그려지지 않은 픽셀. 원인은 확인하지 않았다 (물 영역 가장자리 경계 픽셀로 추정).

### 시선 경로 (같은 깊이를 다른 각도에서)

| 깊이 | lab_top (위) | lab_stairs (사선) | lab_pillar (근접) | lab_low (낮은 시점) |
|---|---|---|---|---|
| 0.2m 받침대 | 0.21m | 0.72m | 1.39m | 4.43m |
| 1m 받침대 | 1.03m | 3.09m | 5.87m | **9.83m** |
| 3m 받침대 | 3.08m | 7.23m | 8.88m | 12.85m |
| 6m 받침대 | 6.27m | 11.66m | 11.25m | 15.99m |

- 모든 샷에서 **경로가 수심보다 짧은 픽셀 0**. 위에서 보면 경로 ÷ 수심 = 1.06, 낮은 시점에서는 3.16.
- 경로 최대값 63.99m는 숫자 인코딩의 상한(64m)에 걸린 것이다 (계단 샷에서 바닥을 아주 비스듬히 보는 픽셀).

### 그 밖

| 항목 | 결과 |
|---|---|
| Debug·Release 빌드 | 경고·오류 0 |
| 기존 샷 21장 vs 0단계 기준 | 바이트 동일 |
| 투과 물 켬 vs 물 끔 (15장 PNG) | 바이트 동일, 2단계 투과 결과와도 동일 |
| 디버그 레이어 (Debug: 복원 수심·경로·정답지 캡처) | 경고·오류 0건 |

**과정에서 고친 것**

- 평평한 바닥(정확히 8m)과 1m 받침대에 가는 가로줄이 생겼다. 깊이 버퍼에서 되찾은 값에 픽셀마다 0.5mm 정도 잡음이 있어서 "평평한 면에는 등고선 없음" 판정(변화 0)이 깨졌다. 판정 기준을 픽셀당 2mm로 올렸다.
- 물체 윤곽을 따라 점선이 생겼다. 경계에서는 픽셀 하나 사이에 수심이 몇 m씩 바뀌어 등고선으로 잘못 그려졌다. 픽셀당 0.5m 넘게 바뀌는 곳은 경사가 아니라 경계로 보고 선을 뺐다.

캡처 위치 (로컬): `C:\Users\da171\ws-capture\depth-reconstruct\captures\`
`key`, `recon_depth`, `recon_path`, `view_key`, `view_depth`, `view_path`, `pass_on`, `pass_off`, `regress`, `dbg_*`

비교 명령:

```powershell
.\tools\compare_depth.ps1 -A C:\Users\da171\ws-capture\depth-reconstruct\captures\key -B C:\Users\da171\ws-capture\depth-reconstruct\captures\recon_depth -Mode depth
.\tools\compare_depth.ps1 -A C:\Users\da171\ws-capture\depth-reconstruct\captures\recon_depth -B C:\Users\da171\ws-capture\depth-reconstruct\captures\recon_path -Mode path
```

## 4. 사용자 검증 안내

**실행:** `.\build\vs2022\Debug\WaterShader.exe` → `1` 키 → Scene = **Water Lab**

| 순서 | 조작 | 정상 | 실패 징후 |
|---|---|---|---|
| 1 | Scene Debug = **Water depth (WP, m)** (정답지), 카메라 **lab_pillar** | 받침대·기둥이 수심별 색 | — |
| 2 | Scene Debug = Render, Debug Mode = **Water depth (from depth buffer)** | 1번과 같은 색 띠 (음영만 없음) | 색이 다르거나, 바닥에 줄무늬 |
| 3 | Debug Mode = **Underwater view path** | 같은 받침대가 1번보다 깊은 쪽 색. 바닥에 "같은 경로" 곡선 | 1번보다 얕은 색이 나옴 |
| 4 | 카메라 **lab_top** ↔ **lab_low** 전환 (3번 상태) | 위에서는 2번과 거의 같고, 낮게 보면 거의 전부 보라색 | 각도에 따라 안 바뀜 |
| 5 | 2번 상태에서 Q/E로 카메라 높이 바꾸기 | 색이 바뀌지 않음 (수심은 카메라와 무관) | 카메라 높이에 따라 색이 바뀜 |
| 6 | 화면 가장자리 자홍색 | 물 아래에 바닥이 없는 곳(수면이 바닥보다 가까워 화면에서 더 넓음) | 받침대·바닥 위에 자홍색 |

**확인 질문**

- [ ] 정답지와 복원 수심이 같은 색으로 보인다
- [ ] 시선 경로 화면에서 카메라를 낮추면 같은 받침대가 깊은 쪽 색으로 바뀐다
- [ ] 수직 수심과 시선 경로의 차이, 4단계가 왜 시선 경로를 쓰는지 설명할 수 있다
