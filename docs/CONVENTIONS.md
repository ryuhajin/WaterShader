# 문서 & 워크플로 컨벤션

이 문서는 WaterShader 포트폴리오 프로젝트의 **문서 작성 규칙**과 **기능 단위 브랜치 워크플로**를 정의합니다.
새 기능을 시작하기 전에 항상 이 문서를 먼저 확인합니다.

## 디렉토리 구조

```text
docs/
  CONVENTIONS.md           # 이 문서 — 모든 규칙의 원본
  OVERVIEW.md              # 프로젝트 목적, 산출물, 평가 포인트
  ROADMAP.md               # 큰 흐름과 일자별 → feature 매핑
  decisions.md             # 기술 의사결정 로그 (선택 이유 기록)
  features/
    _TEMPLATE/
      SPEC.md              # 새 기능 시작 시 복사해서 쓰는 템플릿
    <feature-name>/
      SPEC.md              # 필수 — 4개 핵심 섹션
      NOTES.md             # 선택 — 작업 중 발견/실험/막힌 점 메모
      references/          # 선택 — 참고 이미지, 영상 캡처
  archive/                 # 더 이상 active 하지 않은 문서 보존소
```

- 활성 작업은 `features/<name>/` 안에서만 일어납니다.
- 더 이상 참조하지 않을 문서는 삭제하지 말고 `archive/` 로 보냅니다.

## 기능 이름 규칙

- **kebab-case**를 사용합니다 (예: `water-base`, `foam-mask`, `hlsl-hot-reload`, `imgui-panel`).
- 도메인 prefix(`shader-`, `vfx-`)는 붙이지 않습니다. 모든 기능이 셰이더 포트폴리오 맥락 안에 있습니다.
- 한 기능당 한 폴더, 한 SPEC. 너무 큰 기능은 쪼개고, 너무 작으면 합칩니다.

## 브랜치 워크플로

1. **메인 브랜치는 항상 빌드 가능 상태를 유지합니다.**
2. 새 기능 시작 시:
   1. `docs/features/_TEMPLATE/SPEC.md`를 `docs/features/<name>/SPEC.md`로 복사하고 4개 섹션을 채웁니다.
   2. `main`에 SPEC만 단독 커밋합니다 (예: `docs: add spec for water-base`).
   3. `feature/<name>` 브랜치를 따고 그 위에서 구현합니다.
   - 이유: SPEC을 main에 먼저 박아두면, 구현이 미완이거나 폐기되어도 어떤 기능을 시도했는지 기록이 남습니다.
3. 구현이 완료되면:
   1. `feature/<name>` → `main` 머지.
   2. `docs/ROADMAP.md`의 해당 항목을 `[x]`로 체크.
   3. 브랜치 삭제 (`git branch -d feature/<name>`).
4. 부분 완성 상태로 다른 기능을 시작해야 하면:
   - 미완 기능의 `NOTES.md`에 carry-over 항목을 적습니다.
   - 브랜치는 살려둡니다 (삭제 금지).

## SPEC.md 작성 규칙

모든 `docs/features/<name>/SPEC.md`는 다음 4개 섹션을 **반드시** 포함합니다:

1. **Goal / Visual Target** — 시각적으로 무엇을 달성할지. 참고 이미지/영상 링크 포함. 스코프 가드 ("절대로 만들지 않을 것") 포함.
2. **HLSL 접근법 / 수식 / 의사코드** — 사용할 핵심 함수, 수식, 단계별 의사코드, 참고 자료.
3. **Inputs / Outputs** — 텍스처, constant buffer, ImGui 파라미터 범위. 표 형식 권장.
4. **Acceptance Criteria / Test Plan** — 완료 기준 체크리스트.

상세 형식은 `docs/features/_TEMPLATE/SPEC.md` 참조.

## NOTES.md 사용법 (선택)

작업 중 발견한 사항, 실험 결과, 막힌 점을 기록합니다. SPEC은 "무엇을 만들지"고 NOTES는 "만드는 동안 무슨 일이 있었는지"입니다. 시간 순서대로 추가하고, 머지 후에도 지우지 않습니다.

## 외부 자료 인용

논문, 블로그, 튜토리얼, 다른 셰이더의 기법을 가져올 때는 SPEC 또는 NOTES에 **출처 URL을 명시**합니다. 셰이더는 다른 사람의 기법을 빌려오는 일이 잦으므로 추적 가능성이 중요합니다.

## 문서 언어

- 문서는 **한국어**로 작성합니다.
- 코드 식별자, HLSL 함수명, 라이브러리 이름은 영어 그대로 둡니다.

## 머지 시 체크리스트

기능을 main에 머지하기 전:

- [ ] SPEC의 Acceptance Criteria 모두 체크됨
- [ ] 빌드 깨짐 없음
- [ ] `docs/ROADMAP.md`의 해당 항목 업데이트됨
- [ ] (선택) `references/`에 결과 캡처 1개 이상

## 이 컨벤션 자체를 바꿀 때

이 문서의 규칙이 실제 작업과 안 맞기 시작하면 — 규칙을 깨고 작업하지 말고 — 이 문서를 먼저 고칩니다. 변경 사유는 `docs/decisions.md`에 한 줄 적습니다.
