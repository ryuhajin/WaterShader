# Foam Masks — 6B+ 접촉 거품 마스크 업그레이드

> Branch: `feature/foam-masks` · Status: in-progress · Updated: 2026-11-09
> 6B(접촉 거품) 보강. 앞 단계: [../refraction/NOTES.md](../refraction/NOTES.md), 거품 원래 설계: [../contact-foam/SPEC.md](../contact-foam/SPEC.md)

## 1. Goal

- **한 문장 요약:** 접촉 거품의 가장자리 모양을 정하는 마스크를 **셀 모양 거품망 + 작은 기포 + 침식 노이즈**로 바꿔 자연스럽고 청량한 거품을 만들고, 여러 마스크를 노멀맵처럼 UI에서 고를 수 있게 한다.
- 마스크는 **컴퓨트 셰이더로 계산해 텍스처 폴더에 이미지로 저장**한다(굽기). 앱은 실행 때 노멀맵처럼 저장된 파일을 읽어 GPU로 보낸다. 모든 새 마스크는 **이음매 없이 반복**된다.
- **스코프 가드:** 거품 위치(수심 폭·기울기 필터·노멀 출처), 수면 위 바깥 고리(언리얼 거리장 단계), 파도 마루 거품, 성능 측정(요청 시만)은 바꾸지 않는다.

## 2. 실무 조사 요약

| 방식 | 내용 | 출처 |
|---|---|---|
| 깊이 차 × 노이즈 × smoothstep | 접촉선은 깊이 차, 가장자리는 보로노이·노이즈로 깨뜨리고 smoothstep으로 모양 (6B의 뼈대) | [80.lv](https://80.lv/articles/an-amazing-stylized-water-shader-made-in-unity), [Unity](https://unity.com/blog/engine-platform/nature-shaders-with-shader-graph-in-2022-lts), [Godot Shaders](https://godotshaders.com/shader/toon-style-3d-water-shader-no-textures-needed/) |
| 보로노이(셀) 거품 | 셀 경계가 거품망. 크기·오프셋이 다른 두 장을 겹쳐 흘림 | 위 두 글 |
| 흐름 거품 | 같은 텍스처 두 장을 다른 방향으로 흘려 섞음 | [gamedev.net](https://gamedev.net/shaderlab/53-foam-with-flow-mapping/) |
| 기포(하프톤) | 점 마스크를 섞어 기포가 무리 지어 생기고 터지는 모습 | [Parberry LARC-2011-05](https://ianparberry.com/techreports/LARC-2011-05.pdf) |
| 침식 | 저주파 노이즈로 접촉 마스크를 깎아 섬처럼 끊기게 | [UE Single Layer Water 연구](https://d34kmbgkaszffl.cloudfront.net/blog/a-water-shader-study-in-unreal-engine-using-single-layer-water) |
| 채널 패킹 | 여러 마스크를 RGBA 채널에 나눠 한 장으로 | [Unity HDRP](https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.3/manual/water-debug-mode.html) |

**추천 (기본값):** 패킹 마스크 — **R = 셀 거품망**(보로노이 F2 − F1), **G = 기포**(크기가 다른 원), **B = 저주파 fbm**(침식).

## 3. 마스크 목록

| # | 이름 | R | G | B |
|---|---|---|---|---|
| 0 | Clouds (`wave_foam`, 기존) | 구름 노이즈 | — | — |
| 1 | Cells (`foam_cells`) | 셀망 | 0 | 0.5 |
| 2 | Bubbles (`foam_bubbles`) | 큰 기포 | 작은 기포 | 0.5 |
| 3 | **Cells + bubbles + breakup** (`foam_packed`, 기본) | 셀망 | 기포 | fbm |
| 4 | Streaks (`foam_streaks`) | 한 축으로 늘인 셀망 | 기포 | fbm |

512 × 512 RGBA8. PNG 원본 + 밉 있는 DDS(texconv BC7).

## 4. 방법

**굽기 (오프라인, `--bake-foam-masks <폴더>` → `tools/bake_foam_masks.ps1`)**

```text
foam_bake.hlsl (cs_5_0, 8 × 8 스레드) → RWTexture2D (UAV) → PNG 저장 → texconv → DDS
이음매 없음 = 모든 노이즈를 주기 격자로: 셀 좌표를 셀 수로 나머지 연산(wrap), fbm은 옥타브마다 정수 주기
```

**합성 (물 픽셀 셰이더, 6B 거품 블록)**

```text
마스크 = 크기·방향이 다른 두 장을 흘려 섞음 (새 마스크는 wrap 반복, Clouds만 거울 반복)
문턱   = lerp(부드러움, 1 − 부드러움, R) + (B − 0.5) × 침식
몸통   = smoothstep(문턱 ± 부드러움, 가까움 × 기울기 마스크)
기포   = 띠 바깥쪽(가까움 낮은 곳)에서 G가 높은 점만 더함 × 기포 세기
```

| 종류 | 이름 | 비고 |
|---|---|---|
| Preset | `foamMap`, `foamBubbles`, `foamErosion` | 키가 없으면 3, 기본 세기 |
| ImGui (Water 창 Contact Foam) | Foam texture 콤보, Bubbles, Erosion | 노멀맵 텍스처 콤보와 같은 모양 |
| CLI | `--foam-map <index>`, `--bake-foam-masks <폴더>` | |

## 5. Acceptance Criteria

**에이전트 검증**

- [x] 빌드 경고 0, 디버그 레이어 경고 0 (굽기 모드 + 일반 실행), 기존 바다 21장 바이트 동일
- [x] 새 마스크 4장 모두 이음매 없음: 끝 열↔첫 열, 끝 행↔첫 행 차이가 안쪽 이웃 차이와 같은 수준 + 2 × 2 확인 그림
- [x] 같은 시드로 두 번 구우면 PNG 바이트 동일
- [x] Clouds(0) → 8단계 거품과 같음 (1레벨 수준 차이는 기록). 거품 세기 0은 Clouds 경로와 같은 코드라 따로 비교하지 않음
- [x] 거품 위치: 가장 깊은 거품 ≤ 폭 (기포 포함), 하늘 0
- [x] 마스크 5종 비교 그림 (lab_pillar_near, lab_stairs, front_rails)

**사용자 검증**

- [ ] 기본 마스크(Cells + bubbles + breakup)가 Clouds보다 자연스럽고 청량하게 보인다
- [ ] Water 창에서 마스크를 바꿔 가며 고를 수 있다
- [ ] 2 × 2 확인 그림에서 이음매가 안 보인다
