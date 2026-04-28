# 스타일라이즈드 물 셰이더 포트폴리오

아트 셰이더 디자인 직무 지원을 위한 DirectX11 / HLSL 포트폴리오 프로젝트입니다.

이 프로젝트는 스타일라이즈드 수면 셰이더를 중심으로, HLSL 구현력과 아트 디렉션 조절 능력을 함께 보여주는 것을 목표로 합니다. 핵심 목표는 완성된 HLSL 결과물, 직접 제작한 텍스처/마스크 활용, 명확한 파라미터 제어, 공개 GitHub 링크로 제출하기 좋은 제작 과정을 정리하는 것입니다.

## 현재 상태

DirectX11 초기 세팅을 시작했습니다. 현재 목표는 Visual Studio 2022와 CMake로 실행 가능한 Win32 / D3D11 데모를 만들고, HLSL 파일을 분리해 컴파일 경로를 확인하는 것입니다.

기능 단위로 브랜치와 spec 문서를 관리합니다. 작업 전에 `docs/CONVENTIONS.md`를 먼저 확인하세요. 프로젝트 개요는 `docs/OVERVIEW.md`, 일정은 `docs/ROADMAP.md` 입니다.

## 목표 산출물

- DirectX11 실행 데모
- HLSL vertex shader / pixel shader
- UV 스크롤, 노멀 블렌딩, fresnel, foam, ripple mask를 포함한 스타일라이즈드 수면
- 직접 제작한 텍스처 또는 마스크 에셋
- ImGui 기반 파라미터 조절 UI
- 30-60초 데모 영상
- 포트폴리오 breakdown 문서

## 빌드

필요 도구:

- Visual Studio 2022, Desktop development with C++
- CMake
- vcpkg

의존성 관리 방식:

- vcpkg manifest mode
- DirectXTK
- ImGui

예상 빌드 흐름:

```powershell
$env:VCPKG_ROOT="C:\path\to\vcpkg"
cmake --preset vs2022
cmake --build --preset vs2022-debug
```

아직 `cmake`와 `vcpkg`가 PATH에서 확인되지 않는 환경이라면, Visual Studio Installer와 vcpkg 설치/환경 변수 설정을 먼저 확인해야 합니다.

## 기술 스택

- HLSL
- DirectX11
- DirectXTK
- ImGui
