# 포트폴리오 개요

## 목표

아트 셰이더 디자인 직무 지원을 위해, 완성된 DirectX11 HLSL 물 셰이더를 보여주는 공개 GitHub 포트폴리오 프로젝트를 만듭니다.

평가자가 몇 분 안에 다음 내용을 파악할 수 있어야 합니다.

- 셰이더를 HLSL로 직접 구현했습니다.
- 결과물이 아트 셰이더로 판단 가능한 수준까지 완성되어 있습니다.
- 직접 제작한 텍스처와 마스크를 의도적으로 사용했습니다.
- 아트 디렉션을 조절할 수 있도록 주요 파라미터를 노출했습니다.
- 구현 구조와 제작 의도를 검토하기 쉽게 문서화했습니다.

## 메인 주제

메인 주제는 폭포가 아니라 스타일라이즈드 호수/수면 셰이더입니다.

10일 안에 완성도를 확보하면서도 중요한 셰이더 역량을 보여주기 위한 선택입니다.

- UV 스크롤
- 노멀 블렌딩
- Fresnel
- 얕은 물/깊은 물 색상 블렌딩
- Foam mask
- SDF 방식의 ripple mask
- Alpha blending
- 텍스처 기반 수면 변화

## 기술 방향

- 런타임 데모: DirectX11
- 셰이더 구현: HLSL
- 의존성 관리: vcpkg manifest mode
- 렌더링 헬퍼: DirectXTK
- 파라미터 UI: ImGui

UE5 머티리얼 포팅은 보너스 항목으로만 둡니다. 메인 제출물은 DirectX11 HLSL 결과물, 데모 영상, README, breakdown 문서를 우선합니다.

## 공개 GitHub 정책

공개 문서는 날것의 작업 로그가 아니라 포트폴리오 설명서처럼 읽히도록 정리합니다.

공개:

- `README.md`
- `docs/portfolio/OVERVIEW.md`
- `docs/portfolio/BREAKDOWN.md`
- `docs/portfolio/ROADMAP_10_DAYS.md`

비공개:

- `docs/portfolio/daily/`
- `docs/portfolio/notes/`
- `docs/portfolio/*.private.md`
