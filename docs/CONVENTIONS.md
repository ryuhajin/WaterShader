# 문서 & 워크플로 컨벤션

이 문서는 WaterShader 포트폴리오 프로젝트의 **문서 작성 규칙**과 **기능 단위 브랜치 워크플로**를 정의합니다.
새 기능을 시작하기 전에 항상 이 문서를 먼저 확인합니다.

## 디렉토리 구조

```text
docs/
  CONVENTIONS.md           # 이 문서 — 모든 규칙의 원본
  OVERVIEW.md              # 프로젝트 목적, 방향, 평가 포인트 (로컬 전용, 추적 안 함)
  ROADMAP.md               # 진행 중 / 보류 / 완료 이력 → feature 매핑 (로컬 전용)
  decisions.md             # 기술 의사결정 로그 (선택 이유 기록) (로컬 전용)
  CHANGELOG.md             # 사용자용 버그 수정·퀄리티 업 요약
  readme-images/           # README가 쓰는 이미지 (추적)
  features/
    _TEMPLATE/
      SPEC.md              # 새 기능 시작 시 복사해서 쓰는 템플릿
    <feature-name>/
      SPEC.md              # 필수 — 4개 핵심 섹션
      NOTES.md             # 선택 — 작업 중 발견/실험/막힌 점 메모
      references/          # 선택 — 외부 참고 이미지 (로컬 전용, README.md만 추적)

../ws-capture/             # 저장소 밖 — 캡처 전용 (git 추적 안 함)
  <feature-name>/
    captures/<label>/      # --capture / Capture 버튼으로 찍은 세트
    compare*/              # make_compare.ps1 비교 시트
  portfolio/               # 2026-10 포트폴리오 페이지용 PNG 세트
```

- 활성 작업은 `features/<name>/` 안에서만 일어납니다.
- 더 이상 참조하지 않을 문서는 삭제합니다 (추적 문서는 git 기록에 남음).

## 캡처와 레퍼런스

- 캡처는 저장소 옆 `ws-capture/` 에 저장됩니다 (CMake `WATERSHADER_CAPTURE_DIR`, 실행 인자 `--capture-root`로 변경 가능). Debug·Release 모두 같은 곳.
- NOTES·SPEC에 적힌 `captures/<label>/`, `compare/` 경로는 **`ws-capture/<feature>/captures/<label>/`, `ws-capture/<feature>/compare/`** 를 뜻합니다 (2026-10-23 이전 기록 포함).
- 문서에서 캡처에 링크할 때는 `../../../../ws-capture/<feature>/...` 상대 경로를 씁니다. 공개 저장소에서는 열리지 않는 로컬 링크입니다. 공개용 이미지는 `docs/readme-images/` 에 골라 넣습니다.
- 외부 레퍼런스(게임 스크린샷, 출처 미기재 사진)는 `references/` 에 두되 `.gitignore`로 올리지 않습니다. 설명과 출처는 `references/README.md`에 씁니다.

### 회귀·동일성 캡처는 남기지 않는다 (2026-10-24)

결과가 "같다 / 다르다" 한 줄인 캡처는 `ws-capture`에 쌓지 않습니다.

- **대상**: 회귀 확인(기존 샷이 기준과 같은가 — `regress`, `rt`, `main_check`, `compile_check` 같은 것), 동일성 쌍(효과 없음 = 효과 끔 — `pass_on` / `pass_off` 같은 것).
- **순서**: 임시 폴더(`--capture-root $env:TEMP\ws-check`)에 찍는다 → `tools\compare_sets.ps1`로 비교한다 → NOTES 검증 표에 결과 한 줄("21장 바이트 동일")과 비교 명령을 적는다 → 임시 폴더를 지운다.
- **다를 때만 남긴다**: `-KeepDiffTo ws-capture\<기능>\captures\<라벨>_diff`를 주면 다른 이미지만 `<이름>.A` / `<이름>.B` 쌍으로 남는다. NOTES에 어느 샷이 왜 달랐는지 적는다.
- **기준 세트는 남긴다**: 비교의 기준인 `waterfront/captures/baseline` 같은 before 세트, 단계별 결과, 디버그 화면, 숫자 비교 도구 입력은 지금처럼 `ws-capture`에 둔다.

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

## 단계형 업그레이드 (waterfront)

[features/waterfront/PLAN.md](features/waterfront/PLAN.md)처럼 여러 단계로 나뉜 작업은 위 브랜치 규칙에 더해:

