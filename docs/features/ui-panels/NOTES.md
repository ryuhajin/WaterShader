# ui-panels — 설정 UI를 세 창으로 + 초보자용 Water UI (회고 기록)

> 2026-10-03 · feature/ui-panels

스크린샷: `captures/before_ui.jpg`(기존 한 창), `captures/after_all.jpg`(세 창), `captures/after_water_full.jpg`(Water 창 전체).
재현: `WaterShader.exe --no-input --ui <view|light|water|all>`.

---

## 문제

### 1. 한 창에 전부

`Shader Bench` 창 하나에 섹션 11개가 세로로 이어져 있었다.
- 섹션: 모델 회전 · 카메라 · 카메라 프리셋 · 프리셋 · 라이팅 · Ambient · 환경 · Water · 톤매핑 · 캡처 · 디버그
- 720p 화면에서는 **Lighting 첫 줄까지만** 보이고(`before_ui.jpg`) 나머지는 스크롤해야 했다.
- 물을 만지려면 카메라·프리셋을 지나 내려가야 했고, 창이 화면 오른쪽 1/3을 계속 가렸다.
- 기능을 붙일 때마다 아래에 섹션을 추가해 온 결과다. "무엇을 하려는 사람이 어떤 순서로 보나"를 기준으로 정리한 적이 없었다.

### 2. 내부 용어 그대로인 Water 항목

| 기존 이름 | 실제 의미 | 왜 어려웠나 |
|---|---|---|
| `Layer A - U speed`, `V speed` 외 2개 | 노멀맵 텍스처 스크롤 속도(UV/초) | U/V는 텍스처 좌표라 화면에서 어느 쪽인지 알 수 없다. 게다가 **패턴은 값의 반대 방향**으로 움직인다(아래 참고) |
| `Detail Layer Scale (B / A)` | 레이어 B가 A보다 몇 배 촘촘한가 | "B / A"가 무엇의 비율인지 모름 |
| `Normal Scale (tile)` | 노멀맵 반복 횟수 | 클수록 무늬가 **작아진다**는 게 이름에 없음 |
| `Wave 0~3` + `Direction (deg)` | Gerstner 파도 4개(큰 너울 → 잔물결), 진행 방향 | 번호만으로는 역할을 모르고, 각도는 월드 기준이라 화면에서 어느 쪽인지 머릿속으로 계산해야 함 |
| `Fresnel Power`, `Fresnel F0` | 반사가 시선 각도에 따라 바뀌는 곡선, 정면 반사율 | 용어를 알아야 의미를 앎 |
| `Steepness (Gerstner Q)` | 파도 마루가 뾰족한 정도 | 수식 변수명 |

### 3. 쓰지 않거나 틀린 컨트롤

- `Specular Strength / Sharpness`: hdr-env에서 태양을 중복 계산해 0으로 꺼 둔 항목. 슬라이더가 남아 있어 켜면 오히려 틀린 결과가 된다.
- `Reset Light`: HDR 이전 기본값(흰 해, 강도 1)으로 되돌려 지금 하늘과 맞지 않았다. `Calibrate From Sky`가 올바른 초기화.
- `Time:` 텍스트: Stats 오버레이와 중복.

---

## 해결

### 분류 기준 = "무엇을 바꾸려는가"

| 창 (키) | 들어간 것 | 기준 |
|---|---|---|
| **View Settings [1]** | 프리셋 Apply/Save(맨 위), 카메라, 카메라 프리셋, 씬(Ocean Grid·Skybox·판 회전), 캡처, 디버그 뷰 | 장면을 **보는 방법**, 작업 도구 |
| **Light Settings [2]** | 하늘/환경 + Calibrate, 태양, 태양 글린트, Ambient, 톤매핑 | **빛**이 어디서 얼마나 오는가 |
| **Water Settings [3]** | 물 색, 반사, 노멀맵, 파도 | **물 자체**의 재질과 모양 |

