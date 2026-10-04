# 업데이트 내역

[README로 돌아가기](../README.md)

## 2026-09 ~ 10

모든 작업은 기능 단위 브랜치로 나눠 진행했고, 수정 전후 캡처와 측정값을 각 기록 문서에 남겼습니다.

### 버그 수정

| 증상 | 원인과 해결 | 기록 |
|---|---|---|
| 물 표면 전체가 한쪽으로 기울어 보이고, 해를 수면 아래에 둬야 반사가 보임 | 노멀맵을 DDS로 변환할 때 사진용 색 보정(감마)이 잘못 적용돼, 평평해야 할 값 128이 55로 바뀌어 있었습니다. 색 변환 없이 다시 변환했습니다 | [water-polish](features/water-polish/NOTES.md) |
| 해의 방향이 거꾸로 계산됨 | 위 버그와 서로 맞물려 그럭저럭 밝아 보이는 바람에 늦게 발견했습니다. 해의 위치를 방위각·고도로 다시 정의했습니다 | [water-polish](features/water-polish/NOTES.md) |
| 물결 뒷면에 하늘 대신 땅이 비침 | 반사 방향이 수평선 아래로 꺾이는 경우를 하늘 쪽으로 접었습니다 | [water-polish](features/water-polish/NOTES.md) |
| 자동 캡처 중 프로그램이 "응답 없음"으로 멈춤 | 기존 이미지를 덮어쓰는 순간 외부(보안 프로그램으로 추정)에서 프로세스를 통째로 정지시켰습니다. 기존 파일을 지운 뒤 새로 저장하도록 바꿨습니다 | [TROUBLESHOOTING A](features/bench-tools/TROUBLESHOOTING.md) |
| JPG 노멀맵을 읽지 못함 | 예전엔 파일 형식 문제로 추정했지만, 실제로는 Windows의 이미지 로더(WIC)가 쓰는 COM이 초기화되지 않은 것이었습니다 | [TROUBLESHOOTING B](features/bench-tools/TROUBLESHOOTING.md) |
| 한글 경로·라벨이 깨지고 빌드 경고(C4244) | 유니코드 문자열을 한 글자씩 잘라 변환하고 있었습니다. UTF-8 변환 함수로 바꿨습니다 | [TROUBLESHOOTING C](features/bench-tools/TROUBLESHOOTING.md) |
| Sunset 물이 노을빛이 아니라 흙탕물처럼 보임 | 비치는 하늘이 붉지 않은 데다 물 색이 갈색 영역에 있었습니다. 노을 하늘을 따로 두고 물 색을 다시 잡았습니다 | [TROUBLESHOOTING D](features/bench-tools/TROUBLESHOOTING.md) |
| 버튼을 누른 뒤 WASD로 카메라가 안 움직임 | UI의 키보드 탐색 기능이 키 입력을 가져가고 있었습니다 | [TROUBLESHOOTING E](features/bench-tools/TROUBLESHOOTING.md) |
| 해가 두 번, 세 번 더해져 너무 밝음 | 하늘 사진 속 해, 반짝임, 하이라이트가 같은 해를 각각 더하고 있었습니다. 하늘에서 해를 잘라내고 셰이더가 그리는 해 하나로 합쳤습니다 | [hdr-env](features/hdr-env/NOTES.md) |
| Capture 버튼을 실수로 누르면 기록이 덮어써질 위험 | 확인 창을 띄우고, 같은 이름의 폴더가 있으면 번호를 붙여 새로 만들도록 했습니다 | [hdr-env](features/hdr-env/NOTES.md) |
| Basic 바다에 노을의 붉은빛이 섞임 | Basic 하늘 자체가 노을 사진이었습니다. 한낮 들판 하늘로 바꿨습니다 | [basic-sky](features/basic-sky/NOTES.md) |
| 잔물결 흐름 속도의 부호가 실제 흐르는 방향과 반대 | 텍스처를 읽는 위치를 밀면 무늬는 반대로 움직여 보입니다. UI에서 실제 흐르는 방향과 속도로 보여주도록 바꿨습니다 | [ui-panels](features/ui-panels/NOTES.md) |
| 오션 그리드에서 잔물결의 밝은 면과 어두운 면이 물결 모양과 어긋나 얼룩져 보임 | 코드로 만든 오션 그리드가 노멀맵을 OBJ 평면과 앞뒤 반대 방향(UV v = −Z)으로 붙여, 셰이더가 노멀맵의 앞뒤 기울기(G 채널)를 거꾸로 읽고 있었습니다. 노멀맵 5장 모두 OBJ 배치에서 일관된다는 것을 회전 성분(curl) 검사로 확인하고, 그리드 UV를 OBJ와 같게 맞췄습니다 | [wave-far-normals](features/wave-far-normals/NOTES.md) |
| 벤치 평면에서 잔물결 흐름 방향이 앞뒤 반대로 표시됨 | 위와 같은 원인입니다. 흐름 방향 표시는 오션 그리드 기준으로만 맞아 있었습니다. 두 메시의 UV를 통일하고 계산을 바꿨습니다 | [wave-far-normals](features/wave-far-normals/NOTES.md) |
| 가까운 파도와 먼 물결의 방향이 X자로 엇갈림 | 거리에 따라 파도를 줄이는 처리가 높이와 함께 물결의 기울기까지 지워, 먼 곳에는 바람과 무관한 노멀맵 무늬만 남았습니다. 먼 곳의 파도 기울기를 픽셀 단위로 다시 계산하고, 노멀맵 무늬를 바람 방향으로 돌리는 옵션을 넣었습니다 | [wave-far-normals](features/wave-far-normals/NOTES.md) |
| (위 수정의 1차 결과) 먼 바다에 마름모 격자 무늬가 생김 | 같은 대비의 파도 줄무늬 4개가 원근으로 압축되며 엇갈렸습니다. 픽셀이 충분히 표현할 수 있는 구간까지만 물결로 그리고, 그보다 작은 물결은 표면 거칠기로 바꿔 햇빛 반짝임을 넓히는 방식으로 고쳤습니다 | [wave-far-normals](features/wave-far-normals/NOTES.md) |
| 해를 등진 먼 바다에서 나무·하늘 반사가 회색으로 뭉개짐 | 거칠기만큼 하늘 반사를 흐리게 한 처리가 반사 형상을 지웠습니다. 반사율(Fresnel) 가설은 측정해 보니 효과가 없었습니다. 반사 흐림을 제거했습니다. 해를 등진 캡처 샷이 없어 처음엔 놓쳤기에 이 샷도 추가했습니다 | [wave-far-normals](features/wave-far-normals/NOTES.md) |
| 바람 정렬을 켠 잔물결이 흐름 방향 표시와 다른 쪽으로 흐름 | 텍스처를 회전한 뒤에 흐름 이동을 더해, 흐름까지 정렬 각도만큼 돌아가 있었습니다(Sunset에서 49°). 이동을 회전 전에 더하도록 고치고, 두 시점의 무늬 이동을 측정해 표시와 1° 이내로 맞음을 확인했습니다 | [ripple-flow-fix](features/ripple-flow-fix/NOTES.md) |
| 항공 부감 샷의 중경이 규칙적인 가로 점선 줄로 보임 | 메시로 그리지 못하는 거리에서 작은 파도 두 개를 픽셀 단위 기울기로 그리는데, 그 사인 마루가 교차해 격자가 됐고 바람이 시선과 평행해 가로로 누웠습니다(바람을 돌리면 방사형 줄무늬). 먼 파도 마루의 일부를 넓은 반짝임으로 넘기는 "Far wave crests"를 넣었습니다 | [ocean-hero](features/ocean-hero/NOTES.md) |
| 새 기능을 꺼 둔 상태인데도 기존 프리셋 화면이 일부 픽셀에서 1~4 레벨 달라짐 | 실행되지 않는 분기라도 셰이더 코드가 늘면 드라이버의 명령어 배치가 바뀌었습니다. 새 기능을 별도 셰이더 변형(`OCEAN_DETAIL`)으로 컴파일해 켰을 때만 쓰도록 해, 기존 18장이 해시까지 같아졌습니다 | [ocean-hero](features/ocean-hero/NOTES.md) |
| 수평선 연무가 방사형 줄무늬를 만듦 | 수평선 바로 위 하늘을 샘플해 언덕·나무를 화면 열마다 집었습니다. 조금 위(앙각 약 10°)의 흐린 하늘색을 쓰도록 바꿨습니다 | [ocean-hero](features/ocean-hero/NOTES.md) |
| 2560×1440 렌더가 창에서 보던 화면보다 뿌옇게 보임 | 먼 파도와 노멀맵의 세밀도가 화면 픽셀 수로 정해져, 고해상도에서는 고운 명암이 늘고 축소해 보면 회색으로 평균됐습니다(밝기 표준편차 37 → 32). 고해상도 렌더는 720p와 같은 기준을 쓰도록 맞췄습니다(→ 37.6) | [ocean-hero](features/ocean-hero/NOTES.md) |
| PNG 캡처가 이미지 뷰어에서 하얗게 뜸 | 화면 버퍼를 저장하는 라이브러리가 PNG에 "리니어 감마" 태그를 붙여, 뷰어가 이미 sRGB인 이미지를 한 번 더 밝게 풀었습니다. sRGB 태그로 저장하도록 바꿨습니다(픽셀 값은 그대로) | [ocean-hero](features/ocean-hero/NOTES.md) |