- **한 번에 한 단계만** 구현합니다. 사용자의 명시적 승인 전에는 다음 단계를 시작하지 않습니다.
- 자동 검증 통과, 캡처 생성, 무응답은 승인이 아닙니다. 에이전트 검증 결과와 사용자 승인 상태를 따로 기록합니다.
- 단계마다: 개념 설명 → 구현 → 에이전트 검증 → 사용자 검증 자료(조작법, 정상/실패 징후) → 승인 → 기록.
- 단계마다 같은 카메라·시간·노출의 OFF/ON 캡처를 남깁니다.

### 테스트 장면 이름 (2026-11-17 변경)

두 테스트 장면의 이름을 내용이 드러나게 바꿨습니다. 업그레이드 전체 이름 `waterfront`(폴더, `ws-capture/waterfront/captures/baseline`)와 완료된 기능 이름(`water-lab` 등)은 그대로입니다.

| 옛 이름 | 새 이름 | 내용 |
|---|---|---|
| Water Lab | **Deep Stair** | 깊은 물 검증 장면 — 알려진 깊이(0.2/1/3/6m 받침대, 8m 바닥), 계단, 관통 기둥 |
| Waterfront (그레이박스) | **Shallow Rail** | 얕은 물 목표 장면 — 승강장, 잠긴 철로(자갈 둑·침목·레일), 전주 |
| Lab Waves, `--lab-waves` | Scene Waves, `--scene-waves` | 테스트 장면의 Gerstner 파도 |

| 옛 샷 이름 | 새 샷 이름 |
|---|---|
| `lab_top` / `lab_stairs` / `lab_low` | `stair_top` / `stair_angle` / `stair_low` |
| `lab_pillar` / `lab_pillar_near` | `stair_pillar` / `stair_pillar_near` |
| `front_rails` | `rail_line` |

- 2026-11-17 이전의 SPEC·NOTES와 `ws-capture` 캡처 파일명은 **옛 이름 그대로** 둡니다 (기록).
- `--shot`, `--capture-shots`, `--lab-waves`는 옛 이름도 받습니다. 캡처는 항상 새 이름으로 저장됩니다.
- `tools/compare_sets.ps1`, `tools/compare_depth.ps1`은 옛 이름 파일과 새 이름 파일을 같은 샷으로 짝짓습니다 (`tools/shot_names.ps1`, 코드의 `kShotAliases`와 같은 표).

## SPEC.md 작성 규칙

모든 `docs/features/<name>/SPEC.md`는 다음 4개 섹션을 **반드시** 포함합니다:

1. **Goal / Visual Target** — 시각적으로 무엇을 달성할지. 참고 이미지/영상 링크 포함. 스코프 가드 ("절대로 만들지 않을 것") 포함.
2. **HLSL 접근법 / 수식 / 의사코드** — 사용할 핵심 함수, 수식, 단계별 의사코드, 참고 자료.
3. **Inputs / Outputs** — 텍스처, constant buffer, ImGui 파라미터 범위. 표 형식 권장.
4. **Acceptance Criteria / Test Plan** — 완료 기준 체크리스트.

상세 형식은 `docs/features/_TEMPLATE/SPEC.md` 참조.

## NOTES.md 사용법 (선택)

작업 중 발견한 사항, 실험 결과, 막힌 점을 기록합니다. SPEC은 "무엇을 만들지"고 NOTES는 "만드는 동안 무슨 일이 있었는지"입니다. 시간 순서대로 추가하고, 머지 후에도 지우지 않습니다.

### 과정에서 고친 것 (필수 칸)

NOTES가 있는 기능은 **"과정에서 고친 것"** 칸을 반드시 둡니다. 개선 과정 자체가 포트폴리오 근거가 되기 때문입니다.

- 형식은 표 하나: `| 증상 | 원인 | 해결 |`
- 넣는 것: 화면 결과·동작·검증 방법에 영향을 준 수정 (처음 결과가 틀려서 바꾼 값, 버그, 검증 도구의 결함, 사용자 피드백으로 바꾼 UI·이름)
- 빼는 것: 오타, 바로 고친 컴파일 오류처럼 결과에 흔적이 남지 않는 것
- 고친 것이 없으면 칸을 지우지 말고 "없음"이라고 적습니다.
- 고친 그 시점에 적습니다. 나중에 채우면 원인이 흐려집니다.

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
- [ ] (선택) `ws-capture/<feature>/captures/`에 결과 캡처 1세트 이상

## 이 컨벤션 자체를 바꿀 때

이 문서의 규칙이 실제 작업과 안 맞기 시작하면 — 규칙을 깨고 작업하지 말고 — 이 문서를 먼저 고칩니다. 변경 사유는 `docs/decisions.md`에 한 줄 적습니다.
