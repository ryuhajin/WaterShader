# water-polish

> Branch: `feature/water-polish` · Status: done · Updated: 2026-09-29

## 1. Goal / Visual Target

- **한 문장 요약:** 포트폴리오 06 페이지 캡처가 "색칠된 천"처럼 보이는 문제를 고쳐, 태양 반사광이 반짝이고 하늘을 비추는 "물"로 읽히게 만든다.
- **시각 목표 키워드:** sun glitter path, sky reflection, deep water body color, crisp ripples.
- **Before/After 기록:** 셰이더를 건드리기 전에 고정 카메라·고정 시간으로 `captures/before/`를 먼저 남기고, 단계마다 같은 조건으로 `captures/<step>/`에 저장. 과정은 `NOTES.md`.
- **스코프 가드 — 만들지 않을 것:**
  - 스크린 공간 반사(SSR), 굴절(refraction) — depth/scene color SRV 필요, 별도 feature
  - 풀 HDR 렌더 타깃 + 블룸 post-process 파이프라인 — 셰이더 내 톤매핑으로 대체
  - FFT 해양 — Gerstner 합으로 충분

## 2. 원인 → 접근법

| 증상 | 원인 | 접근 |
|---|---|---|
| 반사광 없음 | `lightDir = (sin y·cos p, −sin p, …)`, 셰이더는 `−lightDir`를 광원 방향으로 사용 → 프리셋의 음수 pitch는 **태양이 수면 아래**. 스카이박스 태양(yaw≈33°, 고도≈17~24°)과도 무관 | 태양 고도(elevation, +=수평선 위)로 재정의하고 스카이박스 태양에 정렬 |
| 1.0에서 잘리는 하이라이트 | LDR 합산 후 그대로 출력 | 반사벡터 기반 HDR sun glint + 셰이더 내 톤매핑 |
| 페인트 색판 | Fresnel `F0 = 0.5` 고정, 채도 높은 Facing/Grazing 색 | Schlick `F0 ≈ 0.02`(파라미터), 어두운 deep 색, 노을은 하늘 반사로 |
| 잔물결 뭉개짐 | 두 레이어 같은 스케일, 강도 파라미터 없음, `MaxAnisotropy = 1` | 레이어 B 스케일 배수 + normal strength + 거리 감쇠, anisotropic 16x |
| 텐트 모양 | 2×2 plane 위 파장 2짜리 sine 2개 | Gerstner 4개, 짧은 파장/작은 진폭 |
| 초원 위의 판 | 2×2 plane, 저고도 태양의 반사 지점이 판 밖 | 수평선까지 이어지는 ocean grid + 파도 거리 LOD |

**핵심 수식:**

```
F        = F0 + (1 − F0)·(1 − N·V)^p                  // Schlick
glint    = pow(saturate(dot(R, L)), glintPower) · glintIntensity   // R = reflect(−V, N)
color    = lerp(body, sky, F) + glint·lightColor
out      = ACES(color · exposure)
Gerstner : P.xz += Q·A·D·cos(θ), P.y += A·sin(θ),  θ = k(D·xz) − ωt
```

## 3. Inputs / Outputs

| 종류   | 이름                        | 비고                                   |
|--------|-----------------------------|----------------------------------------|
| CBuffer| `g_SpecularParams.zw`       | glint power, glint intensity           |
| CBuffer| `g_SurfaceParams`           | F0, normal strength, layer B scale, exposure |
| ImGui  | Sun Elevation / Yaw         | 고도 + = 수평선 위                      |
| Tool   | `--capture <label>`         | 프리셋 × 고정 샷을 PNG로 저장 후 종료   |

## 4. Acceptance Criteria

- [x] `captures/before/` 저장 (셰이더 변경 전)
- [x] 단계별 캡처 + `NOTES.md`에 변경/이유/비교 기록
- [x] 저각 샷에서 태양 방향으로 반짝이는 glint 경로가 보임 (`step6_ocean_grid/sunset_ocean_sunward.jpg`)
- [x] 정면 샷에서 물 색은 어둡고, 수평 쪽은 하늘 반사가 지배
- [x] 기존 Debug Mode 1~4 정상 + Mode 5(lighting terms) 추가, NaN/검은 픽셀 없음 (`final_breakdown/`)
- [x] 프리셋 3종 재조정 후 저장 (v3)
