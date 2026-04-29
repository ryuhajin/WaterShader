# shader-hot-reload — 작업 메모

## 2026-04-29 · 빌드 사본 vs 소스 hlsl 경로 불일치

**증상:** 구현 직후 첫 검증에서 소스 `shaders/simple.hlsl`을 외부 에디터로 편집·저장해도 화면 변화 없음. timestamp 갱신도 안 됨.

**원인:** CMakeLists.txt의 `add_custom_command(POST_BUILD ... copy_if_different ...)`이 빌드 후 소스 hlsl을 exe 옆 디렉토리(`build/vs2022/Debug/shaders/simple.hlsl`)로 복사. `ColorShader::GetShaderPath`는 `GetModuleFileNameW`로 module-relative 경로를 계산하므로 빌드 사본을 watch. 사용자가 소스를 편집해도 빌드 사본 mtime은 그대로 → `CheckHotReload`가 트리거되지 않음.

**수정:** Debug 빌드에서만 CMake가 매크로로 소스 디렉토리 절대 경로를 박음.

```cmake
$<$<CONFIG:Debug>:WATERSHADER_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders">
```

`GetShaderPath`에 `#ifdef WATERSHADER_SHADER_DIR` 분기 추가. Debug는 소스 dir, Release는 기존 module-relative.

**Release 영향 없음:** POST_BUILD copy 로직은 그대로 유지. Release exe는 module 옆 사본을 사용하므로 배포 시 다른 머신에서도 정상 동작.

**교훈:** 에셋 hot-reload 구현 시 "어느 경로를 watch하는가"가 결정적. 빌드 산출물 옆 사본은 배포용, 개발 watch는 항상 원본 소스를 가리켜야 함. 향후 텍스처/큐브맵 hot-reload 시에도 같은 원칙 적용.

## 2026-04-29 · CompileShader 에러 텍스트를 outError로 전달

**배경:** dx11-setup의 `CompileShader`는 D3DCompiler errors blob을 `OutputDebugStringA`로만 출력 → ImGui에 직접 표시 불가. hot-reload 구현 시 컴파일 실패 메시지를 사용자에게 즉시 보여줘야 함.

**수정:** `CompileShader` 시그니처에 `std::string* outError` 추가하여 errors blob 텍스트를 호출자가 흡수. `OutputDebugStringA`도 함께 호출하여 Visual Studio 출력창에서도 동시 확인 가능. `ColorShader::Reload`는 `outError`를 받아 `lastError_` 멤버에 저장, `Graphics::DrawImGuiPanel`이 빨간색 `Compile error:` + `TextWrapped`로 표시.

**교훈:** 에러 핸들링 채널은 처음부터 "텍스트 콜러로 전달 + 디버그 출력"을 함께 두는 것이 실용적. 디버그 출력만 두면 GUI 표시가 필요해질 때마다 시그니처를 바꿔야 함.
