#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

#include "../timeTicker.h"

static void expect_time(double actual, double expected)
{
    if (!isfinite(actual) || fabs(actual - expected) > 0.00000001) {
        fprintf(stderr, "Expected %.12f seconds, got %.12f\n", expected, actual);
        exit(EXIT_FAILURE);
    }
}

/* Catches smoothing/clamping that loses time when a frame stalls. */
static void test_countdown_and_frame_stall(void)
{
    TimeTicker ticker;
    TimeTickerInit(&ticker, -2.0, 100.0);
    expect_time(TimeTickerGetTime(&ticker, 200.0), -2.0);
    TimeTickerSetRunning(&ticker, true, 200.0);
    expect_time(TimeTickerGetTime(&ticker, 200.016), -1.984);
    expect_time(TimeTickerGetTime(&ticker, 200.032), -1.968);
    expect_time(TimeTickerGetTime(&ticker, 200.132), -1.868);
    expect_time(TimeTickerGetTime(&ticker, 200.148), -1.852);
}

/* Catches paused wall time leaking into the timeline on resume. */
static void test_pause_and_resume(void)
{
    TimeTicker ticker;
    TimeTickerInit(&ticker, -2.0, 10.0);
    TimeTickerSetRunning(&ticker, true, 10.0);
    TimeTickerSetRunning(&ticker, false, 12.5);
    expect_time(TimeTickerGetTime(&ticker, 10000.0), 0.5);
    TimeTickerSetRunning(&ticker, true, 10000.0);
    expect_time(TimeTickerGetTime(&ticker, 10000.25), 0.75);
    TimeTickerSetRunning(&ticker, true, 10000.25);
    expect_time(TimeTickerGetTime(&ticker, 10000.5), 1.0);
    TimeTickerSetRunning(&ticker, false, 10000.5);
    TimeTickerSetRunning(&ticker, false, 20000.0);
    expect_time(TimeTickerGetTime(&ticker, 30000.0), 1.0);
}

/* Catches adjustments being overwritten or changing the running state. */
static void test_adjustment_and_independent_tickers(void)
{
    TimeTicker first, second;
    TimeTickerInit(&first, -2.0, 0.0);
    TimeTickerInit(&second, 50.0, 0.0);
    TimeTickerSetTime(&first, -1.9, 10.0);
    expect_time(TimeTickerGetTime(&first, 20.0), -1.9);
    TimeTickerSetRunning(&first, true, 20.0);
    TimeTickerSetTime(&first, -1.0, 20.5);
    expect_time(TimeTickerGetTime(&first, 21.0), -0.5);
    expect_time(TimeTickerGetTime(&second, 21.0), 50.0);
    TimeTickerSetTime(&first, 10000000.125, 21.0);
    expect_time(TimeTickerGetTime(&first, 21.001), 10000000.126);
}

/* Exercises the real backend: elapsed time must include time spent asleep. */
static void test_real_clock(void)
{
    double before = ClockNow();
#if defined(_WIN32)
    Sleep(20);
#else
    struct timespec delay = {0, 20000000};
    nanosleep(&delay, NULL);
#endif
    double after = ClockNow();
    if (!isfinite(before) || !isfinite(after) || after - before < 0.01) {
        fputs("ClockNow did not measure elapsed time during sleep\n", stderr);
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < 10000; ++i) {
        double now = ClockNow();
        if (now < after) {
            fputs("ClockNow moved backwards\n", stderr);
            exit(EXIT_FAILURE);
        }
        after = now;
    }
}

int main(void)
{
    test_countdown_and_frame_stall();
    test_pause_and_resume();
    test_adjustment_and_independent_tickers();
    test_real_clock();
    puts("timeTicker: 4 tests passed");
    return EXIT_SUCCESS;
}
