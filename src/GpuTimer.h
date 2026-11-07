#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <array>

// Measures GPU time between Begin() and End() with timestamp queries.
// Mark() splits the frame into sections (e.g. opaque scene / water / tonemap + UI); each section's time is
// the gap between neighboring timestamps.
// Results are read a few frames later (ring buffer + DONOTFLUSH), so the CPU never waits on the GPU.
class GpuTimer
{
public:
    static constexpr int kMaxMarks = 2;
    static constexpr int kMaxSections = kMaxMarks + 1;

    bool Initialize(ID3D11Device* device);
    void Shutdown();

    void Begin(ID3D11DeviceContext* context);
    // Ends the current section and starts the next one. Up to kMaxMarks per frame.
    void Mark(ID3D11DeviceContext* context);
    void End(ID3D11DeviceContext* context);

    // Latest resolved frame time in milliseconds (negative until the first result arrives).
    float GetLastMs() const { return lastMs_; }
    // Latest resolved section times (Begin -> mark 1, mark 1 -> mark 2, ..., last mark -> End); negative = none.
    const std::array<float, kMaxSections>& GetLastSectionsMs() const { return lastSectionsMs_; }
    // Frames resolved so far (a new result to read when it changes).
    unsigned long long GetResolvedCount() const { return resolvedCount_; }

private:
    void Collect(ID3D11DeviceContext* context);

    static constexpr int kFramesInFlight = 4;

    struct FrameQueries
    {
        Microsoft::WRL::ComPtr<ID3D11Query> disjoint;
        Microsoft::WRL::ComPtr<ID3D11Query> begin;
        std::array<Microsoft::WRL::ComPtr<ID3D11Query>, kMaxMarks> marks;
        Microsoft::WRL::ComPtr<ID3D11Query> end;
        int markCount = 0;
        bool pending = false;
    };

    std::array<FrameQueries, kFramesInFlight> frames_;
    int writeIndex_ = 0;
    bool recording_ = false; // Begin() started this frame's queries (false when the slot was still pending)
    float lastMs_ = -1.0f;
    std::array<float, kMaxSections> lastSectionsMs_ = {-1.0f, -1.0f, -1.0f};
    unsigned long long resolvedCount_ = 0;
};
