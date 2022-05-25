#ifndef __LED_H__
#define __LED_H__

#include "sl_simple_led_instances.h"
#include "sl_bluetooth.h"

#include "dongle.h"
#include "src/common/src/util/log.h"

extern const sl_led_t sl_led_led0;
extern sl_sleeptimer_timer_handle_t led_timer;

static inline void dongle_led_timer_handler(
    __attribute__ ((unused)) sl_sleeptimer_timer_handle_t *handle,
    __attribute__ ((unused)) void *data)
{
  sl_led_toggle(SL_SIMPLE_LED_INSTANCE(0));
}

/*
 * technically, this function is not required.
 * sl_simple_led_init_instances() is already called from sl_driver_init().
 * it is added there by the sdk when adding LED component to the project.
 */
static inline void configure_blinky(void)
{
  sl_simple_led_init_instances();
  sl_simple_led_context_t *ctx = sl_led_led0.context;
  sl_sleeptimer_restart_periodic_timer_ms(&led_timer, LED_TIMER_MS,
      dongle_led_timer_handler, (void *) NULL, 0, 0);
  log_infof("Config LED port %d pin %d polarity %d\r\n", ctx->port,
      ctx->pin, ctx->polarity);
}

static inline void dongle_led_notify(void) {
  sl_status_t sc;
  log_infof("%s", "in alert\r\n");
#if 0
  sc = sl_sleeptimer_stop_timer(&led_timer);
  if (sc != SL_STATUS_OK) {
    log_errorf("failed period led timer stop, sc: %d\r\n", sc);
  }
#endif
  sc = sl_sleeptimer_restart_periodic_timer_ms(&led_timer, LED_TIMER_MS/4,
      dongle_led_timer_handler, (void *) NULL, 0, 0);
  if (sc != SL_STATUS_OK) {
    log_errorf("failed period led timer start, sc: %d\r\n", sc);
  }
}

#endif /* __LED_H__ */
