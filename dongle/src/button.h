#ifndef __DONGLE_BUTTON_H__
#define __DONGLE_BUTTON_H__

#include "sl_simple_button_instances.h"
#include "sl_bluetooth.h"

#include "dongle.h"
#include "src/common/src/util/log.h"

extern const sl_button_t sl_button_btn0;

/**************************************************************************//**
 * Simple Button
 * Button state changed callback
 * @param[in] handle
 *    Button event handle
 *****************************************************************************/
void sl_button_on_change(const sl_button_t *handle)
{
  // Button pressed.
  if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {
    if (&sl_button_btn0 == handle) {
      log_expf("button pressed state: %d\r\n", sl_button_get_state(handle));
//      sl_bt_external_signal(BTN0_IRQ_EVENT);
    }

#if 0
    if (&sl_button_max86161_int == handle) {
      sl_bt_external_signal(MAXM86161_IRQ_EVENT);
    }
#endif
  }
}

static inline void configure_button(void)
{
//  simple_button_init();
//  sl_simple_button_init_instances();
  int ret = sl_button_init(&sl_button_btn0);
  sl_button_enable(&sl_button_btn0);
  int ret2 = sl_button_get_state(&sl_button_btn0);
  log_expf("button init ret: %d state: %d\r\n", ret, ret2);
}

#endif /* __DONGLE_BUTTON_H__ */
