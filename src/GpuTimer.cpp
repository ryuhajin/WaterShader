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
    if (frame.pending)
    {
        // Oldest slot still unresolved (GPU far behind): skip this frame instead of stalling.
        return;
    }
    context->Begin(frame.disjoint.Get());
    context->End(frame.begin.Get());
}

void GpuTimer::End(ID3D11DeviceContext* context)
{
    FrameQueries& frame = frames_[writeIndex_];
    if (!frame.pending)
    {
        context->End(frame.end.Get());
        context->End(frame.disjoint.Get());
        frame.pending = true;
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
        UINT64 begin = 0;
        UINT64 end = 0;
        if (context->GetData(frame.disjoint.Get(), &disjoint, sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
            context->GetData(frame.begin.Get(), &begin, sizeof(begin), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
            context->GetData(frame.end.Get(), &end, sizeof(end), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
        {
            return;
        }

        frame.pending = false;
        if (!disjoint.Disjoint && disjoint.Frequency > 0 && end >= begin)
        {
            lastMs_ = static_cast<float>(static_cast<double>(end - begin) * 1000.0 / static_cast<double>(disjoint.Frequency));
        }
    }
}
