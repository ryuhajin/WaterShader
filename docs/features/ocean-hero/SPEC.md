# ocean-hero

> Branch: `feature/ocean-hero` · Status: in-progress · Updated: 2026-10-04

## 1. Goal / Visual Target

- **한 문장 요약:** 포트폴리오 PDF 메인용 고정 카메라 샷 `ocean_hero`를 추가한다. 하늘 약 1 : 물 약 3의 저공 사선 구도이고, Basic / Sunset / Tropical이 같은 위치를 공유하면서 각자의 태양과 색으로 보인다.
- **추가 목표:** 항공 컷의 근경 → 원경 전환을 다듬는 셰이더 기능 "Ocean Detail"을 넣는다. 기본은 꺼짐이며, 프리셋 값은 바꾸지 않는다.
- **원인 분석 (`ocean_aerial` + Basic):**
  - 중경의 가로 점선: 메시 페이드 이후 픽셀 구간에서 작은 파도 두 개(λ 1.4, 0.92)의 마루가 교차해 격자가 된다. Basic 바람(−105°)이 시선과 거의 평행해 이 격자가 가로 줄로 눕는다.
  - 원경의 거울 띠: 노멀맵이 mip에서 평평해져 하늘·나무가 또렷이 비친다. 대기 원근도 없다.
- **시각 목표 키워드:** 수평선이 위쪽 1/5, 오른쪽 글린트 길, 원경으로 갈수록 넓어지는 글리터와 옅어지는 수평선. 글린트 맺힘은 유지한다.
- **스코프 가드 — 절대로 만들지 않을 것:**
  - Basic / Sunset / Tropical 프리셋 값은 바꾸지 않는다. 기존 18장은 해시까지 같아야 한다.
  - 4번째 프리셋 테마는 만들지 않는다. hero는 프리셋이 아니라 카메라 샷이다(사용자 결정).
  - Gerstner wave 개수(4), 정점 셰이더, 상수버퍼의 wave 배열은 바꾸지 않는다.

## 2. 접근법

**고정 샷** — `kCaptureShots`에 추가:

| 이름 | 위치 | pitch / yaw | 화각 |
|---|---|---|---|
| `ocean_hero` | (−5.38, 6, −20.09) | 14 / 18 | 44° |

- 사용자가 준 참고 화면에서 역산했다: 물/언덕 경계(그리드 끝)가 위에서 21%, 언덕선 7%, 반사 띠 끝 36%, 글린트 중심이 가로 72%.
- `ocean_aerial`과 같은 지점에서 더 낮고 평평하게 본다.

**캡처 도구**

`--capture-presets`, `--capture-shots`(카메라 슬롯 `slot1`~`slot4` 포함), `--camera-file`, `--capture-format png`, `--render-size WxH`

**셰이더 (`PixelShader.hlsl`의 `OCEAN_DETAIL` 변형)**

같은 파일을 `OCEAN_DETAIL=1`로 한 번 더 컴파일하고, Ocean Detail 값이 켜졌을 때만 이 변형을 바인딩한다. 분기만 추가해도 드라이버 코드 배치가 바뀌어 기존 화면이 1~4 레벨 달라졌기 때문이다(NOTES).

```
// a. 원경 리플 글리터: 노멀맵 mip 레벨만큼 그 맵의 (측정한) 경사 분산을 σ²에 더함 — 노멀 불변
σ²_ripple = Σ_layer slopeVariance_layer × smoothstep(1, 5, mipLevel) × strength² × rippleRoughness
// b. 돌풍 패치: 월드 XZ value noise 2옥타브, 바람 방향으로 흐름
gust = noise(xz / gustScale - wind·t)   → 리플 기울기 ×(1 ± gustStrength), 파도 법선 기울기 ×(1 ± gustStrength/2)
// c. 수평선 연무 (하늘색 = 같은 방위 앙각 ~10°, 흐린 mip) — 최종 색만
color = lerp(color, Sky, hazeStrength × (1 - exp(-dist / hazeDistance)))
// d. 먼 파도 마루: 픽셀 구간 가중치 중 (1 - farWaveCrests)를 거칠기 구간으로
```

**참고 자료:**

- Bruneton et al., "Real-time Realistic Ocean Lighting using Seamless Transitions from Geometry to BRDF" (2010)
- Toksvig, "Mipmapping Normal Maps" (2005) — 검토 후 미채택(이 맵들의 mip은 재정규화되어 있다)

## 3. Inputs / Outputs

| 종류 | 이름 | 형식 | 범위 / 비고 |
|---|---|---|---|
| Shot | `ocean_hero` | CaptureShot | View 창 버튼, `--shot`, `--capture` |
| CBuffer | `g_DetailParams.x` | float | rippleRoughness 0~2, 기본 0 |
| CBuffer | `g_DetailParams.y` | float | gustStrength 0~1, 기본 0 |
| CBuffer | `g_DetailParams.z` | float | gustScale 5~200 (월드 단위), 기본 30 |
| CBuffer | `g_DetailParams.w` | float | hazeStrength 0~1, 기본 0 |
| CBuffer | `g_FarParams.y` | float | hazeDistance 20~600, 기본 150 |
| CBuffer | `g_FarParams.zw` | float2 | 레이어 A / B 노멀맵 경사 분산 (`kNormalMaps.slopeVariance`, 저장하지 않음) |
| CBuffer | `g_DetailParams2.x` | float | farWaveCrests 0~1, 기본 1 |
| ImGui | Water › Ocean Detail | slider | 위 6개, 프리셋 extra key로 저장 |
| Debug | 모드 7 | view | R = 리플 거칠기 ×20, G = 돌풍, B = 연무 |
| CLI | `--camera-file` `--capture-presets` `--capture-shots` `--render-size` `--capture-format` | string | 샘플 · 고해상도 렌더 |

**의존하는 다른 feature:** `wave-far-normals`(원경 σ²), `bench-tools`(캡처), `ocean-shots`

## 4. Acceptance Criteria / Test Plan

- [x] Debug/Release 빌드 경고 0
- [x] 기존 Basic / Sunset / Tropical 18장이 수정 전과 해시까지 같다
- [x] `ocean_hero`에서 세 프리셋이 같은 위치, 각자의 태양·색으로 찍힌다
- [x] 디버그 모드 7(σ² / 돌풍 / 연무)로 분포 확인
- [x] Ocean Detail 변형 비교 샘플(`make_samples.ps1`)
- [ ] `ocean_hero` 구도와 최종 이미지 사용자 승인
- [x] NOTES

## 5. Notes

진행 기록은 `NOTES.md`.
