# 프로젝트 개요

## 한 줄 요약

아트 셰이더 디자인 직무 지원을 위한 **DirectX11 + HLSL 기반 스타일라이즈드 수면 셰이더** 포트폴리오.

지원 마감: **2026-05-11 15:00** (펄어비스 아트_셰이더 디자인 인턴십, 출처: [Pearl Abyss Careers](https://www.pearlabyss.com/ko-KR/Company/Careers/detail?_jobOpeningNo=797))

## 평가자에게 보여줄 것

평가자가 몇 분 안에 다음을 파악할 수 있어야 합니다.

- 셰이더를 HLSL로 직접 구현했다.
- 결과물이 아트 셰이더로 판단 가능한 수준까지 완성되어 있다.
- 직접 제작한 텍스처와 마스크를 의도적으로 사용했다.
- 아트 디렉션을 조절할 수 있도록 주요 파라미터를 노출했다.
- 구현 구조와 제작 의도를 검토하기 쉽게 문서화했다.

## 메인 주제

**스타일라이즈드 호수/수면**. 폭포가 아닙니다.

10일 안에 완성도를 확보하면서도 핵심 셰이더 역량을 두루 보여주기 위한 선택입니다. 이유와 대안 검토는 `decisions.md` 참조.

다룰 셰이더 기법:

- UV 스크롤
- 노멀 블렌딩 (2 레이어)
- Fresnel
- 얕은 물 / 깊은 물 색상 블렌딩
- Foam mask
- SDF 방식의 ripple mask
- Alpha blending
- 직접 제작한 텍스처/마스크 활용

## 목표 산출물

- DirectX11 실행 데모 (Win32, vcpkg manifest)
- HLSL vertex / pixel shader
- 직접 제작한 텍스처/마스크 에셋 최소 2장
- ImGui 기반 파라미터 조절 UI
- 30~60초 데모 영상
- README + 공개 breakdown 문서
- (선택) UE5 머티리얼 포팅 — 보너스 항목

## 기술 스택

- **런타임**: DirectX11 (Win32)
- **셰이더**: HLSL
- **의존성 관리**: vcpkg manifest mode
- **렌더링 헬퍼**: DirectXTK
- **파라미터 UI**: ImGui
- **빌드**: CMake + Visual Studio 2022

각 선택의 이유는 `decisions.md`에 기록됩니다.

## 진행 방식

- 작업은 `features/<name>/` 단위로 쪼개서 관리합니다 — 규칙은 `CONVENTIONS.md` 참조.
- 일자별 큰 흐름과 각 일자 → feature 매핑은 `ROADMAP.md` 참조.
- 더 이상 active 하지 않은 초기 계획 문서는 `archive/` 에 보존되어 있습니다.
