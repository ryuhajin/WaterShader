# Normal G-Buffer — NOTES

> 6C단계 진행 기록. 상태: **에이전트 검증 완료, 사용자 승인 대기**
> 범위 (사용자 결정 2026-11-07): 깊이 복원 노멀 vs 저장 노멀의 성능 비교와 참고용 캡처만. 거품 모양은 6B 그대로.

## 1. 개념 요약

```text
불투명 패스 (테스트 장면 물체):  SV_Target0 = HDR 색   +   SV_Target1 = 월드 노멀 × 0.5 + 0.5 (a = 1)   ← MRT
물 패스, 노멀 출처:
  Depth reconstructed  깊이 4번 읽기 + 위치 복원 4번 + 외적        (6B, 추가 버퍼 없음)
  Stored scene normal  노멀 버퍼 1번 읽기                          (G-Buffer의 노멀 한 장)
```

- 디퍼드 렌더러는 불투명 패스에서 노멀·재질 같은 표면 정보를 버퍼(G-Buffer)에 남기고, 뒤 패스(조명, SSR, SSAO, 물)가 그것을 읽는다. 여기서는 노멀 한 장만 만들었다.
- 노멀 버퍼는 Stored를 고르거나 디버그 23·24를 볼 때만 묶고 쓴다. Depth reconstructed일 때는 장면 셰이더도 색만 쓰는 원래 진입점을 써서, 6B와 비용·화면이 같다.

## 2. 만든 것

| 파일 | 내용 |
|---|---|
| `src/D3DClass.*` | 장면 노멀 버퍼 `R10G10B10A2_UNORM` (HDR 타깃 크기, 리사이즈 때 함께), `ClearSceneNormal`, `BindSceneTargetsWithNormal`(색 + 노멀 + 깊이), `BindHdrTarget` |
| `shaders/scene.hlsl` | `PSMainWithNormal`: `PSMain`의 색 + 월드 노멀 (두 출력). `PSMain`은 그대로 |
| `src/SceneShader.*` | 두 진입점을 모두 컴파일, `Begin(..., writeNormal)`로 고름 |
| `shaders/PixelShader.hlsl` | t6 노멀 버퍼, `SceneNormalStored`, 노멀 출처 `g_SceneParams.w`. 디버그 23(저장 노멀 색), 24(두 노멀의 각도 차이, 0~30°) |
| `src/Graphics.*` | Water 창 New: Contact Foam의 **Normal source** 콤보, CLI `--normal-source depth\|stored`, `UsesStoredNormal()`(노멀 버퍼를 쓰는 프레임), Debug Mode 23·24 라벨 |

## 3. 에이전트 검증 (2026-11-07)

| 항목 | 방법 | 결과 |
|---|---|---|
| 빌드 | Debug·Release | 경고·오류 0 |
| 기존 장면 회귀 | 기존 샷 21장 vs 0단계 기준 | **21장 바이트 동일** |
| Depth reconstructed = 6B | main(6B 머지 후)으로 찍은 6샷 × 3프리셋 vs 6C 기본값 | **18장 바이트 동일** (한 번은 1장 4픽셀 1레벨, 다시 찍자 동일: 실행마다의 흔들림) |
| 저장 노멀 = 정답지 | `-Mode normal`, 23 vs Scene Debug 4 | 6샷 모두 **99%까지 0.00°** (같은 법선을 같은 형식으로 저장) |
| 깊이 복원 vs 저장 | `-Mode normal`, 22 vs 23 + 오차 지도 | 6B의 정답지 비교와 같은 수치 (중앙값 0~0.45°, 99% 5~29°) |
| 거품: 평평한 면 | `-Mode foam -D` | 깊이 노멀 0~96픽셀 → **저장 노멀 0픽셀** (6샷 모두) |
| 디버그 레이어 | Debug: Depth / Stored / 24 × 2샷 | 경고·오류 0건 |

**과정에서 고친 것**

