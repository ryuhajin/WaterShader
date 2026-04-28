#include "System.h"

#include <memory>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    auto system = std::make_unique<System>();

    if (!system->Initialize(instance))
    {
        return 1;
    }

    system->Run();
    system->Shutdown();
    system.reset();

    return 0;
}

