# <Feature Name>

> Branch: `feature/<name>` · Status: draft | in-progress | done · Updated: YYYY-MM-DD

## 1. Goal / Visual Target

- **한 문장 요약:** 이 기능이 시각적으로 무엇을 만드는가.
- **참고 이미지/영상:**
  - <설명> — <URL 또는 references/파일명>
  - <설명> — <URL 또는 references/파일명>
- **시각 목표 키워드:** (예: 청량함, 깊이감, 잔잔함, 햇빛 반사)
- **스코프 가드 — 절대로 만들지 않을 것:**
  - <범위 밖 항목 1>
  - <범위 밖 항목 2>

## 2. HLSL 접근법 / 수식 / 의사코드

**핵심 함수:** `lerp`, `smoothstep`, `frac`, `dot`, `pow`, `reflect` 등 사용 예정인 것만 나열.

**핵심 수식:**

```
fresnel = pow(1 - saturate(dot(N, V)), power)
```

**단계별 의사코드:**

```hlsl
// 1. <단계 1 설명>
float2 uv = ...;

// 2. <단계 2 설명>
float3 normal = ...;

// 3. <단계 3 설명>
float3 color = ...;

return float4(color, alpha);
```

**참고 자료:**

- <블로그/논문/튜토리얼 제목> — <URL>
- <블로그/논문/튜토리얼 제목> — <URL>

## 3. Inputs / Outputs

| 종류    | 이름             | 형식        | 범위 / 비고                  |
|---------|------------------|-------------|------------------------------|
| Texture | _NormalMapA      | 2D RGBA     | tiling 1~8                   |
| Texture | _FoamMask        | 2D R        | grayscale, threshold용       |
| CBuffer | g_FlowSpeed      | float2      | 0..2                         |
| CBuffer | g_FresnelPower   | float       | 1..8                         |
| ImGui   | Foam Threshold   | slider      | 0..1, 기본 0.6               |
| ImGui   | Color Shallow    | colorpicker | linear RGB                   |
| Output  | RT0.rgb          | float3      | linear color                 |
| Output  | RT0.a            | float       | 0..1, alpha blend            |

**의존하는 다른 feature:** (예: `feature/water-base` 의 normal 출력 사용)

## 4. Acceptance Criteria / Test Plan

- [ ] HLSL 컴파일 성공, 경고 0
- [ ] 데모 씬에서 시각적 목표(섹션 1) 도달 — 스크린샷 `references/result-YYYYMMDD.png`
- [ ] ImGui 파라미터 전 구간에서 깨짐/NaN/검은 픽셀 없음
- [ ] 다른 기능과의 통합 회귀 없음 — 체크할 기능: `<feature-name>`, `<feature-name>`
- [ ] (선택) 결과 GIF 또는 짧은 영상 1개 `references/` 에 저장
- [ ] (선택) 직접 제작한 텍스처/마스크 사용 — 파일 경로: `<path>`

## 5. Notes (선택)

자유롭게 메모. 길어지면 같은 폴더의 `NOTES.md`로 분리.
