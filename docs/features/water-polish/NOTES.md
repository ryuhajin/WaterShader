# water-polish — 퀄리티 업 기록 (Before / After)

> 2026-09-29 · feature/water-polish

모든 캡처는 같은 조건: 1280×720, 시간 `t = 12.0s` 고정, UI 숨김, 카메라 3종 고정(`oblique` / `top` / `sunward`).
재현: `build/vs2022/Debug/WaterShader.exe --capture <label> [--debug <mode>]` → `captures/<label>/<preset>_<shot>.jpg`
비교 시트: `powershell -File make_compare.ps1 before step1_bugfix ...` → `compare/<shot>.jpg` (행 = 프리셋, 열 = 단계)

---

## Step 0 — Before 기록 도구

- `Graphics::StartCaptureSet / BeginCaptureFrame / EndCaptureFrame` — 프리셋 × 고정 샷 큐를 한 프레임에 하나씩 렌더하고, ImGui를 끈 채 backbuffer를 `DirectX::SaveWICTextureToFile`(JPEG q95)로 저장.
- 캡처 동안 시간은 `kCaptureTime`으로 고정 → 파도 위상이 같아서 before/after 비교가 공정함.
- `sunward` 샷은 스카이박스 태양 방향(yaw ≈ 34.5°)을 낮은 각도로 바라봄 → 반사광 확인용.
- 결과: `captures/before/`

## Step 1 — 버그 2개 수정 (`captures/step1_bugfix/`)

before 캡처를 분석하다 반사광이 없는 원인을 추적하니, 튜닝 문제가 아니라 **버그**였다.

### 1-a. 노멀맵이 감마 디코딩되어 있었음 (가장 큰 원인)

- **증상:** 반사광·확산광이 태양을 수면 *아래*에 둘 때만 보임. `--debug 2`(world normal)가 순수 초록(0,1,0)이 아니라 `(80,240,60)` ≈ **N = (−0.37, 0.88, −0.53)** — 수면 전체가 한쪽으로 기울어 있음.
- **원인:** `water_normal.dds`의 평균 RGB가 **(55, 57, 246)**. 평평한 탄젠트 노멀맵은 (128,128,255)여야 한다. `128`을 sRGB→linear로 변환하면 정확히 `55`. 원본 `water_normal.jpg`는 평균 (127.7, 127.7, 251)로 정상 → **texconv 변환 시 JPG의 sRGB 메타데이터(Photoshop ICC) 때문에 감마 디코딩**이 적용됨. 구 DDS와 `srgb_to_linear(jpg)`의 오차 0.2/255로 확인.
- **수정:** `texconv -f R8G8B8A8_UNORM --ignore-srgb -m 0 water_normal.jpg` 로 재생성.
- **교훈:** 노멀맵·마스크 등 *데이터 텍스처*는 색 공간 변환을 절대 거치면 안 된다. 텍스처 평균값 한 번 찍어보는 것이 가장 빠른 검증.

### 1-b. 광원 방향 규약이 뒤집혀 있었음

- 기존: `lightDir = (sin yaw·cos p, −sin p, cos yaw·cos p)`, 셰이더는 `−g_LightDirection`을 "광원을 향하는 방향"으로 사용 → 프리셋의 음수 pitch(−12°, −39.75°)는 태양이 **수면 아래**.
  1-a의 기울어진 노멀과 우연히 맞물려 "그럭저럭 밝게" 보였기 때문에 발견이 늦었다.
- 수정: `Sun Yaw / Sun Elevation`(고도 > 0 = 수평선 위)으로 재정의, `g_LightDirection = −toSun`.
- 스카이박스 태양 위치 측정: cubemap +Z 면 밝은 픽셀 추적 + `sunward` 캡처로 보정 → **yaw ≈ 34.5°, elevation ≈ 4°**. ImGui `Match Skybox Sun` 버튼 추가.
- 프리셋 파일 v2: 두 번째 필드 의미가 바뀌어 버전 업. 고정 필드 뒤에 `key value` 확장 필드를 붙일 수 있게 해서 이후 파라미터 추가 시 포맷 변경 불필요.
- 기존 튜닝 값(색·Fresnel·스크롤 등)은 그대로 두고 태양만 재배치: Basic 20°, Sunset 5°, Tropical 32° (yaw는 모두 스카이박스 태양에 맞춤).

### 1-c. 디버그 뷰 추가

- `--debug 5` / Debug Mode 5 = **Lighting terms** — R = Lambert, G = Blinn-Phong, B = Fresnel 반사량. 이번 버그를 이 뷰로 확정했다.

## Step 2 — HDR 태양 글린트 + 하이라이트 롤오프 (`captures/step2_sun_glint/`)

```
R        = reflect(−V, N)
glint    = pow(saturate(dot(R, L)), glintPower) · glintIntensity · F
color   += glint · lightColor
out      = HighlightRolloff(color)   // 0.8 이하는 그대로, 초과분은 1 − exp 곡선으로 압축
```

- Blinn-Phong(넓은 광택)과 별개로, **태양 원반의 거울 반사**를 따로 둔다. 잔물결 면 하나하나가 태양 방향과 맞을 때 번쩍이고, 이것들이 모여 태양 쪽으로 뻗는 "빛의 길(glitter path)"이 된다.
- `glintIntensity`는 1보다 훨씬 크게(기본 12) 둔 HDR 값. 그대로 출력하면 1.0에서 잘려 평평한 흰 덩어리가 되므로 셰이더 끝에서 롤오프.
- 롤오프를 0.8 이하에서는 항등으로 둔 이유: 스카이박스는 톤매핑 없이 LDR로 그려지므로, 물에만 ACES 같은 전체 톤커브를 걸면 하늘 반사가 실제 하늘보다 어두워져 이질감이 생긴다.
- 파라미터: `g_SpecularParams.zw` (Sun Glint Power / Intensity), 프리셋 확장 필드 `sunGlintPower`, `sunGlintIntensity`.

### 남은 문제 (→ Step 3)

- 파도 뒷면에서 반사 벡터가 수평선 아래를 향해 **초원(초록)이 비침**.
- Fresnel `F0 = 0.5` 고정 → 정면에서도 하늘이 반쯤 비쳐 물 색이 뿌옇다. Sunset은 Grazing 색이 순수 빨강(1,0,0)이라 "빨간 비닐" 느낌.
- 노멀맵 두 레이어가 같은 스케일 → 반복 패턴. 비스듬한 각도에서 블러(anisotropic 미사용).
