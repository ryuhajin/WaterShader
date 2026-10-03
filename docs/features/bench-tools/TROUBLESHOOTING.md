# bench-tools — 트러블슈팅 기록 (회고용)

각 항목: 증상 → 원인 → 해결 → 검증 → 교훈

---

## A. 캡처 실행 중 프로그램이 "응답 없음"으로 멈춤 (2026-09-29)

**증상**
- `WaterShader.exe --capture <label>` 실행 중 창이 "응답 없음"이 되고 끝나지 않음. 이벤트 로그에 `AppHangB1`(Application Hang, 1002)로 두 번 기록(22:40, 22:49).
- 항상 같은 지점에서 멈춤: 6번째 파일 `sunset_oblique.jpg`가 **1,382,400 바이트**(= 1280×720×1.5)에서 멈추고 파일이 잠긴 상태. 약 1.3초 만에 발생.

**조사**
- CPU 시간이 0.7초에서 더 늘지 않음 → 바쁜 루프가 아니라 대기/정지 상태.
- `Process.Threads`의 WaitReason: 48개 중 46개가 `Suspended`. 디버거·WerFault 없음.
- **실행을 감시하던 부모 PowerShell도 같은 순간에 전체 스레드 `Suspended`** → 앱 코드 문제가 아니라 외부에서 **프로세스 트리째 정지**시킨 것.
- 재현 조건 비교:

| 조건 | 결과 |
|---|---|
| 새 폴더에 캡처 (파일 생성) | 1.0초에 정상 종료, 15장 |
| 같은 폴더에 다시 캡처 (기존 JPG **덮어쓰기**) | 매번 6번째 파일에서 트리 정지 |
| 명령 샌드박스 끄고 덮어쓰기 | 동일하게 정지 → 샌드박스 원인 아님 |
| 기존 JPG를 **먼저 삭제**한 뒤 캡처 | 1.1초에 정상 종료 |

**원인 (추정)**
- 방금 빌드된 서명 없는 exe가 짧은 시간에 **기존 이미지 파일 여러 개를 다른 내용으로 덮어쓰는** 패턴 = 랜섬웨어(파일 암호화)와 같은 행동 패턴. 보안 소프트웨어(Microsoft Defender의 행위 감시로 추정)가 판정을 위해 프로세스 트리를 정지시킨 것으로 보인다.
- Defender Operational 로그에 탐지 이벤트는 남지 않아서 **확정은 아님**. 다만 "덮어쓰기일 때만, 트리 전체가 Suspended, 삭제 후 생성이면 정상"이라는 조건이 이 설명과 모두 맞는다.

**해결**
- `Graphics::EndCaptureFrame`: 저장 직전에 `std::filesystem::remove(pendingCapturePath_)`로 기존 파일을 지우고 새로 생성.

**검증**
- 같은 폴더에 연속 2회 캡처 → 1.1초 / 1.0초, 15장씩 정상.

**교훈**
- "응답 없음"이 항상 코드 데드락은 아니다. **스레드 WaitReason과 부모 프로세스 상태**를 먼저 보면 내부 대기인지 외부 정지인지 바로 갈린다.
- 대량 파일 출력 도구는 in-place 덮어쓰기보다 삭제 후 생성(또는 임시 파일 → rename)이 안전하다.

---

## E. 카메라 프리셋 버튼을 누른 뒤 WASD로 카메라가 안 움직임 (2026-09-30)

**증상**
- `sunward`, `ocean_wide` 같은 프리셋 버튼을 누르고 나면 WASD/화살표로 카메라가 움직이지 않음. 뷰포트 빈 곳을 한 번 클릭하면 다시 됨.

