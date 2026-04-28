# 셰이더 Breakdown

이 문서는 물 셰이더의 기술적 구조와 아트 의도를 설명하는 공개 breakdown 문서로 사용합니다.

## 시각 요소

계획 중인 구성 요소:

- 얕은 물/깊은 물 색상 파라미터를 이용한 기본 수면 색
- 수면 움직임을 만드는 UV 스크롤 레이어
- 시간에 따라 블렌딩되는 두 개의 normal 또는 flow detail 레이어
- 시선 각도에 따른 반사감을 위한 fresnel highlight
- 텍스처 노이즈와 threshold 파라미터로 제어하는 foam mask
- 간단한 signed-distance 방식의 원형 ripple mask
- 서로 다른 아트 방향을 보여주기 위한 color preset

## HLSL 구조

계획 중인 셰이더 파일:

- 수면 plane transform과 선택적 vertex motion을 처리하는 vertex shader
- 수면 색, foam, fresnel, ripple을 합성하는 pixel shader
- 셰이더가 충분히 커질 경우 공용 함수를 분리하는 include 파일

구현은 읽기 쉽고 검토하기 쉬운 구조를 우선합니다. 많은 기능을 넣는 것보다 포트폴리오에서 의도와 완성도가 분명하게 보이는 것이 더 중요합니다.

## 파라미터

노출 예정 파라미터:

- `waterColorShallow`
- `waterColorDeep`
- `normalStrength`
- `uvSpeedA`
- `uvSpeedB`
- `fresnelPower`
- `foamThreshold`
- `foamSoftness`
- `rippleCenter`
- `rippleRadius`
- `rippleWidth`
- `rippleStrength`

## 텍스처와 마스크 에셋

프로젝트에는 직접 제작한 텍스처 또는 마스크 에셋을 최소 2개 포함합니다.

- Foam mask 또는 foam noise
- Flow noise, ripple noise, normal-detail texture 중 하나 이상

최종 breakdown에서는 각 텍스처가 최종 이미지에 어떻게 기여하는지 보여줍니다.

## 최종 캡처 체크리스트

- Beauty shot
- 파라미터 조절 장면
- Foam-only 또는 mask breakdown 장면
- Ripple 시연 장면
- Before/after 또는 layer breakdown
