/***************************************************************************/ /**
 * @file
 * @brief main() function.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#include "sl_component_catalog.h"
#include "sl_system_init.h"
#include "app.h"
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif
#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "sl_system_kernel.h"
#else // SL_CATALOG_KERNEL_PRESENT
#include "sl_system_process_action.h"
#endif // SL_CATALOG_KERNEL_PRESENT

#include "src/common/src/constants.h"
#include "src/common/src/util/log.h"
#include "src/common/src/util/util.h"
#include "src/common/src/pancast/riskinfo.h"

#include <stdio.h>
#include <unistd.h>
#include "sl_iostream.h"
#include "em_gpio.h"

#include "src/led.h"

void add_delay_ticks(uint64_t ticks) {
  uint64_t start_delay = sl_sleeptimer_get_tick_count64();
  uint64_t end_delay = sl_sleeptimer_get_tick_count64();

  while ((end_delay - start_delay) < ticks) {
    end_delay = sl_sleeptimer_get_tick_count64();
  }
}

void add_delay_ms(uint32_t ms) {
  uint64_t ticks = (uint64_t)sl_sleeptimer_ms_to_tick(ms);

  add_delay_ticks(ticks);
}

int main(void)
{
  sl_status_t sc = 0;
  /*
   * Initialize Silicon Labs device, system, service(s) and
   * protocol stack(s). Note that if the kernel is present,
   * processing task(s) will be created by this call.
   */
  sl_system_init();

  /*
   * Initialize the application. For example, create periodic
   * timer(s) or task(s) if the kernel is present.
   */
  app_init();

#if defined(SL_CATALOG_KERNEL_PRESENT)
  /*
   * Start the kernel. Task(s) created in app_init() will start running.
   */
  sl_system_kernel_start();
#else // SL_CATALOG_KERNEL_PRESENT
#endif //

  /*
   * set pin PB01 for output
   */
  GPIO_PinModeSet(gpioPortB, 1, gpioModePushPull, 0);

  /*
   * main timer (for main clock)
   */
  uint8_t main_timer_handle = MAIN_TIMER_HANDLE;
  sl_sleeptimer_timer_handle_t timer;
  sc = sl_sleeptimer_start_periodic_timer_ms(&timer,
      BEACON_TIMER_RESOLUTION, sl_timer_on_expire,
      &main_timer_handle, MAIN_TIMER_PRIORT, 0);
  if (sc != SL_STATUS_OK) {
    log_errorf("Error starting main timer %d\r\n", sc);
  } else {
    log_infof("Beacon clock started, resolution: %u, handle=0x%02x\r\n",
        BEACON_TIMER_RESOLUTION, timer);
  }

  uint8_t *buf = malloc(DATA_SIZE);

#if BEACON_MODE__NETWORK
  int risk_timer_started = 0;

  extern float adv_start;
  float wait = -1; // wait time
  float time;      // cur time
  float delta;
#endif

  while (1) {

    memset(buf, 0, DATA_SIZE);

#define half_I ((((float)PER_ADV_INTERVAL)*1.25) / 2)
#if BEACON_MODE__NETWORK
    if (adv_start >= 0 && !risk_timer_started) {
      time = now();
      delta = time - ADV_START;
      int num_intervals = ((delta - 1) / half_I) + 1;
      int next_interval = num_intervals + (num_intervals % 2 == 0 ? 1: 2);
      wait = next_interval * half_I;

      // spin
      while (time = now(), (delta = time - ADV_START), delta < wait)
        continue;

      risk_timer_started = 1;
      log_infof("[periodic adv] risk timer start: %f ms delta: %f ms\r\n",
          time, delta);
      // TODO: make sure that the interval is synced properly below
    } else if (adv_start >= 0 && risk_timer_started) {

      /*
       * ===================================
       * Beacon application code starts here
       * ===================================
       */

#if (PERIODIC_TEST == 0)
/*
 * XXX: this is a hack!
 * we should only need this for the first couple of minutes
 */
#define LOOP_BREAK 10000

      int rlen = 0, len = 0, tot_len = 0, off = 0, loops = 0;

      // Start timer
      uint64_t start_time = sl_sleeptimer_get_tick_count64();

      GPIO_PinOutSet(gpioPortB, 1);

      while (tot_len < DATA_SIZE) {
        len = (DATA_SIZE - tot_len > READ_SIZE) ?
          READ_SIZE : (DATA_SIZE - tot_len);

        int r = 0;
        while (r < len) {
          loops++;
          if (loops >= LOOP_BREAK)
            break;

          rlen = read(SL_IOSTREAM_STDIN, buf+off+r, len-r);
          if (rlen < 0)
            continue;

          r += rlen;
        }

        off += len;
        tot_len += len;
        loops = 0;

        // add delay after read
        add_delay_ticks(TICK_DELAY);
      }

      GPIO_PinOutClear(gpioPortB, 1);

      // end timer
      uint64_t end_time = sl_sleeptimer_get_tick_count64();
      uint32_t ms = sl_sleeptimer_tick_to_ms(end_time-start_time);

      // extract sequence number
      rpi_ble_hdr *rbh __attribute__ ((unused)) = (rpi_ble_hdr *) buf;
      log_debugf("[periodic adv] rlen: %d tot_len: %d seq: %u chunk: %u "
          "len: %u time: %lu\r\n", rlen, tot_len, rbh->pkt_seq,
          rbh->chunkid, (uint32_t) rbh->chunklen, ms);

      if (rlen > 0) {
        set_risk_data(tot_len, buf);
      }

      // end timer
      end_time = sl_sleeptimer_get_tick_count64();
      ms = sl_sleeptimer_tick_to_ms(end_time-start_time);
      stat_add(ms, stats.broadcast_payload_update_duration);
      log_debugf("READ: %d, LOOP TIME: %lu\r\n", rlen, ms);

      // add second delay to sync up with advertising interval
      add_delay_ms(DATA_DELAY);

#else // PERIODIC_TEST

      // start timer
      float starttime = now();
      send_test_risk_data();
      add_delay_ms(PER_ADV_INTERVAL * 1.25);
      float endtime = now();
      log_debugf("LOOP TIME: %f\r\n", (endtime - starttime));

#endif // PERIODIC_TEST
    }
#endif // BEACON_MODE__NETWORK

    /*
     * ==========================
     * application code ends here
     * ==========================
     */

    /*
     * XXX: Do not remove this call.
     * Silicon Labs components process action routine
     * which must be called from the super loop.
     */
    sl_system_process_action();

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
    // Let the CPU go to sleep if the system allows it.
    // sl_power_manager_sleep();
#endif // SL_CATALOG_KERNEL_PRESENT
  }
}
