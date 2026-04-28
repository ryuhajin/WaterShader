#include "Timer.h"

void Timer::Initialize()
{
    startTime_ = Clock::now();
    previousTime_ = startTime_;
    deltaTime_ = 0.0f;
    totalTime_ = 0.0f;
}

void Timer::Frame()
{
    const auto currentTime = Clock::now();
    deltaTime_ = std::chrono::duration<float>(currentTime - previousTime_).count();
    totalTime_ = std::chrono::duration<float>(currentTime - startTime_).count();
    previousTime_ = currentTime;
}

