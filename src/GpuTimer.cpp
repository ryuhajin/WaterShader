#include "GpuTimer.h"

bool GpuTimer::Initialize(ID3D11Device* device)
{
    D3D11_QUERY_DESC disjointDesc = {D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
    D3D11_QUERY_DESC timestampDesc = {D3D11_QUERY_TIMESTAMP, 0};

    for (FrameQueries& frame : frames_)
    {
        if (FAILED(device->CreateQuery(&disjointDesc, &frame.disjoint)) ||
            FAILED(device->CreateQuery(&timestampDesc, &frame.begin)) ||
            FAILED(device->CreateQuery(&timestampDesc, &frame.end)))
        {
            return false;
        }
        for (auto& mark : frame.marks)
        {
            if (FAILED(device->CreateQuery(&timestampDesc, &mark)))
            {
                return false;
            }
        }
    }
    return true;
}

void GpuTimer::Shutdown()
{
    for (FrameQueries& frame : frames_)
    {
        frame = FrameQueries{};
    }
}

void GpuTimer::Begin(ID3D11DeviceContext* context)
{
    Collect(context);

    FrameQueries& frame = frames_[writeIndex_];
    recording_ = !frame.pending;
    if (!recording_)
    {
        // Oldest slot still unresolved (GPU far behind): skip this frame instead of stalling.
        return;
    }
    frame.markCount = 0;
    context->Begin(frame.disjoint.Get());
    context->End(frame.begin.Get());
}

void GpuTimer::Mark(ID3D11DeviceContext* context)
{
    FrameQueries& frame = frames_[writeIndex_];
    if (recording_ && frame.markCount < kMaxMarks)
    {
        context->End(frame.marks[frame.markCount++].Get());
    }
}

void GpuTimer::End(ID3D11DeviceContext* context)
{
    FrameQueries& frame = frames_[writeIndex_];
    if (recording_)
    {
        context->End(frame.end.Get());
        context->End(frame.disjoint.Get());
        frame.pending = true;
        recording_ = false;
    }
    writeIndex_ = (writeIndex_ + 1) % kFramesInFlight;
}

void GpuTimer::Collect(ID3D11DeviceContext* context)
{
    // Walk from the oldest slot; stop at the first one the GPU has not finished.
    for (int i = 0; i < kFramesInFlight; ++i)
    {
        FrameQueries& frame = frames_[(writeIndex_ + i) % kFramesInFlight];
        if (!frame.pending)
        {
            continue;
        }

        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint = {};
        // Timestamps in order: begin, marks..., end.
        std::array<UINT64, kMaxMarks + 2> stamps = {};
        const int stampCount = frame.markCount + 2;
        if (context->GetData(frame.disjoint.Get(), &disjoint, sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
            context->GetData(frame.begin.Get(), &stamps[0], sizeof(UINT64), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
            context->GetData(frame.end.Get(), &stamps[stampCount - 1], sizeof(UINT64), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
        {
            return;
        }
        for (int mark = 0; mark < frame.markCount; ++mark)
        {
            if (context->GetData(frame.marks[mark].Get(), &stamps[mark + 1], sizeof(UINT64), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
            {
                return;
            }
        }

        frame.pending = false;
        if (disjoint.Disjoint || disjoint.Frequency == 0 || stamps[stampCount - 1] < stamps[0])
        {
            continue;
        }
        const double toMs = 1000.0 / static_cast<double>(disjoint.Frequency);
        lastMs_ = static_cast<float>(static_cast<double>(stamps[stampCount - 1] - stamps[0]) * toMs);
        lastSectionsMs_.fill(-1.0f);
        for (int section = 0; section + 1 < stampCount; ++section)
        {
            const UINT64 from = stamps[section];
            const UINT64 to = stamps[section + 1];
            lastSectionsMs_[section] = to >= from ? static_cast<float>(static_cast<double>(to - from) * toMs) : -1.0f;
        }
        ++resolvedCount_;
    }
}