**원인**
- `System.cpp`에서 `ImGuiConfigFlags_NavEnableKeyboard`를 켜 두었음. 이 상태에선 **ImGui 창에 포커스만 있어도** `io.WantCaptureKeyboard = true`.
- `System::MessageHandler`는 `WantCaptureKeyboard`일 때 `WM_KEYDOWN`을 `Input`에 넘기지 않음 → 버튼 클릭으로 Shader Bench 창이 포커스를 가지면 키 입력이 전부 막힘. 앱 시작 직후에도 창이 자동 포커스되어 같은 상태였다.
- 잠재 버그: `WM_KEYUP`도 같은 조건으로 막혀 있어서, W를 누른 채 ImGui를 클릭하고 떼면 W가 계속 눌린 상태로 남을 수 있었다.

**해결**
- ImGui 키보드 내비게이션 끔 → ImGui는 텍스트 입력 중일 때만 키보드를 가져감(화살표 카메라 회전과의 충돌도 사라짐).
- `WM_KEYUP`은 조건 없이 항상 `Input`에 전달.

**검증**
- 창 메시지(`PostMessage`)로 W 입력을 0.8초 보내는 테스트(실제 마우스·키보드는 건드리지 않음): 수정 전 Shader Bench에 포커스가 있는 상태에서 카메라 위치 변화 없음 → 수정 후 같은 조건에서 이동 확인.

**교훈**
- `WantCaptureKeyboard`는 "지금 타이핑 중"이 아니라 설정에 따라 "ImGui 창이 포커스됨"을 뜻할 수 있다. 입력을 막는 조건은 KEYDOWN에만 걸고, KEYUP(해제)은 항상 통과시킨다.

---

## B. (8번) JPG 노멀맵 로드 실패 원인 — COM 초기화 가설 검증

이전 기록(`water-normal-map/NOTES.md`)은 "Photoshop JPG의 Adobe APP14 마커 + ICC 프로필 때문에 WIC가 거부"로 추정했다.

**현재 확인한 사실**
- 현재 JPG 5장 모두 Adobe APP14 마커(`ff d8 ff ee … Adobe`)와 `ICC_PROFILE`이 **그대로 있는데 WIC 로드는 성공** → 마커 가설은 적어도 현재 조건에선 성립하지 않음.
- `CoGetApartmentType` 프로브 (코드 어디에서도 `CoInitialize`를 호출하지 않는 상태):

| 시점 | 결과 |
|---|---|
| `Graphics::Initialize` 시작 (D3D 디바이스 생성 전) | `0x800401F0 CO_E_NOTINITIALIZED` — 메인 스레드에 COM 없음 |
| D3D 디바이스 생성 후 | `S_OK`, `APTTYPE_MTA`, qualifier `IMPLICIT_MTA` |

- 해석: 메인 스레드는 COM을 초기화한 적이 없지만, **D3D/드라이버가 만든 스레드가 프로세스에 MTA를 만들어 두어서** 메인 스레드가 "암묵적 MTA"로 취급되고 `CoCreateInstance`(WIC 팩토리)가 우연히 성공한다.
- 즉 COM 미초기화는 **실제로 있는 결함**이고, 동작 여부가 드라이버 내부 구현에 달려 있다(로드 순서가 바뀌거나 D3D 생성 전에 WIC를 쓰면 실패).
- 예전 실패의 정확한 HRESULT는 기록이 남지 않아 **재현 불가**. 당시 코드 순서/드라이버 상태에 따라 암묵적 MTA가 없었다면 `CO_E_NOTINITIALIZED`로 실패했을 것으로 보는 것이 가장 그럴듯하다.

**숨어 있던 두 번째 버그 — JPG도 감마 디코딩됨**
- `--normal-map textures/water_normal.jpg --debug 1`로 JPG를 강제 로드해 노멀맵 샘플을 캡처, 화면 평균을 DDS와 비교:

