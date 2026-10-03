#include "System.h"

#include <objbase.h>

#include <memory>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    // WIC (JPG/PNG loading, screenshot saving) is COM. Without this the UI thread only works by
    // accident, as an implicit MTA member once the D3D driver has spun up its own MTA threads.
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(comResult))
    {
        return 1;
    }

    int exitCode = 0;
    {
        auto system = std::make_unique<System>();

        if (system->Initialize(instance))
        {
            system->Run();
        }
        else
        {
            exitCode = 1;
        }

        system->Shutdown();
    }

    CoUninitialize();
    return exitCode;
}