- 프리셋은 하늘·빛·물을 함께 저장해서 어느 창에도 딱 맞지 않는다. 사용자 결정으로 View 창 맨 위에 두었다.
- 태양 글린트는 물 셰이더 항목이지만 태양 빛의 반사라 Light 창에 두었다. 강도 1이 물리값이라는 점도 빛 단위와 연결된다.
- 단축키 `1`/`2`/`3`(숫자패드 포함)으로 켜고 끈다.
  - 처음엔 세 창 모두 닫혀 있고, Stats 오버레이에 `Keys: [1] View [2] Light [3] Water` 안내가 있다.
  - 창의 X 버튼도 같은 bool에 연결된다.
  - 키를 누른 순간만 토글한다(누르고 있는 동안 깜빡이지 않게 이전 프레임 상태와 비교).
  - 캡처 라벨 입력 중에는 숫자가 글자로 들어간다. `System`이 ImGui가 키보드를 쓸 때 `Input`으로 키를 안 보내고, 여기서 `WantTextInput`으로 한 번 더 막는다.

### Water 창 — 이름은 위에, 대분류는 구분선

- 슬라이더 옆에 라벨을 두면 창 폭 때문에 이름을 줄여야 했다. **라벨을 슬라이더 위에**(긴 이름은 줄바꿈) 두고 슬라이더는 창 폭 전체를 쓰는 헬퍼 `LabeledSlider` / `LabeledColor`를 만들었다.
- 대분류: `── Water Color ──`, `── Reflection ──`, `── Normal Map ──`, `── Waves (geometry) ──`.
- 노멀맵과 파도의 차이를 한 줄로 명시했다. 노멀맵은 "Small ripples on the surface (lighting only)", 파도는 "Moving swells that shape the mesh". 둘 다 물결이라 헷갈리기 쉽다.

| 기존 | 새 이름 |
|---|---|
| Facing / Grazing Color | Color looking straight down / Color at a low angle |
| Reflection Strength / Fresnel F0 / Fresnel Power | Reflection strength / Reflectivity head-on (F0, real water = 0.02) / Fresnel curve (5 = physical; higher = mirror only at low angles) |
| Normal Strength | Ripple strength (bumpiness) |
| Normal Map A/B | Large ripples (layer A) / Small ripples (layer B) 아래에 Texture |
| Normal Scale (tile) | Normal map scale (repeats per 2 units; higher = smaller) |
| Detail Layer Scale (B / A) | Size (times smaller than the large ripples) |
| Layer A/B U, V speed (4개) | 레이어마다 **Flow direction + Flow speed** (+ 방향 다이얼) |
| Wave 0~3 | Wave 1 - Big swell / 2 - Medium swell / 3 - Small waves / 4 - Ripples |
| Amplitude / Wavelength / Steepness (Gerstner Q) | Height / Length (crest to crest) / Sharpness (0 = round, 1 = pointed crests) |

### 방향을 "화면 기준"으로 — 다이얼 + 설명

사용자 요청은 "수평/수직처럼". 그런데 파도 방향은 **월드 기준 각도**라, 카메라를 돌리면 같은 파도도 화면에서는 가로가 됐다가 세로가 된다. 고정된 "수평/수직" 이름은 시점이 바뀌면 틀린다.
→ 매 프레임 **카메라 기준 상대 각도**를 계산해 보여준다.
- `ViewRelativeDeg`: 파도 방향(xz)을 카메라 forward `(sin yaw, cos yaw)`와 right `(cos yaw, −sin yaw)`에 투영한다. `atan2(right 성분, forward 성분)`에서 0° = 카메라에서 멀어짐, +90° = 화면 왼→오.
- `DrawDirectionDial`: 작은 원. 위쪽 눈금이 카메라 시선이고 화살표가 진행 방향이다. **화면과 같은 방향으로 읽힌다**(위 = 멀어짐, 오른쪽 = 오른쪽).
- `DescribeViewDirection`: 8방위 텍스트 — `away from camera`, `left -> right`, `toward, to the left` 등.
- 파도 헤더 줄과 노멀맵 Flow direction에 같이 표시한다. 정확한 값은 월드 각도 슬라이더(`0 = +X, 90 = +Z`)로 조절한다.

