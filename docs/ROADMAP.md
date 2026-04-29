# 9일 로드맵 (마감 2026-05-07 목)

이 로드맵은 큰 흐름을 잡기 위한 것이고, 실제 작업은 `features/<name>/` 단위로 진행합니다.
각 Day의 산출물이 어떤 feature 폴더로 떨어지는지 함께 표기합니다.

각 항목은 머지 시 `[x]`로 체크합니다.

전략: 환경 세팅(라이팅·시간 cbuffer·큐브맵)을 Day 2~3에 마무리한 뒤, Day 4부터 본격적으로 HLSL을 짜며 water/foam/ripple 작업.

---

## Day 1 — 2026-04-29 수 — 인프라 완료 ✓

이미 완료된 항목 (앞당겨 진행한 것 포함):

- [x] DirectX11 프로젝트 초기화, vcpkg manifest, DirectXTK + ImGui 연동 → `feature/dx11-setup`
- [x] HLSL 파일 저장 시 즉시 재컴파일 → `feature/shader-hot-reload`
- [x] OBJ 임포트(tinyobjloader), DSV depth buffer, RasterizerState(FrontCCW), Model Rotation/Camera/FOV 워크벤치 컨트롤, WASD/화살표키 카메라 input → `feature/asset-pipeline`

**완료 기준:** plane이 화면에 그려지고, 셰이더 저장 즉시 결과 반영, 카메라가 자유 이동/회전 가능.

---

## Day 2 — 2026-04-30 목 — 라이팅 + 시간 cbuffer → `feature/scene-lighting`

- [ ] cbuffer 분리: `g_World` / `g_View` / `g_Projection` (현재는 `g_MVP` 한 개) — normal 변환과 world-space 라이팅 가능하게
- [ ] 방향광 cbuffer 추가: `g_LightDirection`, `g_LightColor`(rgb), `intensity`(g_LightColor.a 또는 별도)
- [ ] 시간 cbuffer 추가: `g_Time` (float, 누적)
- [ ] 단순 Lambert 라이팅으로 검증 (`saturate(dot(N, L))`)
- [ ] ImGui: Light Direction 카테고리 (Yaw 0~360, Pitch -90~90), Light Color R/G/B 0~255 SliderInt, Intensity 0~3 SliderFloat, Reset Light

**완료 기준:** plane을 회전시키면 자세에 따라 명암 변화. Light Direction 슬라이더 즉시 반영. `g_Time`이 흐름 (ImGui Text로 표시).

---

## Day 3 — 2026-05-01 금 — 환경 큐브맵 + Skybox → `feature/env-cubemap`

- [ ] DDS 큐브맵 로딩 (DirectXTK `DDSTextureLoader`)
- [ ] Skybox 렌더 (큐브 또는 fullscreen quad + reverse projection)
- [ ] `TextureCube` + `SamplerState` 셰이더 바인딩
- [ ] plane에 reflect 벡터로 환경 샘플링 (간단 반사)
- [ ] ImGui: Skybox on/off 토글, 반사 강도 슬라이더

**완료 기준:** plane이 환경을 반사. 배경에 skybox가 보임. 카메라 회전에 따라 반사가 자연스럽게 추적.

> 큐브맵은 `assets/textures/` 폴더에 `.dds` 한 장. 첫 텍스처라 `assets/textures/` 폴더가 이때 생성됨. 무료 HDRI를 cmft/Lys로 .dds 변환하거나 DirectXTK 샘플 큐브맵 활용.

---

## Day 4 — 2026-05-02 토 — Water Base → `feature/water-base`

- [ ] UV scroll (`g_Time` 활용, 두 방향 different speed)
- [ ] normal map 두 장 블렌드 (기본 + detail)
- [ ] Fresnel 항: `pow(1 - dot(N, V), power)`
- [ ] Shallow/Deep color 블렌딩 (camera 거리 또는 fresnel 기반)
- [ ] 양면 그리기 RasterizerState (water plane은 단면이라 양면 필요)

