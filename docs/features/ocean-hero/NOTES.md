# ocean-hero — 포트폴리오 메인 항공 컷 자연화 (작업 기록)

> 2026-10-04 · feature/ocean-hero (main에서 분기)

캡처 조건은 이전 feature와 같다(1280×720, `t = 12s`, 무인 모드, Debug 빌드 = 소스 셰이더·assets).
재현:
- 샘플: `powershell -File make_samples.ps1` → `samples/w0~w3.txt`(변형 프리셋), `captures/w0~w3/hero_slotN.jpg`, 비교 시트 `compare/slotN.jpg`
- 회귀: `WaterShader.exe --capture-feature ocean-hero --capture <label> --capture-presets basic,sunset,tropical` → `captures/before`와 해시 비교
- 최종: `WaterShader.exe --capture-feature ocean-hero --capture final --capture-presets hero --capture-shots slot4 --render-size 2560x1440 --capture-format png`

---

## 문제

포트폴리오 PDF 메인으로 쓸 `ocean_aerial` + Basic(`captures/before/basic_ocean_aerial.jpg`)에서 근경 → 원경 전환이 부자연스러웠다.
글린트가 맺히는 모습은 사용자가 마음에 들어 해서 유지 대상이다.

| 구간 | 보이는 것 | 원인 |
|---|---|---|
| 근경 | 노멀맵 잔물결 + 메시 파도 | — |
| 중경 | 일정 간격의 가로 어두운 점선 줄 | 메시 페이드 이후 **픽셀 구간**에서 작은 파도 2개(λ 1.4, 0.92)의 사인 마루가 교차해 규칙 격자가 된다. Basic 바람(−105°)이 시선과 거의 평행해 이 격자가 가로 줄로 눕는다 |
| 원경 | 나무·하늘이 또렷이 비치는 거울 띠 | 거칠기 구간의 σ²는 글린트 폭만 넓히고 하늘 반사는 흐리게 하지 않는다(wave-far-normals step4 결정). 대기 원근이 없다 |

진단 근거: 디버그 뷰 2(월드 법선)에서 격자 무늬가 보였다. `--far-waves off`로 찍으면 격자와 줄무늬가 사라지고 근경 글린트도 그대로였다.

## 설계

### 1. 별도 셰이더 변형 `OCEAN_DETAIL` — 기존 프리셋 비트 동일

처음에는 새 항목을 모두 `if (값 > 0)` 분기 안에 넣었다. 기존 프리셋 18장 중 9장이 1~4 레벨 다르게 나왔다(일부 픽셀).
- 분기를 타지 않아도, 코드가 늘어나면 드라이버의 명령어 배치(FMA 결합 등)가 바뀐다. 이전 픽셀 셰이더 + 새 C++로 찍으면 0장이 달랐으므로 원인은 셰이더 쪽이다.
- 연무 블록 하나만 남겨도 18장이 모두 달라졌다.

그래서 `ColorShader::Reload`가 같은 `PixelShader.hlsl`을 두 번 컴파일한다: 기본(정의 없음) / `OCEAN_DETAIL=1`.
Ocean Detail 값이 하나라도 켜져 있거나(디버그 7 포함) Far wave crests < 1이면 변형을 바인딩한다.
결과: 기본 프로그램은 이전과 같은 코드이고, Basic / Sunset / Tropical 18장이 `captures/before`와 **해시까지 같다**(세 번 재확인).

### 2. 항목 (`g_DetailParams`, `g_DetailParams2`, `g_FarParams.yzw`)

| 항목 | 구현 | 파라미터 (기본 = 꺼짐) |
|---|---|---|
| Far wave crests | `EvaluateFarWaves`의 픽셀 구간 가중치 w 중 (1 − keep)을 거칠기로 넘긴다. 격자 대신 넓은 글리터가 된다 | `farWaveCrests` 1 |
| Far ripple glitter | 노멀맵 mip 레벨(`CalculateLevelOfDetail`, mip 1 → 5 smoothstep)만큼 그 맵의 경사 분산을 σ²에 더한다 | `rippleRoughness` 0 |
| Gust patches | 월드 XZ value noise 2옥타브(바람 방향으로 흐름)로 리플 기울기 ±, 파도 법선 기울기 ±절반 | `gustStrength` 0, `gustScale` 30 |
| Horizon haze | 거리 `1 − exp(−d / D)` × 세기만큼 같은 방위 하늘색(앙각 약 10°, mip 7)으로 보간 | `hazeStrength` 0, `hazeDistance` 150 |

