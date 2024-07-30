#pragma once

#include <chrono>
#include "pch.h"

using std::chrono::high_resolution_clock;

struct TimeEventArgs
{
    double ElapsedTime;
    double TotalTime;
};

class HighResolutionClock
{
public:
    HighResolutionClock();

    void Tick();

    void Reset();

    double GetDeltaNanoseconds() const;
    double GetDeltaMicroseconds() const;
    double GetDeltaMilliseconds() const;
    double GetDeltaSeconds() const;

    double GetTotalNanoseconds() const;
    double GetTotalMicroseconds() const;
    double GetTotalMilliSeconds() const;
    double GetTotalSeconds() const;

    TimeEventArgs GetTimeArgs();

private:
    high_resolution_clock::time_point m_t0;

    high_resolution_clock::duration m_deltaTime;
    high_resolution_clock::duration m_totalTime;
};