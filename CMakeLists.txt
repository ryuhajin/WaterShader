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
    src/System.cpp
    src/System.h
    src/SystemConfig.h
    src/Texture.h
    src/Timer.cpp
    src/Timer.h
)

find_package(directxtk CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

target_compile_definitions(WaterShader PRIVATE
    UNICODE
    _UNICODE
)

target_link_libraries(WaterShader PRIVATE
    d3d11
    dxgi
    d3dcompiler
    Microsoft::DirectXTK
    imgui::imgui
)

set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_OUTPUT_DIR "$<TARGET_FILE_DIR:WaterShader>/shaders")

add_custom_command(TARGET WaterShader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SHADER_SOURCE_DIR}/simple.hlsl"
        "${SHADER_OUTPUT_DIR}/simple.hlsl"
)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT WaterShader)
set_property(TARGET WaterShader PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:WaterShader>")