### 퀄리티 업

| 내용 | 기록 |
|---|---|
| Sine 파도 2개 → Gerstner 파도 4개, 잔물결 2겹, 태양 글린트, 실제 물 반사율, 오션 그리드 | [water-polish](features/water-polish/NOTES.md) |
| 마우스 시점 회전, 카메라 고정 샷·저장 슬롯, 프레임 시간 표시, 프리셋별 하늘 | [bench-tools](features/bench-tools/NOTES.md) |
| 리니어 워크플로, HDR 타깃 + 노출·톤매핑, 무인 캡처 | [linear-hdr](features/linear-hdr/NOTES.md) |
| HDR 하늘과 하늘에서 측정한 조명 | [hdr-env](features/hdr-env/NOTES.md) |
| Basic 하늘을 한낮 들판(belfast_farmhouse)으로 교체 | [basic-sky](features/basic-sky/NOTES.md) |
| 잔물결 텍스처 5종 중 레이어별 선택 | [normal-select](features/normal-select/NOTES.md) |
| 설정을 보기·빛·물 세 창으로 분리, 쉬운 이름, 파도 방향 표시 | [ui-panels](features/ui-panels/NOTES.md) |
| 파도 Simple 모드: 바람 방향·퍼짐·크기·높이·거칠기·속도 6개 값으로 파도 4개를 자동 생성, 개별 조정은 Advanced, 직접 고치면 Custom 표시 | [wave-macro](features/wave-macro/NOTES.md) |
| 먼 바다의 파도: 가까이는 메시, 중간은 픽셀 단위 기울기, 아주 먼 곳은 거칠기(넓은 반짝임)로 이어지는 3구간 처리와 구간 확인용 디버그 뷰 | [wave-far-normals](features/wave-far-normals/NOTES.md) |
| 노멀맵 바람 정렬(Align to wind)과 텍스처별 무늬 방향 측정 도구 | [wave-far-normals](features/wave-far-normals/NOTES.md) |
| 먼 바다 햇빛 반짝임 폭 조절(Far spread): 해가 수평선에 걸린 장면 연출용 | [wave-far-normals](features/wave-far-normals/NOTES.md) |
| 캡처 도구: 고정 프리셋 파일로 찍기, 기능 켜고 끄기 옵션, 해를 등진 고정 샷 | [wave-far-normals](features/wave-far-normals/NOTES.md) |
| 세 프리셋을 테마로 재구성: 잔잔한 호수(Basic) / 바람 센 노을 바다(Sunset) / 에메랄드빛 바다(Tropical). 테마마다 파도 모양과 노멀맵 조합이 다름 | [preset-themes](features/preset-themes/NOTES.md) |
| 프리셋을 작은 평면 / 오션 그리드별로 따로 저장하고, 평면을 바꾸면 그 평면의 버전을 자동 적용 | [mesh-presets](features/mesh-presets/NOTES.md) |
| 위에서 내려다보는 top 샷 전용 태양(방향 + 세기): 정반사가 카메라로 오도록 해를 평면 정면에 둠 | [top-view-sun](features/top-view-sun/NOTES.md) |
| 오션 고정 샷을 수면 역광 / 항공 부감 / 망원 압축으로 재구성 | [ocean-shots](features/ocean-shots/NOTES.md) |
| 포트폴리오 메인용 고정 샷 `ocean_hero`: 하늘 약 1 : 물 약 3의 저공 사선 구도, 세 프리셋이 같은 위치를 공유 | [ocean-hero](features/ocean-hero/NOTES.md) |
| Ocean Detail: 원경 리플 글리터(멀어서 안 보이는 잔물결 → 넓은 반짝임), 돌풍 패치(바람에 흐르는 거친 곳/잔잔한 곳), 수평선 연무, 먼 파도 마루 조절과 확인용 디버그 뷰 7. 기본은 꺼짐 | [ocean-hero](features/ocean-hero/NOTES.md) |
| 캡처 도구: 프리셋·샷 골라 찍기, 카메라 슬롯 캡처, 무손실 PNG, 창 크기와 별개인 고해상도 렌더(`--render-size`) | [ocean-hero](features/ocean-hero/NOTES.md) |

## 2026-05 — 기존 버전

DirectX 11 기반 구축(dx11-setup, shader-hot-reload, asset-pipeline), 조명(scene-lighting), 큐브맵(env-cubemap), Sine wave 수면(water-base), 노멀맵(water-normal-map).
