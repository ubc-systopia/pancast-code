#include "time.h"

#include "sl_sleeptimer.h"

uint32_t timer_freq;
uint64_t timer_ticks;

void dongle_time_init()
{
  timer_ticks = 0;
  timer_freq = sl_sleeptimer_get_timer_frequency(); // Hz
}

/*
 * Return current system time in ms
 */
float dongle_time_now()
{
  timer_ticks = sl_sleeptimer_get_tick_count64();
  return (((float)timer_ticks) / ((float)timer_freq)) * 1000;
}