**완료 기준:** 수면이 흐르고, 시야각에 따라 fresnel로 reflection이 강조되며, 깊이감 있는 색.

---

## Day 5 — 2026-05-03 일 — Foam + Ripple → `feature/foam-mask`, `feature/ripple-sdf`

- [ ] Foam mask: depth 차이 기반 또는 noise + threshold + softness
- [ ] SDF 방식의 원형 ripple (시간 기반 펼침)
- [ ] 자체 제작 noise/mask 텍스처 1장 이상 연결
- [ ] ImGui: foam threshold/softness, ripple radius/speed

**완료 기준:** foam이 물결 위에 점점이 떠 있고, ripple이 한 점에서 펼쳐지는 동작.

---

## Day 6 — 2026-05-04 월 — 폭포 + 아트 디렉션 → `feature/waterfall`, `feature/color-presets`

- [ ] 폭포 ribbon 메시 (Blender plane + 약간 굽음) export
- [ ] 폭포용 셰이더 (UV scroll 빠르게 + foam at edges)
- [ ] 메인 비주얼 목표 고정 (참고 이미지 references/ 저장)
- [ ] 색상/분위기 preset 2개

**완료 기준:** 물 + 폭포가 한 씬에 함께 보이며 아트 방향이 잡힘. 포트폴리오 대표 스크린샷 1장 캡처 가능.

---

## Day 7 — 2026-05-05 화 — 안정화 + ImGui Polish + 텍스처 다듬기 → `feature/polish`

- [ ] 셰이더 파라미터 ImGui 노출 정리 (속도/색상/fresnel/foam/ripple 한 패널에 정돈)
- [ ] 텍스처/마스크 에셋 다듬기 (foam, noise, surface detail)
- [ ] 렌더링 문제 수정 (깨짐, NaN, edge case)
- [ ] 데모 영상용 파라미터 조절 시퀀스 준비

**완료 기준:** 셰이더를 실시간 조절해 데모 가능. 자체 제작 텍스처가 결과물에 보임.

---

## Day 8 — 2026-05-06 수 — 캡처 + 문서 → `feature/demo-capture`, `feature/breakdown-doc`

- [ ] 30~60초 데모 영상 녹화 (parameter 변화 + camera fly-through)
- [ ] Beauty shot 1~2장, breakdown shot (UV viz / normal viz / foam viz)
- [ ] README 빌드/실행 방법
- [ ] 공개 shader breakdown 문서 (한국어, 평가자가 코드 안 보고 이해 가능 수준)

**완료 기준:** 빌드 안 해도 결과물 + 의도가 보임.

---

## Day 9 — 2026-05-07 목 — 최종 패키징 + 제출

- [ ] README 최종 정리
- [ ] 공개 문서 최종 정리
- [ ] GitHub repo 정리 (private notes 추적 안 됨, archive/ 정리)
- [ ] 포트폴리오 PDF 또는 링크 모음 정리
- [ ] **제출**

**완료 기준:** GitHub 링크 + 영상 + breakdown PDF가 한 묶음으로 제공 가능.

---

## 메모

- 일정은 압축적이라 우선순위가 낮은 항목(예: ripple SDF, color preset 2개째)은 잘라낼 준비. 핵심 산출물(water + 폭포 + 영상 + breakdown)을 우선.
- **그림자: shadow map 구현 안 함.** 폭포 충돌부 depth-fade darkening, 바위 밑 베이크 AO 등 셰이더 트릭으로 fake 처리 (water-base/foam-mask/waterfall 단계에 흡수).
- UE5 머티리얼 포팅은 본 일정 후 별도 (`feature/ue5-port`).
- 매 Day 시작 시 SPEC을 main에 doc-only commit 후 `feature/<name>` 브랜치에서 구현 — 컨벤션 그대로.