| 로드 경로 | 평균 RGB | 판정 |
|---|---|---|
| DDS (`--ignore-srgb`로 재생성한 것) | (127.6, 127.4, 251.0) | 정상 |
| JPG, 기본 `CreateWICTextureFromFile` | **(55.5, 56.8, 247.0)** | sRGB로 디코딩됨 |
| JPG, `WIC_LOADER_IGNORE_SRGB` | (127.6, 127.4, 251.0) | 정상 |

- 원인: Photoshop JPG의 sRGB 메타데이터 → DirectXTK WIC 로더가 `R8G8B8A8_UNORM_SRGB`로 텍스처를 만듦 → 샘플링 시 GPU가 sRGB→linear 변환. water-polish Step 1의 DDS 버그와 **같은 원인이 다른 경로에 또 있었던 것**. JPG 폴백이 실제로 쓰였다면 노멀이 똑같이 −X/−Z로 기울었을 것.

**해결**
- `main.cpp`: `wWinMain` 시작에서 `CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)`, 종료 시 `CoUninitialize`. `EndCaptureFrame`의 임시 `CoInitializeEx(MTA)` 제거.
- `Texture.cpp`: `CreateWICTextureFromFileEx(..., WIC_LOADER_IGNORE_SRGB, ...)` — 이 클래스는 노멀맵(데이터 텍스처) 전용.
- `--normal-map <path>` 옵션 추가(assets 기준 경로, 기본 DDS보다 먼저 시도) — 로더 경로별 검증용.

**검증**
- 위 표의 세 번째 행. COM 초기화 후에도 캡처(WIC JPEG 저장) 15장 정상.

**교훈**
- "되니까 괜찮다"가 아니라 **왜 되는지**를 확인해야 한다. COM은 드라이버 덕에 우연히 동작하고 있었고, JPG 경로는 로드는 성공하지만 값이 틀린 상태였다.
- 데이터 텍스처(노멀·마스크·높이)는 모든 로드 경로(DDS 변환, WIC, 엔진 임포터)에서 **색 공간 변환을 끄는 옵션**을 명시해야 한다. 한 경로만 고치면 다른 경로에서 재발한다.
- 로더 상태 문자열(`[OK]/[FAIL]`)만으로는 부족하다. **로드된 값의 평균**을 한 번 찍어 보는 것이 가장 빠른 검증.

---

## D. (5번) Sunset 물이 흙색으로 보임

**증상**
- Sunset 프리셋에서 물의 붉은 부분이 "노을빛"이 아니라 흙탕물(갈색)처럼 보임.

**측정** (`sunset_ocean_sunward` 캡처의 영역 평균, HSB)

| 단계 | 앞쪽 물 | 중간 물 | 빛의 길 |
|---|---|---|---|
| before (초원 하늘) | H 328° S 0.10 B 0.26 | H 324° S 0.06 B 0.34 | H 16° S 0.07 B 0.71 |
| env (붉은 노을 하늘만 교체) | H 337° S 0.15 B 0.23 | H 341° S 0.13 B 0.30 | H 13° S 0.11 B 0.67 |
| final (+ 몸체색 수정) | H 300° S 0.05 B 0.19 | H 340° S 0.06 B 0.28 | H 20° S 0.09 B 0.68 |

- 반사된 하늘(초원 스카이박스)은 수평선 근처가 S 0.12, 위쪽 S 0.15 — **붉은 하늘이 아니라 흐린 회분홍**.

**원인**
- 갈색 = **따뜻한 색상 + 낮은 채도 + 낮은 명도**. 물 색이 정확히 그 영역에 있었다.
- 몸체색 (0.14, 0.12, 0.20) × 붉은 ambient (0.85, 0.60, 0.60)·0.7 ≈ (0.08, 0.05, 0.08) → 어둡고 붉은 기가 도는 회색. 태양 고도 5°라 Lambert ≈ 0.09로 직접광 기여도 거의 없음.
- 반사로도 붉은색이 들어올 수 없었음: 스카이박스 하늘 자체가 붉지 않다.

