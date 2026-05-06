cmake_minimum_required(VERSION 3.24)

project(WaterShaderPortfolio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(WaterShader WIN32
    src/Camera.cpp
    src/Camera.h
    src/ColorShader.cpp
    src/ColorShader.h
    src/CubemapTexture.cpp
    src/CubemapTexture.h
    src/D3DClass.cpp
    src/D3DClass.h
    src/Graphics.cpp
    src/Graphics.h
    src/Input.cpp
    src/Input.h
    src/Light.cpp
    src/Light.h
    src/main.cpp
    src/Model.cpp
    src/Model.h
    src/Skybox.cpp
    src/Skybox.h
    src/SkyboxShader.cpp
    src/SkyboxShader.h
    src/System.cpp
    src/System.h
    src/SystemConfig.h
    src/Texture.cpp
    src/Texture.h
    src/Timer.cpp
    src/Timer.h
)

find_package(directxtk CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)

target_compile_definitions(WaterShader PRIVATE
    UNICODE
    _UNICODE
    $<$<CONFIG:Debug>:WATERSHADER_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders">
    $<$<CONFIG:Debug>:WATERSHADER_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets">
)

target_link_libraries(WaterShader PRIVATE
    d3d11
    dxgi
    d3dcompiler
    Microsoft::DirectXTK
    imgui::imgui
    tinyobjloader::tinyobjloader
)

set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_OUTPUT_DIR "$<TARGET_FILE_DIR:WaterShader>/shaders")

# NOTE: .hlsl/.hlsli files are NOT compile targets; they are runtime data
# loaded by D3DCompileFromFile. This rule only mirrors them next to the exe.
add_custom_command(TARGET WaterShader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SHADER_SOURCE_DIR}/vertexShader.hlsl"
        "${SHADER_OUTPUT_DIR}/vertexShader.hlsl"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SHADER_SOURCE_DIR}/PixelShader.hlsl"
        "${SHADER_OUTPUT_DIR}/PixelShader.hlsl"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SHADER_SOURCE_DIR}/Common.hlsli"
        "${SHADER_OUTPUT_DIR}/Common.hlsli"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SHADER_SOURCE_DIR}/Lighting.hlsli"
        "${SHADER_OUTPUT_DIR}/Lighting.hlsli"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SHADER_SOURCE_DIR}/Cubemap.hlsli"
        "${SHADER_OUTPUT_DIR}/Cubemap.hlsli"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SHADER_SOURCE_DIR}/skybox.hlsl"
        "${SHADER_OUTPUT_DIR}/skybox.hlsl"
)

set(ASSETS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(ASSETS_OUTPUT_DIR "$<TARGET_FILE_DIR:WaterShader>/assets")

add_custom_command(TARGET WaterShader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ASSETS_SOURCE_DIR}" "${ASSETS_OUTPUT_DIR}"
)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT WaterShader)
set_property(TARGET WaterShader PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:WaterShader>")