| 증상 | 원인 | 해결 |
|---|---|---|
| Depth reconstructed에서 디버그 레이어 경고 1건: "Pixel Shader expects a Render Target View bound to slot 1, but none is bound" | 장면 셰이더가 항상 두 출력을 냈고, 노멀 버퍼는 Stored일 때만 묶음 | 장면 셰이더에 진입점 두 개(`PSMain` 색만, `PSMainWithNormal` 색 + 노멀). 노멀 버퍼를 묶을 때만 두 번째를 씀. 경고 0건, 색만 쓰는 경로는 6B와 같은 셰이더 |

## 4. 화질 비교 — 깊이 복원이 틀리는 곳 (참고용 캡처)

비교 묶음 그림 `compare_sheets/normals_<샷>.png` (깊이 복원 | 저장 | 차이), `compare_sheets/foam_<샷>.png` (거품: 깊이 노멀 | 저장 노멀 | 저장 노멀 최종 화면).

| 곳 | 깊이 복원 | 저장 노멀 |
|---|---|---|
| 넓은 면 (바닥, 벽 안쪽) | 0~0.5° (같음) | 정답 |
| 모서리 | 1픽셀 선 전부 틀림 (양옆이 다른 면) | 정답 |
| 가는 물체 (레일, 먼 침목·전주) | 통째로 틀림 | 정답 |
| 원기둥 | 옅은 세로 줄무늬 (면 조각마다 평평) | 매끈한 보간 노멀 |
| 거품 기울기 필터 | 평평한 면 모서리에 거품 0~96픽셀 | 0픽셀 |

## 5. 성능 비교

RTX 4080 SUPER, Release, `--bench` 600프레임 × 2회, 샷별 GPU 중앙값의 회차 평균. 불투명 구간 = 지우기 + 하늘 + 장면 물체 (노멀 버퍼 지우기·쓰기 포함), 물 구간 = 장면 색 복사 + 물.

**실제 사용 (거품의 기울기 필터만 노멀을 읽음, 물 픽셀의 0.1~2.3%)** — 4K (3840 × 2160)

| 샷 | 불투명: Depth → Stored | 물: Depth → Stored | 프레임: Depth → Stored |
|---|---|---|---|
| lab_top | 0.114 → 0.135 (+0.021) | 0.210 → 0.206 (−0.004) | 0.367 → 0.382 (+0.015) |
| lab_stairs | 0.183 → 0.238 (+0.054) | 0.259 → 0.257 (−0.003) | 0.487 → 0.538 (+0.051) |
| lab_low | 0.162 → 0.214 (+0.052) | 0.238 → 0.236 (−0.002) | 0.441 → 0.493 (+0.052) |
| lab_pillar | 0.302 → 0.411 (+0.109) | 0.298 → 0.293 (−0.005) | 0.644 → 0.749 (+0.105) |
| lab_pillar_near | 0.178 → 0.235 (+0.057) | 0.310 → 0.307 (−0.003) | 0.532 → 0.589 (+0.057) |
| front_rails | 0.318 → 0.370 (+0.052) | 0.283 → 0.278 (−0.005) | 0.648 → 0.697 (+0.049) |

(단위 ms. 회차 간 흔들림은 대부분 ±0.002 이하, Stored 불투명 구간은 ±0.004)

**전 화면 (모든 물 픽셀이 노멀을 읽음: 디버그 22 vs 23)** — 4K, 물 구간

| 샷 | Depth (22) | Stored (23) |
|---|---|---|
| lab_top | 0.160 | 0.135 (−16%) |
| lab_stairs | 0.140 | 0.145 |
| lab_low | 0.121 | 0.122 |
| lab_pillar | 0.147 | 0.148 |
| lab_pillar_near | 0.157 | 0.155 |
| front_rails | 0.128 | 0.123 |

720p에서도 같은 경향: 불투명 구간 +0.003~0.016 ms, 물 구간은 차이 없음(타이머 단위 수준).

**읽는 법**

