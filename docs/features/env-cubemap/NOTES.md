# env-cubemap — 작업 메모

## 2026-05-01 · skybox depth = 1 트릭 (`clip.xyww` + LessEqual)

skybox는 항상 가장 먼 평면에 그려져 다른 모든 지오메트리가 그 위에 그려져야 함. 표준 트릭:

```hlsl
float4 clip = mul(viewPos, g_Projection);
output.position = clip.xyww;   // z = w → z/w = 1.0
```

`xyww`로 z를 w로 교체하면 perspective divide 후 NDC z = 1 (가장 먼 평면). 단 D3D11 기본 depth func는 `Less`라 z=1 픽셀이 reject됨. **DSS = `LessEqual` 필수.**

`D3DClass::SetDepthLessEqual()`을 skybox 그리기 직전에 호출, 직후 `SetDepthDefault()`로 복귀. 후속 모델은 z<1이라 LessEqual 하에서도 skybox 위에 그려짐.

## 2026-05-01 · view 행렬에서 translation 제거 — skybox 무한 거리감

skybox는 카메라 회전만 반영하고 이동은 무시해야 함. 그래야 카메라가 어디로 움직여도 항상 같은 거리에 보임.

```cpp
XMMATRIX viewNoTrans = view;
viewNoTrans.r[3] = XMVectorSet(0, 0, 0, 1);  // 4행(translation) 0
```

이 viewNoTrans를 skybox 셰이더에만 전달. plane의 ColorShader는 원본 view 그대로 사용.

**대안:** projection 행렬에서 처리하거나, vertex shader에서 cam pos 빼주는 방법도 있지만 view 행렬 row 3을 직접 0으로 두는 게 가장 직관적이고 GPU 비용 0.

## 2026-05-01 · cube vertex winding — outside-CW = inside-CCW

8 corners + 36 indices의 inline cube. 우리 RS = `FrontCounterClockwise=TRUE` + `CullBack`. 카메라가 cube 안쪽에 있을 때 내부 face가 보여야 함.

**핵심 사실:** outside 시점에서 CCW로 정렬된 winding은 inside 시점에서는 CW로 보임 (winding 반전). 그러면 우리 RS는 그것을 back face로 판정 → cull → 안 보임.

**해결:** vertex를 outside-CW로 정렬. inside에서 CCW(front)로 보이고 cull back에 살아남음.

```cpp
const unsigned long indices[36] = {
    0, 2, 1,  0, 3, 2,  // Front  (z=-1)
    5, 7, 4,  5, 6, 7,  // Back   (z=+1)
    // ...
};
```

대안으로 skybox 전용 RS(CullFront)를 만들어 standard CCW winding을 그대로 써도 되지만, RS 한 개 추가보다 winding 한 번 정해두는 게 단순.

## 2026-05-01 · Sampler clamp (wrap 아님)

큐브맵 sampler는 `D3D11_TEXTURE_ADDRESS_CLAMP` (UVW 모두). wrap이면 면 경계 보간 시 반대편 면 픽셀이 끌려와 black seam이 생김. clamp는 경계 픽셀을 stretch해 매끄럽게 이어짐.

D3DClass에 단일 `defaultSampler_`를 두고 skybox와 plane 양쪽이 공유. 후속 feature(water/foam)에서 같은 sampler 쓰거나 별도 sampler가 필요하면 `D3DClass::GetSampler()`를 더 분기.

## 2026-05-01 · CubemapTexture에서 SRV ViewDimension 검사

`CreateDDSTextureFromFile`은 일반 2D `.dds`도 성공. 큐브맵 플래그가 없는 DDS면 SRV가 `D3D11_SRV_DIMENSION_TEXTURE2D`로 만들어지고, 셰이더의 `TextureCube.Sample`은 그 SRV를 받으면 검정을 뱉음 (D3D 디버그 레이어 경고도 안 뜨는 silent failure).

**진단:** `Initialize` 안에서 `srv_->GetDesc()`로 ViewDimension을 검사, `TEXTURECUBE`가 아니면 `outError`로 사유 알리고 false. Graphics가 MessageBox로 사용자에게 보여줌.

```cpp
D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
srv_->GetDesc(&desc);
if (desc.ViewDimension != D3D11_SRV_DIMENSION_TEXTURECUBE) {
    *outError = L"DDS file loaded, but it is not a cubemap...";
    return false;
}
```

## 2026-05-01 · HDRI → cubemap DDS 워크플로 (외부 도구)

흔한 함정: HDRI-to-CubeMap 같은 웹 도구의 "정육면체 전개도(cross)" 옵션은 **6면을 한 장에 나열한 평면 PNG**일 뿐 D3D11 cubemap이 아님. 그걸 그대로 `.dds` 변환하면 단일 2D 텍스처가 됨.

**올바른 워크플로:**
1. HDRI-to-CubeMap에서 layout = **"Separate"** (6 PNG 분리)
2. `texassemble.exe cube -o skybox.dds px.png nx.png py.png ny.png pz.png nz.png`
   - **`texconv`가 아니라 `texassemble`** — Microsoft DirectXTex의 cubemap 묶기 전용 도구
   - DirectXTex Releases 페이지에 texconv와 함께 배포
   - 면 순서: `+X, -X, +Y, -Y, +Z, -Z` (D3D 큐브맵 표준)
3. 결과 `.dds`를 `assets/textures/skybox.dds`로

**대안:** cmftStudio가 HDRI `.hdr` 직접 → cubemap DDS 한 번에. HDRI-to-CubeMap 단계 건너뛰기 가능.

## 2026-05-01 · cubemap SRV 공유 (t0 두 셰이더)

skybox.hlsl과 simple.hlsl 모두 `register(t0)`에 큐브맵을 받음. CubemapTexture의 SRV 한 개를 두 곳에 바인딩 (메모리 1장 공유). Graphics::Render에서 두 번 `cubemap_->GetSRV()` 호출.

후속 water-base에서 큐브맵 외에 normal map 등이 추가되면 슬롯 분배 (e.g. cubemap=t0, normalA=t1, normalB=t2). 본 단계에선 t0 단일.

## 2026-05-01 · `g_ReflectionStrength=0`에서 분기 안 함

PS는 항상 cubemap을 sample한 뒤 `* g_ReflectionStrength` 곱함. strength가 0이면 결과가 0이 되어 효과 없음. `if (strength > 0)` 분기 추가하지 않음.

**이유:** GPU에서 분기는 warp divergence 비용. 단순 곱셈이 셰이더 코드도 깔끔하고 비용도 거의 없음. 큐브맵 SRV는 항상 바인딩되므로 strength=0이라도 sample 자체는 동작.
