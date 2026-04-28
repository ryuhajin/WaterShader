# 스타일라이즈드 물 셰이더 포트폴리오

아트 셰이더 디자인 직무 지원을 위한 DirectX11 / HLSL 포트폴리오 프로젝트입니다.

이 프로젝트는 스타일라이즈드 수면 셰이더를 중심으로, HLSL 구현력과 아트 디렉션 조절 능력을 함께 보여주는 것을 목표로 합니다. 핵심 목표는 완성된 HLSL 결과물, 직접 제작한 텍스처/마스크 활용, 명확한 파라미터 제어, 공개 GitHub 링크로 제출하기 좋은 제작 과정을 정리하는 것입니다.

## 현재 상태

계획 문서와 프로젝트 구조를 준비하는 단계입니다.

## 목표 산출물

- DirectX11 실행 데모
- HLSL vertex shader / pixel shader
- UV 스크롤, 노멀 블렌딩, fresnel, foam, ripple mask를 포함한 스타일라이즈드 수면
- 직접 제작한 텍스처 또는 마스크 에셋
- ImGui 기반 파라미터 조절 UI
- 30-60초 데모 영상
- 포트폴리오 breakdown 문서

## 문서

- [포트폴리오 개요](docs/portfolio/OVERVIEW.md)
- [셰이더 breakdown](docs/portfolio/BREAKDOWN.md)
- [10일 로드맵](docs/portfolio/ROADMAP_10_DAYS.md)

## 빌드

DirectX11 프로젝트 초기 세팅이 끝난 뒤 빌드 방법을 추가할 예정입니다.

예정된 의존성 관리 방식:

- Visual Studio 2022
- DirectX11
- HLSL
- vcpkg manifest mode
- DirectXTK
- ImGui
