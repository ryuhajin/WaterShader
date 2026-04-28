# 10일 로드맵

## Day 1 - DirectX11 세팅

- DirectX11 프로젝트를 초기화합니다.
- 의존성은 vcpkg manifest mode로 관리합니다.
- DirectXTK를 연결하고 ImGui 연동 준비를 합니다.
- clear color 또는 간단한 plane을 렌더링합니다.
- HLSL shader compile 경로를 확인합니다.

완료 기준: 창이 뜨고 간단한 shader 기반 렌더링이 보입니다.

## Day 2 - 기본 수면 셰이더

- UV 스크롤을 구현합니다.
- 두 개의 normal 또는 detail 레이어를 블렌딩합니다.
- Fresnel을 추가합니다.
- 얕은 물/깊은 물 색상 블렌딩을 추가합니다.

완료 기준: 수면 움직임과 아트 디렉션 가능한 색상 조절이 보입니다.

## Day 3 - Foam과 Ripple

- Foam mask 로직을 추가합니다.
- Threshold와 softness 조절을 추가합니다.
- SDF 방식의 원형 ripple 로직을 추가합니다.
- 직접 제작한 mask 또는 noise texture를 최소 1개 연결합니다.

완료 기준: 장면 안에서 foam과 ripple 동작이 보입니다.

## Day 4 - 아트 디렉션

- 메인 비주얼 목표를 고정합니다.
- 색상 또는 분위기 preset 2개를 만듭니다.
- 수면, foam, fresnel, ripple 값을 튜닝합니다.

완료 기준: 포트폴리오 대표 스크린샷 1장을 캡처할 수 있습니다.

## Day 5 - ImGui 컨트롤

- 핵심 shader 파라미터를 노출합니다.
- 속도, 색상, fresnel, foam, ripple 컨트롤을 추가합니다.
- 데모 영상에 넣을 파라미터 조절 장면을 준비합니다.

완료 기준: 셰이더를 실시간으로 조절할 수 있습니다.

## Day 6 - 안정화

- 렌더링 문제를 수정합니다.
- 텍스처 경로를 검증합니다.
- shader compile 에러 처리를 개선합니다.
- core shader가 이미 안정적일 때만 HLSL hot reload를 추가합니다.

완료 기준: 데모가 안정적으로 실행됩니다.

## Day 7 - 텍스처 Polish

- 직접 제작한 텍스처와 마스크 에셋을 만들거나 다듬습니다.
- Foam, noise, surface detail을 개선합니다.
- 가능하면 texture breakdown 이미지를 캡처합니다.

완료 기준: 직접 제작한 텍스처 작업이 결과물과 문서에서 보입니다.

## Day 8 - 문서화

- README 빌드/실행 방법 초안을 작성합니다.
- 공개 shader breakdown 문서를 작성합니다.
- 포트폴리오 PDF 구성을 준비합니다.

완료 기준: 글만 읽어도 결과물의 의도와 구조를 이해할 수 있습니다.

## Day 9 - 캡처

- 30-60초 데모 영상을 녹화합니다.
- Beauty shot과 breakdown shot을 캡처합니다.
- 공개 링크와 미디어 참조를 확인합니다.

완료 기준: 빌드하지 않아도 프로젝트 결과물을 이해할 수 있습니다.

## Day 10 - 패키징

- README를 최종 정리합니다.
- 공개 문서를 최종 정리합니다.
- GitHub 제출용으로 저장소 내용을 정리합니다.
- ignore된 private notes가 추적되지 않는지 확인합니다.

완료 기준: GitHub 저장소 링크를 제출할 수 있습니다.