### 노멀맵 흐름: 보이는 값과 저장 값 분리

- 저장 값은 그대로 UV 속도 `(u, v)`다. 셰이더(`uv + scroll * t`)와 프리셋 포맷을 바꾸지 않는다.
- **과정에서 확인한 것 — 화면 방향:**
  - 평면 UV는 `u = x/2 + 0.5`, `v = 0.5 − z/2`(`Model::InitializeGrid`)라서 u는 +X, **v는 −Z** 방향이다.
  - 텍스처를 `uv + s·t` 위치에서 샘플링하면 무늬는 **−s 방향**으로 움직인다.
  - 그래서 실제 월드 흐름 방향은 `(−u, +v)`다. 예: Basic 레이어 A `(0.025, 0.02)`는 월드 141°로 흐른다. 기존 "U speed +"를 보고 +X로 흐른다고 생각하면 정반대였다.
- `FlowControls`: 표시는 `방향 = atan2(v, −u)`, `속도 = |s|`이고, 슬라이더를 움직일 때만 `(u, v) = (−cos·speed, sin·speed)`로 다시 쓴다. **안 건드리면 저장 값이 비트 단위로 그대로다.**
- 속도 0이면 방향을 알 수 없어서, 레이어마다 마지막 방향을 기억한다(`flowDirectionDeg_`).

---

## 검증

- **렌더 회귀:** UI만 바꿨으므로 렌더 결과가 같아야 한다. `--capture`(UI 숨김) 15장을 normal-select 결과와 비교 → **15장 모두 최대 차이 0**.
- **프리셋 왕복:** 기존 값으로 `Save Current`를 하면 숫자 표기만 바뀌고(`1.0000` → `1`) 값은 그대로였다. 전 필드를 수치 비교해 차이 0.
- **방향 설명 확인:** `sunward` 샷(카메라 yaw 33°)에서 Wave 1(월드 20°)은 `away, to the right`로 나온다. forward 57°와 right −33° 사이에 있으니 맞다. 레이어 A 흐름(141°)은 `right -> left`로 나온다.
- **스크린샷:**
  - `--ui` 옵션으로 무인 모드에서도 창을 띄워 캡처했다.
  - 720p에서는 Water 창 아래쪽이 잘린다. 처음엔 imgui.ini의 창 위치를 음수로 바꿔 아랫부분을 화면에 올리려 했지만 **ImGui가 창 위치를 화면 안으로 클램프**해서 실패했다.
  - 앱 창 크기를 `SetWindowPos(SWP_NOACTIVATE)`로 1280×1400까지 키워(포커스를 빼앗지 않음) 전체를 캡처했다.
- Debug/Release 경고 0.
- 사용자 수동 확인 필요: 1/2/3 키 토글, 캡처 라벨에 숫자 입력 시 창이 토글되지 않는지.

## 교훈

- 기능 순서대로 쌓인 UI는 만든 사람의 작업 순서를 따른다. **쓰는 사람의 질문**(보는 법? 빛? 물?) 기준으로 다시 묶어야 찾을 수 있다.
- 이름에 단위와 방향(클수록 어떻게 되는지, 물리값이 얼마인지)을 넣으면 툴팁 없이도 쓸 수 있다.
- 사람이 보는 값(방향·속도, 화면 기준)과 데이터 값(UV 벡터, 월드 각도)을 분리하면 UI를 바꿔도 저장 포맷과 셰이더를 건드리지 않아도 된다. 또 UI로 옮기는 과정에서 기존 UV 속도의 **방향이 직관과 반대**였다는 것도 드러났다.
