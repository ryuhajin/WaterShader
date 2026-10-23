# Water Lab — 1단계 검증 장면

> Branch: `feature/water-lab` · Status: in-progress · Updated: 2026-10-23
> waterfront 업그레이드 1단계. 기준 상태: [../waterfront/BASELINE.md](../waterfront/BASELINE.md)

## 1. Goal / Visual Target

- **한 문장 요약:** 물 아래에 "알고 있는 깊이"의 불투명 물체들을 놓아, 다음 단계(투과·감쇠·굴절)를 숫자로 검증할 수 있는 장면을 만든다.
- **장면 두 개:**
  - **Water Lab** — 검증용. 체크무늬 바닥, 깊이 0.2 / 1 / 3 / 6m 받침대, 계단, 수면을 뚫고 올라오는 기둥, 수면 바로 아래(−5cm)·위(+5cm) 받침대, 자갈 바닥 구역.
  - **Waterfront 그레이박스** — 목표 장면의 배치 초안. 승강장, 승강장에서 물로 내려가는 계단, 얕게 잠긴 철로(자갈 둑·침목·레일), 전주.
- **단위:** 1 = 1m, Y-up, 수면 y = 0. 파도는 끈다.
- **이번 단계에서 물은 아직 불투명**이다. 그래서 확인은 "물 끄기"와 "수심 디버그 뷰"로 한다.
- **스코프 가드 — 만들지 않을 것:**
  - 물이 아래 장면을 읽는 것(투과·감쇠) — 2단계 이후
  - 그림자, 코스틱, 거품
  - Blender 메시, 외부 텍스처 다운로드 (자갈은 셰이더에서 절차적으로 만든다)

## 2. HLSL 접근법 / 수식 / 의사코드

**새 셰이더 `scene.hlsl`** — 불투명 물체용. 태양 Lambert + 환경광.

```text
색 = 재질색 × 무늬 × (환경광 + 태양색 × max(0, N·L))
```

**무늬는 월드 좌표 기준** (물체를 늘려도 무늬 크기가 그대로라 눈금처럼 읽힌다):

| 무늬 | 쓰는 곳 | 만드는 법 |
|---|---|---|
| 체크무늬 | 바닥, 받침대 윗면, 계단 | 면 방향에 맞는 두 축으로 1m 칸, 0.25m 잔 칸 |
| 눈금 띠 | 기둥, 받침대 옆면 | 높이 y를 0.5m마다 번갈아 칠하고 1m마다 진한 선 |
| 자갈 | 자갈 구역, 철로 둑 | 월드 XZ 보로노이(가장 가까운 점까지 거리)로 둥근 돌, 돌마다 밝기 다르게 |
| 단색 | 승강장, 전주 등 | 재질색 그대로 |

**수심 디버그 뷰** (Scene Debug = Depth):

```text
수심 = −y (수면 아래만)
수면 위    : 원래 색을 흐리게
수면 아래  : 수심 색 띠 (0~0.2m / ~1m / ~3m / ~6m / 그 이상)
1m마다 얇은 등고선, y = 0에 굵은 빨간 수면선
```

**참고:** 보로노이 — Inigo Quilez, "Voronoi edges" (https://iquilezles.org/articles/voronoilines/)

## 3. Inputs / Outputs

| 종류 | 이름 | 형식 | 범위 / 비고 |
|---|---|---|---|
| Mesh | 단위 박스 / 단위 원기둥 | 코드 생성 | 물체마다 월드 행렬로 크기·위치 |
| Mesh | Lab 수면 / Waterfront 수면 | 코드 생성 평면 그리드 | 유한 영역, 기존 물 셰이더로 그림 |
| CBuffer | world, worldInvTranspose, view, proj | float4x4 | 비균일 크기의 노멀 보정 |
| CBuffer | baseColor, pattern, patternScale | float4 / int / float | 물체별 재질 |
| CBuffer | debugMode | int | 0 render, 1 depth |
| ImGui | Scene | combo | Bench plane / Ocean / Water Lab / Waterfront |
| ImGui | Show Water | checkbox | 기본 켬 |
| ImGui | Lab Waves | checkbox | 기본 끔 (Lab·Waterfront에서 Gerstner 진폭 0) |
| ImGui | Scene Debug | combo | Render / Depth below water |
| 고정 샷 | `lab_top`, `lab_stairs`, `lab_low`, `lab_pillar`, `front_rails` | | 위에서 / 계단 사선 / 낮은 수면 / 기둥 근접 / 목표 장면 |
| 파일 | `assets/camera_presets.txt` | v2 | 슬롯에 장면 번호 추가, v1도 읽음 |

- Lab·Waterfront는 **오션 그리드용 프리셋**을 쓴다 (1 = 1m 스케일이 같음).
- 새 셰이더는 핫 리로드와 빌드 후 복사 목록에 등록한다.

**의존하는 다른 feature:** 없음 (기존 물·하늘·톤매핑 경로 그대로)

## 4. Acceptance Criteria / Test Plan

**에이전트 검증**

- [ ] Debug·Release 빌드, 셰이더 컴파일 경고 0
- [ ] 기존 샷 21장이 기준 캡처와 같음 (기존 장면 회귀 없음)
- [ ] 새 샷 5장 × 프리셋 3 캡처, 물 켬/끔 + 수심 디버그
- [ ] 배치 수치표: 받침대 윗면 −0.2 / −1 / −3 / −6m, 계단 단 높이, 기둥 높이가 코드 값과 일치
- [ ] 장면 전환·창 리사이즈·핫 리로드(scene.hlsl 저장)에서 오류 없음

**사용자 검증**

- [ ] 깊이가 다른 받침대 4개를 수심 디버그에서 색으로 구별할 수 있다
- [ ] 기둥과 계단에서 수면 위·아래 부분이 빨간 수면선으로 나뉜다
- [ ] −5cm / +5cm 받침대 중 하나만 수면 아래로 표시된다
- [ ] 그레이박스 `front_rails` 구도가 목표 장면 배치(얕은 철로·전주·승강장)로 읽힌다
- [ ] 다음 단계에서 왜 "물 없는 장면의 색·깊이"가 필요한지 설명할 수 있다

## 5. Notes

- 진행 기록은 같은 폴더 `NOTES.md`.
