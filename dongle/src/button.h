#ifndef __DONGLE_BUTTON_H__
#define __DONGLE_BUTTON_H__

#include "sl_simple_button_instances.h"
#include "sl_bluetooth.h"

#include "dongle.h"
#include "nvm3_lib.h"
#include "stats.h"
#include "src/common/src/util/log.h"

#define BUTTON_DELAY_SHORT_MS  1000
extern const sl_button_t sl_button_btn0;
extern dongle_config_t config;
extern dongle_stats_t stats;
extern dongle_epoch_counter_t epoch; // current epoch
extern dongle_timer_t dongle_time; // main dongle timer
extern dongle_encounter_entry_t *cur_encounters;
extern size_t cur_id_idx;
extern download_t download;


/*
 * index 0 - SL_SIMPLE_BUTTON_RELEASED
 * index 1 - SL_SIMPLE_BUTTON_PRESSED
 * index 2 - SL_SIMPLE_BUTTON_DISABLED (unused)
 */
float button_time[3] = { 0.0, 0.0 };

/**************************************************************************//**
 * Simple Button
 * Button state changed callback
 * @param[in] handle
 *    Button event handle
 *****************************************************************************/
void sl_button_on_change(const sl_button_t *handle)
{
  sl_button_state_t state = sl_button_get_state(handle);
  button_time[state] = now();

  /*
   * ignore button press on other buttons
   */
  if (&sl_button_btn0 != handle)
    return;

  log_expf("button state: %d ts: %.0f\r\n", state, button_time[state]);
  /*
   * we only need the timestamp for button press
   */
  if (state > SL_SIMPLE_BUTTON_RELEASED)
    return;

#if 0
//      sl_bt_external_signal(BTN0_IRQ_EVENT);

  if (&sl_button_max86161_int == handle) {
    sl_bt_external_signal(MAXM86161_IRQ_EVENT);
  }
#endif

  float button_delay = button_time[SL_SIMPLE_BUTTON_RELEASED] - button_time[SL_SIMPLE_BUTTON_PRESSED];
  button_time[SL_SIMPLE_BUTTON_RELEASED] = button_time[SL_SIMPLE_BUTTON_PRESSED] = 0;
  log_expf("button delay: %.0f\r\n", button_delay);

  // should not happen ideally
  if (button_delay < 0)
    return;

  // on a short button press, only reset log
  config.en_head = 0;
  config.en_tail = 0;

  // on a long button press, additionally reset clock, stats, and ongoing scans
  if (button_delay > (float) BUTTON_DELAY_SHORT_MS) {
    dongle_time = config.t_cur = config.t_init;
    cur_id_idx = epoch = download_complete = 0;
    memset(cur_encounters, 0,
        sizeof(dongle_encounter_entry_t) * DONGLE_MAX_BC_TRACKED);
    dongle_stats_reset();
    nvm3_save_stat(&stats);
    dongle_download_init();
  }

  nvm3_save_config(&config);
  dongle_info();

  log_expf("==== UPLOAD LOG START [%u:%u] ===\r\n",
      config.en_tail, config.en_head);
  dongle_storage_load_encounter(config.en_tail,
      num_encounters_current(config.en_head, config.en_tail),
      dongle_print_encounter);
  log_expf("%s", "==== UPLOAD LOG END ====\r\n");

  nvm3_load_stat(&stats);
  dongle_encounter_report(&config, &stats);
  dongle_stats(&stats);
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
