#pragma once

#include <chrono>

class Timer
{
public:
    void Initialize();
    void Frame();

    float GetDeltaTime() const { return deltaTime_; }
    float GetTotalTime() const { return totalTime_; }

private:
    using Clock = std::chrono::steady_clock;

    Clock::time_point startTime_ = {};
    Clock::time_point previousTime_ = {};
    float deltaTime_ = 0.0f;
    float totalTime_ = 0.0f;
};

