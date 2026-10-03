# wave-far-normals

> Branch: `feature/wave-far-normals` (from `feature/wave-macro`) · Status: done · Updated: 2026-10-04

## 1. Goal / Visual Target

- **한 문장 요약:** 정점 셰이더가 거리 LOD로 Gerstner 파도를 지운 원경에서도, 같은 4개 파도의 기울기(노멀)를 픽셀 셰이더에서 계산해 **조명상의 파도 마루가 바람 방향 그대로 수평선까지 이어지게** 한다.
- **문제:**
  - 정점 셰이더는 파도마다 파장 × 8 ~ 14 거리에서 높이를 페이드한다(정점 간격이 파장보다 넓어지면 깜빡이므로).
  - 이때 높이와 함께 **파도 노멀도 사라진다**. 기본 크기에서는 약 20 유닛 너머가 평평한 면 + 노멀맵만 남는다.
  - 노멀맵 무늬 방향은 텍스처가 정하고, 근경 파도 방향은 Wind direction이 정한다. 그래서 근경과 원경의 줄무늬가 X자로 엇갈려 보인다.
- **시각 목표 키워드:** 근경-원경 연속성, 바람 방향 일관성, 수평선 깜빡임 없음
- **스코프 가드 — 절대로 만들지 않을 것:**
  - 원경 메시 변위(높이) 복원, 테셀레이션, 그리드 해상도 변경
  - 근경 렌더 변경 (정점이 지운 만큼만 픽셀이 채움)
  - (1차) cbuffer 레이아웃 변경 — 기존 미사용 `g_SurfaceParams.w` 사용. 2차에서 `g_NormalRotation`(C), `g_FarParams`(Far spread)를 추가함
  - 노멀맵 텍스처 수정 — 조명 어긋남의 원인은 텍스처가 아니라 오션 그리드 UV였다(아래 UV 수정)
- **2차 범위 (사용자 테스트 이후 추가):** step1이 원경에 마름모 격자를 만들어 아래 단계를 더함. 상세는 NOTES(맨 위 요약).
  - **B. 기울기 분산 → 거칠기:** 픽셀이 해상하지 못하는 파도는 마루 대신 분산 σ²로 바꿔 글린트·스펙큘러를 넓힌다. 하늘 반사 흐림도 넣었으나 step4에서 제거했다.
  - **C. 노멀맵 바람 정렬:** 레이어별 `Align to wind`로 노멀맵 무늬를 Wave 1 방향에 맞춰 회전한다(프리셋 저장, 옛 프리셋 OFF).
  - **UV 수정:** 오션 그리드 UV를 OBJ 평면과 같게(v = +Z) 맞췄다. 오션에서 노멀맵 조명이 뒤집혀 있던 버그.
  - **step4:** 하늘 반사 흐림 제거(해를 등진 원경의 반사 형상 복구), 해를 등진 캡처 샷 `ocean_away` 추가.
  - **Far spread:** Light 창에서 원경 글린트 확산 배율을 조절(0~3, 1 = 물리값).

## 2. HLSL 접근법 / 수식 / 의사코드

**핵심 함수:** `smoothstep`, `ddx`, `ddy`, `sin`, `cos`, `dot`

파도 i의 노멀 기여는 페이드 가중치 w에 선형이다. VS에서는 높이와 Q가 모두 fade를 포함하므로 `q·kA = steepness·fade / 4`가 된다.

```
contribution_i(w) = ( −D.x·k·A·w·cos θ ,  −(steepness·w / 4)·sin θ ,  −D.y·k·A·w·cos θ )
θ = k(D·restXZ) − k·speed·t,   k = 2π/λ
```

- **VS:** 지금처럼 `w = vsFade_i = 1 − smoothstep(8λ, 14λ, 거리)`
- **PS:** 나머지 `w = (1 − vsFade_i) · pixelFade_i`
- **pixelFade:** 한 픽셀이 파도 진행 방향으로 덮는 파장 수 `footprint = max(|ddx(xz)·D|, |ddy(xz)·D|) / λ`
  - `pixelFade = 1 − smoothstep(0.125, 0.25, footprint)` → 파장당 8픽셀부터 줄이기 시작해 4픽셀에서 0 (2px 나이퀴스트 기준은 수평선 moire로 기각, NOTES 참고)
- **위상:** 변위 전 local 위치로 계산한다(VS와 같은 기준). VS → PS로 `restPosLocal`을 넘긴다.

