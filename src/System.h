#pragma once

#include "SystemConfig.h"

#include <windows.h>

#include <memory>

class Graphics;
class Input;
class Timer;

class System
{
public:
    System();
    ~System();
    System(const System&) = delete;
    System& operator=(const System&) = delete;

    bool Initialize(HINSTANCE instance);
    void Run();
    void Shutdown();

    LRESULT MessageHandler(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    bool Frame();
    bool InitializeWindow(HINSTANCE instance);
    void ShowWindowAfterInitialize();
    void ShutdownWindow();

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    bool shouldQuit_ = false;

    std::unique_ptr<Input> input_;
    std::unique_ptr<Timer> timer_;
    std::unique_ptr<Graphics> graphics_;
};
