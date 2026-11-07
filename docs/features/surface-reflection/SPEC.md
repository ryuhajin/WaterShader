# Surface Reflection — 6단계 반사 재연결

> Branch: `feature/surface-reflection` · Status: in-progress · Updated: 2026-11-02
> waterfront 업그레이드 6단계. 앞 단계: [../water-scatter/NOTES.md](../water-scatter/NOTES.md)

## 1. Goal / Visual Target

- **한 문장 요약:** 5단계에서 만든 "물속 색" 위에 **수면이 비추는 하늘과 태양**을 다시 얹는다. 둘의 몫은 Fresnel 비율 하나로 나눈다.
- 위에서 내려다보면 물속(계단·바닥)이 잘 보이고, 낮게 볼수록 하늘이 비쳐 물속이 가려진다.
- 수면에 잔물결(노멀맵)이 있으므로 반사가 일렁이고, 수면선 위·아래의 색이 달라져 **물이 시작되는 경계가 보이기 시작**한다.
- 반사 몫, 투과 몫, Fresnel 비율을 따로 보는 디버그 화면을 만든다.
- **스코프 가드 — 만들지 않을 것:**
  - 접촉 거품 (6B단계), 굴절 (8단계), 전주 같은 물체의 반사 (8B단계, 이번에는 하늘 큐브맵만 비친다)
  - 파도 (7단계). Water Lab 수면은 평평하고 노멀맵 잔물결만 있다.
  - 기존 바다 모드(테스트 장면 없음)의 화면 변경 — 아래 "결정 필요" 참고

## 2. HLSL 접근법 / 수식 / 의사코드

**지금 물 (불투명, 1~2차에 만든 것)**

```text
칠한 물색 = 시선 각도 2색(Facing / Grazing) × (환경광 + 햇빛 확산) + 블린-퐁 하이라이트
최종      = 칠한 물색 ↔ 하늘 반사 를 반사량(R)으로 섞음 + 태양 글린트 × R
```

물을 **페인트 칠한 판**으로 다룬다. 판 아래가 보이지 않으니, 물 아래 색을 2색과 확산광으로 흉내 냈다.

**6단계 물 (테스트 장면)**

```text
F         = Fresnel 비율 = F0 + (1 − F0) × (1 − 정면도)^곡선      // 정면 0.02, 수평에 가까울수록 1
R         = F × 반사 세기                                           // 반사 세기 1 = 물리값, 아트 조절용
물속 색   = 5단계 결과 (바닥 × 투과율 + 물색 몫)
반사 색   = 하늘 큐브맵(반사 방향) + 태양 글린트
최종      = 물속 색 × (1 − R) + 반사 색 × R
```

- **칠한 물색 경로 전체(2색·확산광·블린-퐁)를 테스트 장면에서 뺀다.** 물속 색이 그 자리를 대신한다. 남겨 두면 물색이 두 번 들어간다 (PLAN R6).
- 블린-퐁 하이라이트는 "칠한 판의 반짝임"이라 같이 뺀다. 태양 반사는 에너지 보존 글린트 하나만 남긴다 (태양이 두 번 비치지 않게).
- 빛이 수면에서 반사된 만큼 물속으로 덜 들어가므로 물속 색에 `(1 − R)`을 곱한다. 반사 + 투과 = 들어온 빛 전체.
- 노멀은 기존 노멀맵 잔물결을 그대로 쓴다. 반사 방향과 정면도가 잔물결마다 달라져 반사가 일렁인다.
- **반사 끔 토글**: 끄면 `R = 0` → 5단계 화면과 같다.

**디버그 뷰**

| 번호 | 이름 | 내용 |
|---|---|---|
| 17 | Fresnel (reflection amount) | R을 회색으로 (검정 = 반사 없음, 흰색 = 거울) |
| 18 | Reflection part only | 반사 색 × R (HDR) |
| 19 | Underwater part only | 물속 색 × (1 − R) (HDR) |

`18 + 19 = 최종`이어야 한다.

**참고 자료**