```hlsl
// PS
float2 dx = ddx(rest.xz), dy = ddy(rest.xz);           // 루프 밖에서 한 번
for each wave:
    float vsFade = WaveVertexFade(λ, viewDistance);
    float footprint = max(abs(dot(dx, D)), abs(dot(dy, D))) / λ;
    float w = (1 - vsFade) * (1 - smoothstep(0.125, 0.25, footprint));
    delta.xz -= D * k * A * w * cos(θ);
    delta.y  -= steepness * w / WAVE_COUNT * sin(θ);
baseNormalWS = normalize(input.normalWS + farOn * mul(delta, (float3x3)g_World));
// 이후 노멀맵 TBN 결합은 그대로
```

**참고 자료:**

- GPU Gems 1, Ch.1 "Effective Water Simulation from Physical Models" — https://developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-1-effective-water-simulation-physical-models

## 3. Inputs / Outputs

| 종류 | 이름 | 형식 | 범위 / 비고 |
|---|---|---|---|
| VS→PS | `restPosLocal` (TEXCOORD2) | float3 | 변위 전 local 위치 |
| CBuffer | `g_SurfaceParams.w` | float | far wave normals 0/1 (기존 미사용 칸) |
| CBuffer | `g_Waves[4]` | struct | 기존 그대로 |
| ImGui | Far waves (lighting only past the mesh fade) | checkbox | 기본 ON, 프리셋에 저장하지 않음 |
| ImGui | Debug Mode 6 "Wave LOD" | combo | R = 정점 파도, G = 픽셀 노멀, B = 거칠기 가중치 |
| CLI | `--far-waves off` | flag | 회귀 캡처용 |
| Output | `baseNormalWS` | float3 | 노멀맵 TBN의 기준 노멀 |
| 2차 B | σ² (픽셀 셰이더 내부) | float | 글린트·스펙큘러 지수 `n/(1+n·σ²·FarSpread)` (하늘 반사 흐림은 step4에서 제거) |
| 2차 C | `g_NormalRotation` | float4 | 레이어 A/B 회전 cos/sin |
| 2차 C | Align to wind (레이어 A/B) | checkbox | 프리셋 키 `rippleAlignA/B`, CLI `--align-ripples on` |
| 2차 C | `kNormalMaps[].rippleAxisDeg` | float | `tools/measure_normal_orientation.ps1`로 측정 |
| CLI | `--preset-file <path>` | flag | 캡처를 고정 값(`capture_presets.txt`)으로 찍기 |
| Far spread | `g_FarParams.x` | float | Light 창 `Far spread` 0~3 (1 = 물리값), 프리셋 키 `farGlintSpread`(없으면 1) |
| UV 수정 | `Model::InitializeGrid` uv | float2 | u = x·0.5 + 0.5, v = z·0.5 + 0.5 (OBJ 평면과 동일) |
| Capture | `ocean_away` | 고정 샷 | 해(yaw 34.5°)를 등진 저각. 고정 샷은 6개 |

**의존하는 다른 feature:** `water-polish`(Gerstner 4파 + 거리 LOD + ocean 그리드), `wave-macro`(Simple 파도 UI).

## 4. Acceptance Criteria / Test Plan

- [x] Debug/Release 빌드 경고 0, 셰이더 컴파일 경고 0, `WaveMacroTest` 통과
- [x] `--far-waves off` 캡처 15장이 변경 전과 픽셀 차이 0 (VS 리팩터 + OFF 경로 동일)
- [x] ON 캡처: 차이가 원경에 집중되고 근경은 그대로
- [x] Debug 6으로 근경(빨강)/원경(초록)/수평선(검정) 구역 확인
- [x] GPU 시간 증가 측정·기록
- [x] 사용자 확인 1: 줄무늬 방향은 이어지나 마름모 격자 → 2차 범위(B, C) 추가
- [x] before/after 비교 + NOTES
- [x] B: 거칠기 구간 Debug 6 파랑으로 확인, OFF 회귀 최대 10 / 평균 ≤ 0.05 (컴파일 차이, NOTES)
- [x] C: top 샷 측정으로 회전량 검증(예측과 1° 이내), 플래그 없으면 step2와 동일(같은 컴파일 차이)
- [x] 단계별 비교 시트(before / step1 / step2 / step3), 수평선 크롭, GPU 시간
- [x] 사용자 확인 2: 해를 등진 원경 반사가 뭉개짐 → 흐림 제거(step4). 이후 격자 해소와 수평선 지글거림 없음을 확인
- [x] UV 수정: 벤치 평면 캡처 픽셀 차이 0, 오션만 변경. 모든 노멀맵의 slope 축과 pattern 축 일치
- [x] Far spread: 기본값 1에서 변경 전과 동일(Sunset/Tropical 픽셀 차이 0), 수평선 태양 데모
