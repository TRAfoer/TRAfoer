/* Keep the POSIX clock declarations visible even with strict C99 flags. */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "timeTicker.h"
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__) && defined(__MACH__)
#include <mach/mach_time.h>
#elif defined(__unix__) || defined(__linux__)
#include <time.h>
#else
#error "timeTicker needs a monotonic clock backend for this platform"
#endif

double ClockNow(void)
{
#if defined(_WIN32)
    LARGE_INTEGER counter, frequency;
    /* Local values keep this function safe to call from multiple threads. */
    if (!QueryPerformanceFrequency(&frequency) ||
        !QueryPerformanceCounter(&counter)) {
        fputs("timeTicker: performance counter unavailable\n", stderr);
        abort();
    }
    return (double)counter.QuadPart / (double)frequency.QuadPart;
#elif defined(__APPLE__) && defined(__MACH__)
    mach_timebase_info_data_t timebase;
    if (mach_timebase_info(&timebase) != KERN_SUCCESS) {
        fputs("timeTicker: Mach clock unavailable\n", stderr);
        abort();
    }
    return (double)mach_absolute_time() * (double)timebase.numer /
           (double)timebase.denom / 1000000000.0;
#else
    struct timespec timestamp;
    if (clock_gettime(CLOCK_MONOTONIC, &timestamp) != 0) {
        perror("timeTicker: clock_gettime");
        abort();
    }
    return (double)timestamp.tv_sec + (double)timestamp.tv_nsec / 1000000000.0;
#endif
}

void TimeTickerInit(TimeTicker *ticker, double initialTime, double now)
{
    ticker->anchorTime = initialTime;
    ticker->anchorClock = now;
    ticker->running = false;
}

double TimeTickerGetTime(const TimeTicker *ticker, double now)
{
    return ticker->anchorTime +
           (ticker->running ? now - ticker->anchorClock : 0.0);
}

void TimeTickerSetRunning(TimeTicker *ticker, bool running, double now)
{
    if (ticker->running == running) {
        return;
    }
    ticker->anchorTime = TimeTickerGetTime(ticker, now);
    ticker->anchorClock = now;
    ticker->running = running;
}

void TimeTickerSetTime(TimeTicker *ticker, double time, double now)
{
    ticker->anchorTime = time;
    ticker->anchorClock = now;
}