**"Grazing 색만 바꾸면 되나?" → 아니다**
- 비스듬한 각도(= 화면 대부분)에선 Fresnel이 1에 가까워 **반사가 지배**하고 grazing 색 비중은 작다.
- 그리고 몸체색은 ambient와 **곱해지므로**, 붉게 올리면 어두운 붉은 회색(= 더 갈색)이 된다. 실제로 `env` 단계(하늘만 붉게)에서 채도가 0.13~0.15로 올랐지만 명도가 낮아 적갈색으로 보였다.

**해결**
1. Sunset에 실제 붉은 노을 하늘(Poly Haven `the_sky_is_on_fire`) 환경맵 → 붉은색은 **반사(빛의 길, 수평선)**로 들어오게.
2. 몸체색을 차가운 남색으로: Facing (0.02, 0.05, 0.09) / Grazing (0.06, 0.09, 0.14), ambient 붉은기 축소 (0.60, 0.50, 0.62)·0.55, Fresnel power 5 → 4(중간 각도에서 반사 비중 약간 증가).

**검증**
- final: 물 몸체는 채도 0.05의 어두운 중성색(= 저녁의 어두운 물), 따뜻함은 빛의 길(H 20°)과 수평선 반사에만 있음 → 흙색이 아니라 "노을이 비친 어두운 바다"로 읽힘. 비교: `compare/ocean_sunward.jpg`.

**교훈**
- 물 색은 대부분 **반사된 환경의 색**이다. 노을 물을 만들려면 물을 붉게 칠하는 게 아니라 붉은 하늘을 비춰야 한다.
- 어두운 따뜻한 색은 채도를 올려도 갈색이 된다. 몸체는 차갑게, 따뜻함은 반사/하이라이트로.

---

## C. (7번) C4244 — `wstring` → `string` 변환 경고

**증상**
- 빌드 시 `xutility(4813): warning C4244: 'wchar_t'에서 'char'(으)로 변환하면서 데이터가 손실될 수 있습니다`. 경고 위치가 STL 헤더라 원인 코드가 바로 안 보임.

**원인**
- `Graphics.cpp`의 `std::string s(ws.begin(), ws.end())` 패턴 3곳(노멀맵 로그의 경로·에러 문자열, `--capture` 라벨). 반복자 생성자가 `wchar_t`(UTF-16)를 **한 글자씩 `char`로 잘라** 넣는다.
- ASCII만 쓰면 멀쩡해 보이지만 한글 경로/라벨은 깨진다. 예: `--capture 테스트_한글` → 폴더 이름이 깨짐.
- 같은 계열 문제: `std::filesystem::path::string()`은 시스템 코드 페이지(CP949)로 변환하는데, ImGui는 UTF-8을 기대 → 한글 경로가 ImGui에서 깨져 보임.

**해결**
- `WideToUtf8` / `Utf8ToWide` 헬퍼(`WideCharToMultiByte` / `MultiByteToWideChar`, `CP_UTF8`).
- `StartCaptureSet`이 `std::wstring` 라벨을 받도록 변경. ImGui `InputText`(UTF-8) → `Utf8ToWide`, 표시용 경로는 `WideToUtf8(path.wstring())`, 디버그 출력은 `OutputDebugStringW`.
- 명령줄 파싱을 `Initialize` 앞부분 한 곳으로 모음(`--capture`, `--debug`, `--normal-map`).

**검증**
- 빌드 경고 0. `--capture 테스트_한글` → `captures/테스트_한글/` 폴더에 15장 정상 생성.

**교훈**
- STL 내부에서 뜨는 경고도 호출 지점을 추적할 것. "기존 코드에도 있던 경고"가 실제 버그(한글 경로)를 가리키고 있었다.
- Windows에서 문자열 경계는 셋: Win32(UTF-16) / 파일시스템 path(wide) / UI·로그(UTF-8). 경계마다 명시적으로 변환한다.
