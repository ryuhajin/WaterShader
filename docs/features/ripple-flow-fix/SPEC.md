# ripple-flow-fix

> Branch: `feature/ripple-flow-fix` · Status: done · Updated: 2026-10-04

## 1. Goal / Visual Target

- **한 문장 요약:** 노멀맵 레이어에 "Align to wind"를 켜도, 무늬가 Water 창 Flow direction 다이얼이 가리키는 방향으로 흐르게 한다.
- **문제:**
  - 지금 셰이더는 `uv = Rotate(uv × scale) + scroll × t`로, 회전한 **뒤에** 스크롤을 더한다.
  - 그러면 스크롤이 회전된 무늬 좌표계 안에서 적용된다. 월드에서 무늬는 `−R(a) × scroll` 방향, 즉 다이얼 방향에서 정렬 각도 a만큼 돌아간 방향으로 흐른다.
  - 바로 위 주석("the scroll stays in plane uv, so Flow direction keeps its meaning")의 의도와 다르다.
  - 예: preset-themes의 Sunset 레이어 A는 a = −49°라, 다이얼과 49° 어긋나게 흐른다.
- **어느 쪽이 틀렸나:** 다이얼(`FlowControls`)과 회전 수식(`RotateRippleUv` / `RotateRippleSlope`)은 맞다. 틀린 건 셰이더에서 스크롤을 더하는 순서다.
  - 다이얼 쪽을 맞추면, 바람 방향을 바꿀 때마다 같은 저장값의 실제 흐름 방향이 함께 돌아간다. 그래서 셰이더를 고친다.
- **시각 목표 키워드:** 바람 정렬한 물결이 바람 방향으로 흐름
- **스코프 가드 — 절대로 만들지 않을 것:**
  - UI, 프리셋 포맷, 저장값 의미 변경 없음 (`normalScroll`은 계속 평면 UV 속도)
  - 정렬을 끈 레이어의 렌더 결과 변경 없음

## 2. HLSL 접근법 / 수식

```
지금:  p = R(−a)·(uv·s) + v·t        → 월드 드리프트 = −R(a)·v
수정:  p = R(−a)·(uv·s + v·t)        → 월드 드리프트 = −v   (다이얼과 일치)
```

```hlsl
float2 uv1 = RotateRippleUv(input.uv * normalScale + g_NormalScroll.xy * time, g_NormalRotation.xy);
float2 uv2 = RotateRippleUv(input.uv * normalScale * detailScale + g_NormalScroll.zw * time, g_NormalRotation.zw);
```

- 스크롤 속도의 단위는 그대로다. 레이어 B도 지금처럼 `detailScale`을 곱한 뒤 uv 단위로 더한다.
- 회전이 (1, 0)이면 수학적으로 같다. 다만 fma 묶는 순서가 바뀌어 반올림 수준 차이는 남는다(NOTES).

## 3. Inputs / Outputs

| 종류 | 이름 | 형식 | 범위 / 비고 |
|---|---|---|---|
| CBuffer | `g_NormalScroll` | float4 | 변경 없음. 평면 UV 속도 (A = xy, B = zw) |
| CBuffer | `g_NormalRotation` | float4 | 변경 없음. 레이어별 (cos a, sin a) |
| Output | `uv1`, `uv2` | float2 | 스크롤을 회전 전에 더함 |

**의존하는 다른 feature:** `wave-far-normals`(Align to wind), `ui-panels`(Flow direction 다이얼)

## 4. Acceptance Criteria / Test Plan

- [x] HLSL 컴파일 성공, Debug 빌드 경고 0
- [x] 정렬 OFF 회귀: 기본 캡처 18장 — 비트 동일은 아님(fma 순서에 따른 반올림, 최대 9/255, 평균 ≤ 0.041/255). NOTES 참고
- [x] 정렬 ON(`--align-ripples on`): 시간을 두 번 다르게 잡아 무늬 이동 방향을 재고, 다이얼 방향과 일치하는지 확인 (수정 전은 a만큼 어긋남)
- [x] NOTES: 원인, 수식, before/after
