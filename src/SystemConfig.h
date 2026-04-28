#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define DIRECTINPUT_VERSION 0x0800

#define FULL_SCREEN false
#define VSYNC_ENABLED true
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define SCREEN_DEPTH 1000.0f
#define SCREEN_NEAR 0.1f

#define WINDOW_CLASS_NAME L"WaterShaderWindowClass"
#define WINDOW_TITLE L"Water Shader - DirectX11 Setup"
