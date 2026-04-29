# scene-lighting — 작업 메모

## 2026-04-30 · cbuffer 분리 — World/View/Projection

기존 `g_MVP` 한 개 대신 `g_World` / `g_View` / `g_Projection` 세 개로 분리.

**이유:**
- normal을 world-space로 변환하려면 `g_World`만 필요 (view·projection은 normal에 부적절)
- world-space에서 라이팅 계산해야 light direction 의미가 직관적 (객체 회전과 무관하게 라이트 방향 고정)
- 후속 fresnel(`dot(N, V)`)에서 view 벡터 계산 시 카메라 위치도 world-space에 있음이 자연스러움
- 쉐이더 디버깅이 쉬워짐 — 각 단계 행렬을 따로 보고 검증 가능

비용: cbuffer가 64 byte (행렬 1개) → 192 byte (3개) + 라이트/시간 → 약 256 byte. 한 cbuffer에 정렬 무관 충분.

## 2026-04-30 · light direction 좌표계 결정

ImGui slider는 yaw(0~360)/pitch(-90~90)로 조절. 코드에서 unit vec로 변환할 때 의미를 한 번에 정해두지 않으면 헷갈림 — 본 프로젝트의 컨벤션:

- `g_LightDirection.xyz`는 **"광원→표면" 방향** = 빛이 진행하는 방향. PS에서 `L = -g_LightDirection`로 뒤집어 "표면→광원" 벡터 만든 후 `dot(N, L)`.
- yaw 0 → 광원 위치가 +Z. 빛은 -Z 방향으로 진행.
- pitch +90 → 광원이 머리 위. 빛은 -Y 방향(아래)로. plane 위면(+Y normal)이 밝음.
- pitch -90 → 광원이 발 밑. 빛은 +Y 방향(위)로. plane 위면이 어두움.

처음 SPEC에 "pitch=-90이 머리 위"라고 잘못 적어서 검증 단계 후 정정함. 좌표계 정의는 SPEC 본문 의사코드의 주석에도 명시.

## 2026-04-30 · g_LightColor.a에 intensity 패킹

`g_LightColor`를 `float4`로 두고 rgb=색상, a=intensity로 합침.

**이유:**
- cbuffer 슬롯/byte 절약 — 별도 `float4 g_LightIntensity` 두면 padding 포함 16-byte 추가
- "라이트 컬러"라는 단일 개념으로 묶이는 게 자연스러움 (HDR 라이팅에서도 보통 RGB scale로 표현)
- ImGui는 둘로 분리(R/G/B 슬라이더 + Intensity 슬라이더), C++/HLSL에서 `(rgb * a)` 한 번에 곱하면 끝

비용: rgb와 intensity를 따로 디버깅 출력하려면 a 분리 필요. 현재 단계에선 무관.

## 2026-04-30 · ImGui ID 충돌 — `##suffix`로 분리

Tint Color 섹션과 Lighting 섹션 모두 R/G/B SliderInt 사용. ImGui는 label로 widget ID 결정하므로 같은 label "R"이 두 곳에 있으면 두 슬라이더가 같은 widget으로 묶여 한쪽 입력이 다른 쪽으로 전달됨.

**해결:** label에 `##suffix`로 표시명은 같게, ID는 분리.

```cpp
ImGui::SliderInt("R", &tintR, 0, 255);            // Tint section
ImGui::SliderInt("R##light", &lightR, 0, 255);    // Lighting section
```

`##` 뒤 부분은 화면에 안 보이고 ID에만 쓰임. ImGui의 표준 패턴.

**교훈:** SeparatorText로 카테고리를 분리해도 ImGui 내부적으로 라벨 충돌은 자동 해결되지 않음. 같은 라벨을 두 번 이상 쓸 가능성이 있으면 항상 ##suffix로 ID를 분리.

## 2026-04-30 · g_Time 누적 — wraparound 미처리

`Graphics::Frame(deltaTime, input)`에서 `elapsedTime_ += deltaTime`로 단순 누적. wraparound(예: 일정 시간 후 0으로 리셋)는 처리하지 않음.

**이유:**
- float 정밀도: 1초 = 1.0f, 1시간 = 3600.0f, 100시간 = 360,000.0f. float 가수부 23비트 ≈ 7자리 정밀도라 100시간까지는 0.01초 단위 정확. 셰이더 작업 세션 한 번에 그만큼 안 돎.
- UV scroll 등 fract 기반 효과는 자연스럽게 wrap됨 (HLSL `frac()`).
- 정말 길게 돌리면 정밀도 떨어지지만 데모 용도엔 무관.

후속 water-base에서 만약 정밀도 문제가 보이면 `fmodf(elapsedTime_, 1000.0f)` 등으로 wrap 추가 검토.
