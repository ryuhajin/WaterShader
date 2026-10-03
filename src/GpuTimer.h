#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <array>

// Measures GPU time between Begin() and End() with timestamp queries.
// Results are read a few frames later (ring buffer + DONOTFLUSH), so the CPU never waits on the GPU.
class GpuTimer
{
public:
    bool Initialize(ID3D11Device* device);
    void Shutdown();

    void Begin(ID3D11DeviceContext* context);
    void End(ID3D11DeviceContext* context);

    // Latest resolved frame time in milliseconds (negative until the first result arrives).
    float GetLastMs() const { return lastMs_; }

private:
    void Collect(ID3D11DeviceContext* context);

    static constexpr int kFramesInFlight = 4;

    struct FrameQueries
    {
        Microsoft::WRL::ComPtr<ID3D11Query> disjoint;
        Microsoft::WRL::ComPtr<ID3D11Query> begin;
        Microsoft::WRL::ComPtr<ID3D11Query> end;
        bool pending = false;
    };

    std::array<FrameQueries, kFramesInFlight> frames_;
    int writeIndex_ = 0;
    float lastMs_ = -1.0f;
};
