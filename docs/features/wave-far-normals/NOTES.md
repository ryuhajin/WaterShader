# wave-far-normals NOTES

## 문제

- 사용자 테스트 중 ocean 그리드에서 "근경과 원경의 물결 방향이 X자로 엇갈려 보인다"는 지적이 나왔다.
- 원인: 정점 셰이더의 거리 LOD가 파도마다 `파장 × 8 ~ 14` 거리에서 Gerstner 파도를 끈다.
  - 이때 끄는 건 높이(`offset`)와 노멀(`normalSum`) **둘 다**다. 노멀도 `amplitude × fade`로 계산되기 때문이다.
  - 기본 크기(Wave 1 = 1.6)에서는 약 20 유닛 너머가 평평한 면 + 노멀맵만 남는다.
- 근경 무늬 방향은 Wind direction이 정하고, 원경 무늬 방향은 노멀맵 텍스처(예: "Diagonal ripples")가 정한다. 둘이 서로 연결돼 있지 않아서 X자가 된다.

## 접근

- 높이는 계속 끈다. 정점 간격이 파장보다 넓어지면 메시가 파도를 표현할 수 없기 때문이다.
- 노멀은 픽셀마다 계산할 수 있어서 정점 밀도와 상관이 없다. 그래서 같은 4개 파도의 기울기를 **픽셀 셰이더에서** 다시 계산한다.
- 근경을 바꾸지 않도록, 파도 i의 노멀 기여를 fade 가중치 w로 나눴다. 기여는 w에 선형이다.
  - VS: `w = vertexFade`
  - PS: `w = (1 − vertexFade) · pixelFade`
  - 둘을 합치면 근경에서는 VS 노멀과 같고, 원경에서는 PS 노멀이 된다.
- VS의 Q 항은 `q·kA = steepness·fade / 4`라서 PS에서도 같은 식(`steepness·w / 4`)을 쓴다.
- 위상은 변위 전 local 위치(`restPosLocal`, VS→PS로 새로 전달)로 계산한다. VS와 같은 기준이라 전환 구간에서도 마루 위치가 맞는다.
- 공용 함수 `WaveVertexFade`는 `shaders/Waves.hlsli`로 뺐다. VS는 이 함수를 호출하도록만 바꿨고 결과는 비트 단위로 같다(아래 OFF 회귀).

## 픽셀 페이드 기준 (실험)

`footprint = max(|ddx(xz)·D|, |ddy(xz)·D|) / λ`는 한 픽셀이 파도 진행 방향으로 덮는 파장 수다. 방향별로 계산하므로, 카메라 쪽으로 뻗은 마루는 더 멀리까지 남는다.

`captures/horizon_threshold.jpg`(Basic ocean_wide 수평선 2배 확대)에 세 기준을 비교했다. 행 순서: before, 2px, 3px, 4px.

| 기준 (파장당 픽셀, 시작 → 0) | 결과 |
|---|---|
| 4 → 2 (나이퀴스트) | 수평선까지 줄무늬는 이어지지만, 고대비 간섭무늬(moire)가 생김 |
| 6 → 3 | 조금 줄었지만 여전히 촘촘한 고대비 줄 |
| **8 → 4 (채택)** | 수평선 바로 앞의 고주파 줄이 사라지고 노멀맵으로 넘어감. 바람 방향 마루는 원경까지 유지 |

- 나이퀴스트(2px)만 지키면 될 것 같았지만 실제로는 부족했다.
- 태양 글린트(power 300~800)와 Fresnel 반사는 노멀에 매우 민감하다. 그래서 해상 가능한 범위의 노멀 변화도 화면에서는 고대비 줄로 증폭된다.

## 검증 (AI agent)

- Debug/Release 빌드 경고 0. `fxc /Ges`(strict)로 VS/PS를 최적화·디버그 빌드 모두 컴파일했고 경고 0. `WaveMacroTest` 통과.
- **OFF 회귀:** `--capture off --far-waves off` 15장 vs before → **15장 모두 최대 차이 0**. before와 같은 이미지라 커밋하지 않았다. VS 리팩터와 OFF 경로가 이전과 동일하다.
- **ON 결과:** `--capture after` vs before (최대 / 평균 채널 차이)

| 샷 | Basic | Sunset | Tropical |
|---|---|---|---|
| oblique / top / sunward (벤치 평면 2×2) | 0 / 0 | ≤ 3 / 0.00 | ≤ 1 / 0.00 |
| ocean_sunward | 199 / 1.74 | 145 / 1.70 | 125 / 0.79 |
| ocean_wide | 211 / 6.09 | 175 / 5.12 | 122 / 2.36 |

  - 변화는 원경이 있는 ocean 샷에만 있다. 작은 평면은 거의 전부 VS 구간이라 사실상 그대로다.
- **Debug 6 (Wave LOD):** `--capture debug6 --debug 6` → `compare/*.jpg`의 세 번째 열
  - 빨강 = 메시 파도, 초록 = 픽셀 파도, 검정 = 둘 다 끝난 수평선
  - Basic 프리셋은 파장이 길어서(Wave 1 = 4.26) 빨강 구간이 넓다. 기본 파도를 쓰는 Sunset/Tropical은 초록 구간이 넓다.
- **GPU 시간:** Release, `--no-vsync --shot ocean_wide`, 1280×760에서 ON/OFF 모두 **0.19 ms**. 측정 단위 안에서 차이가 없다(`captures/gpu_on_off.jpg`).
- 비교 시트: `compare/*.jpg` (before / after / debug6)
- `assets/shader_presets.txt`는 테스트 중 변경되지 않았다(해시 비교).

## 사용자 확인 필요

- 근경과 원경의 줄무늬 방향이 이어지는지(X자 해소)
- Wind direction을 돌리면 원경도 같이 도는지
- **움직일 때 수평선 근처가 지글거리지 않는지.** 정지 캡처로는 확인할 수 없다.
- Water 창 `Far waves` 체크박스 ON/OFF, View 창 Debug Mode `Wave LOD`