- **리플 경사 분산:** jpg 원본에서 (x² + y²) / z²의 평균을 쟀다. 값은 0 Diagonal 0.060 / 1 Soft swell 0.0125 / 2 Soft chop 0.0072 / 3 Long streaks 0.023 / 4 Fine chop 0.062이다.
  - `kNormalMaps.slopeVariance`에 넣고, 매 프레임 `g_FarParams.zw`로 보낸다.
  - 처음에는 Toksvig(mip 평균 법선의 길이)로 추정했으나 0에 가까웠다. 이 DDS mip들은 재정규화되어 있어 길이가 줄지 않는다(디버그 7의 R이 거의 0).
- **연무 하늘색:** 처음에는 수평선 바로 위(앙각 2.3°, mip 0)를 샘플했다. 이 하늘의 수평선에는 언덕·나무가 있어, 열마다 다른 색을 집어 방사형 줄무늬와 대각선 띠가 생겼다. 앙각 10°의 흐린 mip으로 바꿔 해결했다.
- **돌풍 크기:** 처음 값(gustScale 40, 대비 ×2.6)은 프레임에 패치가 하나뿐이고 경계가 딱딱했다. 15 / ×1.6으로 바꿨다.

## 샘플 비교 (`make_samples.ps1`, 시트 `compare/`)

구도(`cameras_candidates.txt`): C1 `ocean_aerial` 그대로 / C2 같은 위치에서 태양 정면(pitch 28, yaw 38) / C3 고공 사선(높이 20, pitch 38) / C4 저공 광각(높이 6, 화각 60, 수평선 포함).

| 변형 | 내용 | 관찰 |
|---|---|---|
| w0 | Basic | 기준 |
| w1 | Basic + 원경 글리터 0.3, 돌풍 0.4 / 15, 연무 0.35 / 150 | 가로 줄이 옅어지고 원경 거울 띠가 연무로 이어짐. 근경 글린트 그대로 |
| w2 | w1 + 바람 −125°, chop 0.2, Far wave crests 0.6 | 줄이 비스듬히 흐르고 격자가 약해짐 |
| w3 | w2 + spread 32, size 4.2, crests 0.35, Far spread 0.35, 돌풍 0.55, 연무 0.45 | 가장 잔잔함. 글린트가 덩어리로 뭉침 |

기각한 시도 (샘플 1·2차):
- **바람 −140°, spread 45°, 노멀맵 Soft chop / Fine chop:** 다이아몬드 격자가 생기고 글린트가 큰 얼룩이 됐다. spread가 35°를 넘으면 격자가 생긴다(wave-far-normals와 같은 결론).
- **파도 높이 0.035, chop 0.15:** 물이 매끈해져 글린트가 넓은 판으로 번졌다.
- **리플 세기 0.55:** 비스듬한 노멀맵 무늬가 도드라졌다.
- **원경 글리터 1.0:** 글린트가 흰 판으로 번져 반짝임이 사라졌다.
- **Far wave crests 0 / 0.3 (단독):** 격자는 사라졌지만 원경 글린트가 크게 번졌다. Far spread를 함께 낮춰야 한다.

C4는 수평선과 언덕이 프레임에 들어온다. 연무가 물에만 걸려 물과 언덕 사이에 경계가 생긴다(하늘 셰이더에는 연무가 없다).

## 사용자 선택 (2026-10-04)

- **구도:** C1 `ocean_aerial` → 카메라 슬롯 4 (`1 -5.38 12 -20.09 30 15 0 50 1 0 0 0`)
- **웨이브:** w1 → 4번째 프리셋 **Hero**(ocean)
  - 값: Basic + `rippleRoughness 0.3`, `gustStrength 0.4`, `gustScale 15`, `hazeStrength 0.35`, `hazeDistance 150`
  - bench Hero는 bench Basic과 같다.
- **저장 확인:** 기본 파일로 찍은 `hero_slot4`가 샘플 `w1/hero_slot1`, 고정 샷 `hero_ocean_aerial`과 해시까지 같다.
- **최종 렌더:** `captures/final/hero_slot4.png` (2560×1440)

## 한계 / 남은 것

- 픽셀 구간의 시작·끝은 화면 픽셀 기준(파장당 16 → 8 px)이다. 그래서 `--render-size 2560x1440`에서는 같은 구도라도 마루 구간이 더 멀리까지 남는다. 720p 샘플보다 중경 점선이 또렷하다.
  - 고해상도 최종 컷을 720p와 같은 인상으로 만들려면 Far wave crests를 낮추거나, 픽셀 구간 기준을 렌더 배율로 나누는 보정이 필요하다(미구현).
- 연무는 물에만 적용된다. 수평선이 보이는 구도에서는 하늘 쪽 경계가 드러난다.
- 디버그 7 캡처: `captures/debug7/hero_slot4.jpg` (R = 리플 거칠기 ×20, G = 돌풍, B = 연무)