- Schlick, "An Inexpensive BRDF Model for Physically-based Rendering" (1994) — Fresnel 근사식
- 물의 정면 반사율 F0 ≈ 0.02 (굴절률 1.33: ((1.33 − 1)/(1.33 + 1))² ≈ 0.02)
- Tessendorf, "Simulating Ocean Water" (2001) — 반사·투과를 Fresnel로 나누는 물 셰이딩

## 3. Inputs / Outputs

| 종류 | 이름 | 형식 | 범위 / 비고 |
|---|---|---|---|
| CBuffer | 기존 `fresnelF0`, `fresnelPower`, `reflectionStrength`, 글린트 값 | float | 새 값 없음. 테스트 장면에서도 그대로 쓴다 |
| CBuffer | 반사 켬/끔 | float | `g_SceneParams.z` (0 끔 = 5단계 화면, 1 켬) |
| ImGui (View 창) | Water version | radio | Scene 목록 바로 위. Old(opaque, painted) / New(underwater). 바다·벤치는 Old 고정, 테스트 장면을 고르면 자동 New. Light·Water 창이 버전을 따라감. 카메라는 두 버전 공유 |
| ImGui (Water 창) | New: Underwater | combo, checkbox, slider | Transmission 하위 3모드, Surface reflection(기본 켬), Max underwater distance (View 창에서 옮김) |
| ImGui (Water 창) | Old: Painted Color | | Old일 때만 표시. New 섹션들은 New일 때만 표시 |
| CLI | `--reflection on\|off` | | 캡처용 |
| Debug Mode | 17 / 18 / 19 | | 위 표 |
| 도구 | `compare_depth.ps1 -Mode sum` 재사용 | | `반사 몫 + 물속 몫 = 최종` |

**의존하는 다른 feature:** `water-scatter` (물속 색), `hdr-env` (하늘 큐브맵·글린트), `water-polish` (Fresnel·노멀맵)

## 4. Acceptance Criteria / Test Plan

**에이전트 검증**

- [x] 빌드 경고 0, 디버그 레이어 경고 0, 기존 샷 21장 바이트 동일
- [x] 반사 끔 → 5단계(Scattering) 화면과 바이트 동일
- [x] 톤 곡선 끈 선형 화면에서 `반사 몫(18) + 물속 몫(19) = 최종` (8비트 반올림 안)
- [x] Fresnel 화면(17)의 평균: `lab_top` < `lab_stairs` < `lab_low` (낮게 볼수록 반사 증가). `lab_top` 중앙은 F0 근처
- [x] 테스트 장면에서 칠한 물색 값(Facing/Grazing 색)을 바꿔도 화면이 바뀌지 않음 (경로를 실제로 뺐는지 확인)

**사용자 검증**

- [ ] 위에서 보면 물속이 잘 보이고, 낮은 시점에서는 하늘 반사가 물속을 덮는다
- [ ] 반사 몫과 물속 몫을 디버그로 나눠 보고, 둘을 더하면 최종 화면이 되는 것을 설명할 수 있다
- [ ] 예전 불투명 물(2색 + 확산광)에서 무엇을 빼고 무엇으로 바꿨는지 설명할 수 있다

## 5. 결정 필요

- **기존 바다 모드도 바꿀지**: PLAN R6은 바다 모드(바닥 없음)도 "최대 광학 거리"로 같은 경로를 타게 하자고 했다. 바꾸면 기존 바다 21장이 모두 달라지고, 프리셋 3종(Basic/Sunset/Tropical)을 다시 튜닝해야 한다.
  - **결정 (사용자, 2026-11-02): 6단계는 테스트 장면만 바꾼다.** 기존 바다는 21장 바이트 동일로 지킨다. 바다 모드 전환은 11단계(적용 장면 완성)에서 수변 장면과 함께 판단한다.

## 6. Notes

- 언리얼 대응: Single Layer Water는 반사(Lumen/SSR/큐브맵)와 투과를 엔진이 Fresnel로 나눈다. 13B단계에서 대응을 확인한다.
