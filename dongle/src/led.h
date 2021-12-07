#ifndef __LED_H__
#define __LED_H__

#include "sl_simple_led_instances.h"
#include "sl_bluetooth.h"

#define LOG_LEVEL__INFO

#include "src/common/src/util/log.h"

extern const sl_led_t sl_led_led0;
/*
 * technically, this function is not required.
 * sl_simple_led_init_instances() is already called from sl_driver_init().
 * it is added there by the sdk when adding LED component to the project.
 */
static inline void configure_blinky(void)
{
//  sl_led_init(&sl_led_led0);
  sl_simple_led_init_instances();
  sl_simple_led_context_t *ctx = sl_led_led0.context;
  log_infof("Config LED port %d pin %d polarity %d\r\n", ctx->port,
      ctx->pin, ctx->polarity);
}

static inline void dongle_led_timer_handler(
    __attribute__ ((unused)) sl_sleeptimer_timer_handle_t *handle,
    __attribute__ ((unused)) void *data)
{
//  sl_led_toggle(&sl_led_led0);
  sl_led_toggle(SL_SIMPLE_LED_INSTANCE(0));
}

#endif /* __LED_H__ */
