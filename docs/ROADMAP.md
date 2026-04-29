# 10일 로드맵

이 로드맵은 큰 흐름을 잡기 위한 것이고, 실제 작업은 `features/<name>/` 단위로 진행합니다.
각 Day의 산출물이 어떤 feature 폴더로 떨어지는지 함께 표기합니다.

각 항목은 머지 시 `[x]`로 체크합니다.

---

## Day 1 — DirectX11 세팅 → `feature/dx11-setup`

- [x] DirectX11 프로젝트 초기화
- [x] vcpkg manifest mode 의존성 셋업
- [x] DirectXTK 연결, ImGui 연동 준비
- [x] clear color 또는 간단한 plane 렌더
- [x] HLSL shader compile 경로 확인

**완료 기준:** 창이 뜨고 간단한 shader 기반 렌더링이 보임.

> 비고: 이미 `src/`에 `D3DClass`, `Camera`, `ColorShader`, `Light`, `Model` 등 기본 클래스가 들어와 있음. SPEC 작성 시 어디까지 만들었고 무엇이 남았는지 정리.

---

## Day 2 — 기본 수면 셰이더 → `feature/water-base`

- [ ] UV 스크롤 구현
- [ ] 두 개의 normal/detail 레이어 블렌딩
- [ ] Fresnel 추가
- [ ] 얕은 물 / 깊은 물 색상 블렌딩

**완료 기준:** 수면 움직임과 아트 디렉션 가능한 색상 조절이 보임.

---

## Day 3 — Foam과 Ripple → `feature/foam-mask`, `feature/ripple-sdf`

- [ ] Foam mask 로직 (threshold, softness)
- [ ] SDF 방식의 원형 ripple
- [ ] 직접 제작한 mask/noise texture 1장 이상 연결

**완료 기준:** 장면 안에서 foam과 ripple 동작이 보임.

---

## Day 4 — 아트 디렉션 → `feature/color-presets`

- [ ] 메인 비주얼 목표 고정 (참고 이미지 references/ 에 저장)
- [ ] 색상/분위기 preset 2개
- [ ] 수면, foam, fresnel, ripple 값 튜닝

**완료 기준:** 포트폴리오 대표 스크린샷 1장 캡처 가능.

---

## Day 5 — ImGui 컨트롤 → `feature/imgui-panel`

- [ ] 핵심 shader 파라미터 노출 (속도, 색상, fresnel, foam, ripple)
- [ ] 데모 영상용 파라미터 조절 장면 준비

**완료 기준:** 셰이더를 실시간으로 조절할 수 있음.

---

## Day 6 — 안정화 + 핫리로드 → `feature/hlsl-hot-reload`

- [ ] 렌더링 문제 수정
- [ ] 텍스처 경로 검증
- [ ] shader compile 에러 처리 개선
- [ ] **core shader가 안정적일 때만** HLSL hot reload 추가 (선택적)

**완료 기준:** 데모가 안정적으로 실행됨.

---

## Day 7 — 텍스처 Polish → `feature/texture-assets`

- [ ] 직접 제작한 텍스처/마스크 에셋 다듬기
- [ ] Foam, noise, surface detail 개선
- [ ] texture breakdown 이미지 캡처

**완료 기준:** 직접 제작한 텍스처 작업이 결과물과 문서에서 보임.

---

## Day 8 — 문서화 → `feature/breakdown-doc`

- [ ] README 빌드/실행 방법 초안
- [ ] 공개 shader breakdown 문서
- [ ] 포트폴리오 PDF 구성 준비

**완료 기준:** 글만 읽어도 결과물의 의도와 구조를 이해할 수 있음.

---

## Day 9 — 캡처 → `feature/demo-capture`

- [ ] 30~60초 데모 영상 녹화
- [ ] Beauty shot, breakdown shot 캡처
- [ ] 공개 링크와 미디어 참조 확인

**완료 기준:** 빌드하지 않아도 결과물을 이해할 수 있음.

---

## Day 10 — 패키징

- [ ] README 최종 정리
- [ ] 공개 문서 최종 정리
- [ ] GitHub 제출용 저장소 정리
- [ ] private notes가 추적되지 않는지 확인

**완료 기준:** GitHub 저장소 링크 제출 가능.

---

## 메모

- Day 단위는 일정 가이드일 뿐, 실제로는 feature 머지 단위로 진행. 일정이 밀리면 우선순위가 낮은 항목(예: hot reload, color presets 2개)을 잘라냄.
- UE5 머티리얼 포팅은 모든 main 산출물이 끝난 뒤에만 시도. SPEC 작성 시 별도 feature(`feature/ue5-port`)로 분리.
