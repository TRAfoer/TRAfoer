#ifndef TIME_TICKER_H
#define TIME_TICKER_H

#include <stdbool.h>

typedef struct TimeTicker {
    double anchorTime;
    double anchorClock;
    bool running;
} TimeTicker;

/* Monotonic seconds from an arbitrary origin; aborts if the OS clock fails. */
double ClockNow(void);

/* All now arguments are seconds from ClockNow(), using one sample per frame.
   A ticker starts paused. Access a given ticker from one thread at a time. */
void TimeTickerInit(TimeTicker *ticker, double initialTime, double now);
double TimeTickerGetTime(const TimeTicker *ticker, double now);
void TimeTickerSetRunning(TimeTicker *ticker, bool running, double now);

/* Re-anchor the timeline without changing whether it is running. */
void TimeTickerSetTime(TimeTicker *ticker, double time, double now);

#endif
