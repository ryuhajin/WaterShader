#include "System.h"

#include "Graphics.h"
#include "Input.h"
#include "Timer.h"

#include <imgui.h>
#include <imgui_impl_win32.h>

#include <stdexcept>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
System* gSystem = nullptr;

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (gSystem)
    {
        return gSystem->MessageHandler(hwnd, message, wParam, lParam);
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}
} // namespace

System::System() = default;
System::~System() = default;

bool System::Initialize(HINSTANCE instance)
{
    try
    {
        if (!InitializeWindow(instance))
        {
            return false;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        imguiContextCreated_ = true;
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();

        if (!ImGui_ImplWin32_Init(hwnd_))
        {
            return false;
        }
        imguiWin32Initialized_ = true;

        input_ = std::make_unique<Input>();
        input_->Initialize();

        timer_ = std::make_unique<Timer>();
        timer_->Initialize();

        graphics_ = std::make_unique<Graphics>();
        if (!graphics_->Initialize(hwnd_, SCREEN_WIDTH, SCREEN_HEIGHT))
        {
            return false;
        }

        ShowWindowAfterInitialize();
    }
    catch (const std::exception& error)
    {
        MessageBoxA(nullptr, error.what(), "WaterShader initialization error", MB_ICONERROR | MB_OK);
        return false;
    }

    return true;
}

void System::Run()
{
    MSG message = {};

    while (!shouldQuit_)
    {
        if (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessage(&message);

            if (message.message == WM_QUIT)
            {
                shouldQuit_ = true;
            }
        }
        else if (!Frame())
        {
            shouldQuit_ = true;
        }
    }
}

void System::Shutdown()
{
    if (graphics_)
    {
        graphics_->Shutdown();
        graphics_.reset();
    }

    timer_.reset();
    input_.reset();

    ShutdownImGui();
    ShutdownWindow();
}

LRESULT System::MessageHandler(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (imguiContextCreated_ && ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam))
    {
        return 1;
    }

    const ImGuiIO* io = imguiContextCreated_ ? &ImGui::GetIO() : nullptr;
    const bool imguiWantsKeyboard = io && io->WantCaptureKeyboard;

    switch (message)
    {
    case WM_KEYDOWN:
        if (input_ && !imguiWantsKeyboard)
        {
            input_->KeyDown(static_cast<unsigned int>(wParam));
        }
        return 0;
    case WM_KEYUP:
        if (input_ && !imguiWantsKeyboard)
        {
            input_->KeyUp(static_cast<unsigned int>(wParam));
        }
        return 0;
    case WM_SIZE:
        if (graphics_ && wParam != SIZE_MINIMIZED)
        {
            graphics_->Resize(LOWORD(lParam), HIWORD(lParam));
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        shouldQuit_ = true;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
}

bool System::Frame()
{
    timer_->Frame();
    input_->Frame();

    if (input_->IsEscapePressed())
    {
        return false;
    }

    return graphics_->Frame(timer_->GetDeltaTime());
}

bool System::InitializeWindow(HINSTANCE instance)
{
    instance_ = instance;
    gSystem = this;

    WNDCLASSEX windowClass = {};
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = WINDOW_CLASS_NAME;

    if (!RegisterClassEx(&windowClass))
    {
        return false;
    }

    DWORD windowStyle = WS_OVERLAPPEDWINDOW;
    RECT windowRect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    AdjustWindowRect(&windowRect, windowStyle, FALSE);

    const int windowWidth = windowRect.right - windowRect.left;
    const int windowHeight = windowRect.bottom - windowRect.top;
    const int posX = (GetSystemMetrics(SM_CXSCREEN) - windowWidth) / 2;
    const int posY = (GetSystemMetrics(SM_CYSCREEN) - windowHeight) / 2;

    hwnd_ = CreateWindowEx(
        0,
        WINDOW_CLASS_NAME,
        WINDOW_TITLE,
        windowStyle,
        posX,
        posY,
        windowWidth,
        windowHeight,
        nullptr,
        nullptr,
        instance_,
        nullptr);

    if (!hwnd_)
    {
        return false;
    }

    return true;
}

void System::ShowWindowAfterInitialize()
{
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    SetForegroundWindow(hwnd_);
    SetFocus(hwnd_);
}

void System::ShutdownWindow()
{
    if (hwnd_)
    {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }

    if (instance_)
    {
        UnregisterClass(WINDOW_CLASS_NAME, instance_);
        instance_ = nullptr;
    }

    gSystem = nullptr;
}

void System::ShutdownImGui()
{
    if (imguiWin32Initialized_)
    {
        ImGui_ImplWin32_Shutdown();
        imguiWin32Initialized_ = false;
    }

    if (imguiContextCreated_)
    {
        ImGui::DestroyContext();
        imguiContextCreated_ = false;
    }
}