- **노멀 버퍼의 비용은 불투명 패스에 생긴다.** 4K에서 0.02~0.11 ms: 버퍼 지우기(4K × 4바이트 ≈ 33MB)와 물체가 덮는 픽셀마다 노멀 쓰기. 물체가 화면을 많이 덮는 lab_pillar가 가장 비싸다.
- **깊이 복원의 계산 비용은 이 GPU에서 거의 안 보인다.** 거품은 물 픽셀의 몇 %에서만 노멀을 구해서 절약이 2~5 µs에 그친다. 모든 물 픽셀이 노멀을 구해도(22 vs 23) 대부분 샷에서 같고, 물 너머 표면이 화면을 가장 많이 덮는 lab_top에서만 저장 노멀이 16% 빠르다. 이웃 깊이 4번 읽기는 캐시에 걸리고, 이 패스는 계산보다 메모리 읽기가 지배적인 것으로 보인다.
- **결론:** 노멀이 필요한 소비자가 거품 하나뿐이면 깊이 복원이 프레임 전체로 더 싸다 (0.015~0.105 ms 절약). 대신 모서리·가는 물체·둥근 면에서 틀린다. 노멀 버퍼는 조명·SSR·SSAO처럼 노멀을 읽는 패스가 여럿일 때 쓰기 비용이 나뉘어 이득이 커진다. 언리얼이 G-Buffer를 늘 만드는 이유다 (15단계 RenderDoc에서 GBufferA 확인).

```powershell
.\tools\summarize_bench.ps1 -Root C:\Users\da171\ws-capture\normal-gbuffer\captures -Cases depth,stored,fulldepth,fullstored -Prefix bench4k_ -Section opaque
.\tools\summarize_bench.ps1 -Root C:\Users\da171\ws-capture\normal-gbuffer\captures -Cases depth,stored,fulldepth,fullstored -Prefix bench4k_ -Section water
.\tools\compare_depth.ps1 -Mode normal -A C:\Users\da171\ws-capture\normal-gbuffer\captures\mask -B C:\Users\da171\ws-capture\normal-gbuffer\captures\normal_depth -C C:\Users\da171\ws-capture\normal-gbuffer\captures\normal_stored -MapDir C:\Users\da171\ws-capture\normal-gbuffer\captures\normal_error_map
```

**캡처 위치 (로컬):** `C:\Users\da171\ws-capture\normal-gbuffer\captures\`

| 폴더 | 내용 |
|---|---|
| `compare_sheets` | 샷별 비교 묶음 그림 (노멀 3장, 거품 3장) — 나중에 비교할 때 참고용 |
| `render_depth`, `render_stored`, `render_stored_all` | 최종 화면 (Basic 6샷 / 3프리셋 × 6샷) |
| `normal_depth`, `normal_stored`, `normal_difference`, `normal_error_map` | 디버그 22, 23, 24, 도구 오차 지도 |
| `foam_depth`, `foam_stored`, `key_depth`, `key_normal`, `mask` | 거품 양, 정답지 |
| `dbg_*` | Debug 빌드 + `d3d_messages.txt` |
| `bench_*`, `bench4k_*` | 프레임 측정 (`timings.csv`) |

## 6. 사용자 검증 안내

**실행:** `.\build\vs2022\Debug\WaterShader.exe` → `1` 키 → Scene = **Water Lab** → `3` 키 Water 창 → New: Contact Foam의 **Normal source**

| 순서 | 조작 | 정상 |
|---|---|---|
| 1 | Debug Mode **Normal difference** (24), 카메라 lab_stairs, front_rails(Waterfront) | 모서리·레일·먼 침목만 흰 선, 넓은 면은 검정 |
| 2 | Debug Mode **Normal from depth** ↔ **Stored scene normal** | 기둥의 줄무늬, 레일 색이 저장 노멀에서 바로잡힘 |
| 3 | Debug Mode Render로, Normal source를 Depth ↔ Stored | 거품 모양은 거의 같고, 평평한 면 모서리의 작은 거품 점만 사라짐 |
| 4 | Stats의 GPU ms (VSync 끄고) | 저장 노멀에서 약간 늘어남 (위 표의 프레임 차이) |

**확인 질문**

- [ ] 깊이 복원 노멀이 틀리는 곳(모서리·가는 물체·둥근 면)을 화면에서 짚을 수 있다
- [ ] 노멀 버퍼의 비용이 어느 패스에 생기는지, 왜 이 장면에서는 깊이 복원이 더 싼지 숫자로 설명할 수 있다
