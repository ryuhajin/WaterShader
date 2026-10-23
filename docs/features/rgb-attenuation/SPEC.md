# RGB Attenuation — 4단계 RGB 감쇠

> Branch: `feature/rgb-attenuation` · Status: in-progress · Updated: 2026-10-24
> waterfront 업그레이드 4단계. 앞 단계: [../depth-reconstruct/NOTES.md](../depth-reconstruct/NOTES.md)

## 1. Goal / Visual Target

- **한 문장 요약:** 물 아래 장면 색을 **시선 경로가 길수록 어둡게**, 그리고 **빨강·초록·파랑마다 다른 빠르기로** 줄인다.
- 결과: 얕은 받침대는 거의 원래 색, 깊은 받침대일수록 어두워지면서 빨강이 먼저 빠져 청록 쪽으로 기운다. 같은 받침대도 낮은 각도에서 보면 더 어둡다.
- 이 단계는 **"빛이 빠져나가는 것"만** 다룬다. 물이 스스로 띠는 색(산란)이 아직 없으므로 아주 깊은 곳과 바닥이 없는 곳은 **검게** 보이는 것이 정상이다 (5단계에서 물색을 더함).
- **스코프 가드 — 만들지 않을 것:**
  - 산란색·물색 더하기 (5단계)
  - 반사 (6단계), 굴절 (8단계)
  - 수직 수심으로 감쇠하기 (감쇠는 빛이 실제로 지나간 **시선 경로**로 한다)

## 2. HLSL 접근법 / 수식 / 의사코드

**투과율** — 빛이 물을 지나고 남는 비율 (0 = 다 사라짐, 1 = 그대로). 채널마다 따로.

```text
투과율 = exp(− 소멸 계수 × 시선 경로)
물 아래 색 = 물 없는 장면 색 × 투과율
```

- **소멸 계수(σ, 단위 1/m)**: 1m 지날 때 빛이 얼마나 빨리 사라지는가. 0이면 아무것도 안 사라짐.
- **반감 거리** = 0.693 ÷ 소멸 계수: 빛이 절반 남는 거리. UI에 함께 표시한다 (아티스트가 읽기 쉬운 값).

```hlsl
float3 transmittance = exp(-g_Extinction.rgb * viewPath);   // viewPath: 3단계 시선 경로 (공기 중 거리 제외)
if (transmissionMode == 2) return float4(sceneColor * transmittance, 1.0);
```

- 바닥이 없는 픽셀(하늘)은 3단계대로 최대 광학 거리(기본 50m)를 쓴다 → 거의 0 → 검정.
- 소멸 계수 0이면 `exp(0) = 1`이라 **투과 모드와 바이트까지 같아야** 한다.

**참고 값 (순수한 물의 흡수, 대략):** 빨강(650nm) ≈ 0.35/m, 초록(550nm) ≈ 0.06/m, 파랑(450nm) ≈ 0.01/m — Pope & Fry 1997, "Absorption spectrum (380–700 nm) of pure water", Applied Optics 36(33). 아트 값은 이 비율을 출발점으로 조절한다.

**디버그 뷰**

| 번호 | 이름 | 내용 |
|---|---|---|
| 14 | Transmittance (RGB) | 투과율을 그대로 색으로 (흰색 = 다 통과, 검정 = 다 사라짐). 비교 도구 입력 |

## 3. Inputs / Outputs

| 종류 | 이름 | 형식 | 범위 / 비고 |
|---|---|---|---|
| CBuffer | `g_Extinction` | float4 | rgb = 소멸 계수 (1/m) |
| Preset | `extinctionR/G/B` | float | 키가 없는 예전 프리셋은 0 (효과 꺼짐) |
| ImGui (Water 창) | Extinction R / G / B | slider | 0 ~ 2 /m, 옆에 반감 거리(m) 표시 |
| ImGui (Water 창) | 버튼 | | `Zero`, `Pure water (ref.)` (위 참고 값) |
| ImGui (View 창) | Transmission | combo | Opaque / Pass-through / **Attenuation** (테스트 장면 기본값) |
| CLI | `--extinction r,g,b` | | 캡처용, 프리셋 값을 덮어씀 |
| 도구 | `tools/compare_depth.ps1 -Mode transmit` | | 시선 경로 숫자(13) + 투과율(14): 픽셀마다 `투과율 = exp(−계수 × 경로)` 확인 |

**의존하는 다른 feature:** `depth-reconstruct` (시선 경로), `scene-color-depth` (물 없는 장면 색)

## 4. Acceptance Criteria / Test Plan

**에이전트 검증**

- [ ] 빌드 경고 0, 디버그 레이어 경고 0
- [ ] 기존 샷 21장 바이트 동일, 소멸 계수 0의 감쇠 모드 == 투과 모드 (15장 바이트 동일)
- [ ] 모든 물 픽셀에서 측정 투과율 = `exp(−계수 × 경로)` (8비트 오차 안). 이것이 성립하면 `T(2d) = T(d)²`와 "경로가 길수록 단조 감소"가 따라온다
- [ ] 같은 받침대의 평균 투과율: `lab_top` > `lab_stairs` > `lab_low` (공기 중 거리가 아니라 물속 경로만 영향)
- [ ] 빨강만 계수를 준 캡처에서 초록·파랑 채널은 투과 모드와 같음

**사용자 검증**

- [ ] 계수를 0에서 올리면 깊은 받침대부터 어두워지고 빨강이 먼저 빠진다
- [ ] 같은 받침대가 위에서 볼 때보다 낮게 볼 때 더 어둡다
- [ ] 깊은 곳·바닥 없는 곳이 검게 되는 이유(산란이 아직 없음)를 설명할 수 있다

## 5. Notes

- 언리얼 대응: Single Layer Water의 **Absorption** 계수가 같은 역할 (13A단계에서 비교).
