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

#include "../../common/src/pancast.h"
#include "../../common/src/log.h"

int main(void)
{
    // Initialize Silicon Labs device, system, service(s) and protocol stack(s).
    // Note that if the kernel is present, processing task(s) will be created by
    // this call.
    sl_system_init();

    // Initialize the application. For example, create periodic timer(s) or
    // task(s) if the kernel is present.
    app_init();

#if defined(SL_CATALOG_KERNEL_PRESENT)
    // Start the kernel. Task(s) created in app_init() will start running.
    sl_system_kernel_start();
#else // SL_CATALOG_KERNEL_PRESENT

    // Set up timers
    sl_status_t sc = sl_sleeptimer_init();

    // Main timer (for main clock)
    log_info("Starting main clock\r\n");
    uint8_t main_timer_handle = MAIN_TIMER_HANDLE;
    sl_sleeptimer_timer_handle_t timer;
    sc = sl_sleeptimer_start_periodic_timer_ms(&timer,
                               TIMER_1MS * BEACON_TIMER_RESOLUTION,
                                               sl_timer_on_expire,
                                               &main_timer_handle,
                               MAIN_TIMER_PRIORT, 0);
    if (sc != SL_STATUS_OK) {
            log_error("Error starting main timer \r\n", timer);
        } else {
            log_info("Main Timer Started. handle=0x%02x\r\n", timer);
        }

    int risk_timer_started = 0;

    extern float adv_start;
    float wait = -1; // wait time
    float time;      // cur time
    float delta;

    extern uint32_t timer_freq;
    extern uint64_t timer_ticks;

    while (1)
    {

#ifdef BEACON_MODE__NETWORK
        if (adv_start >= 0 && !risk_timer_started) {
            time = now();
            delta = time - ADV_START;
#define half_I ((((float)PER_ADV_INTERVAL)*1.25) / 2)
              int num_intervals = ((delta - 1)
                            / half_I) + 1;
              int next_interval = num_intervals
                                    + (num_intervals % 2 == 0 ? 1: 2);
              wait = next_interval * half_I;
#undef half_I
              while (time = now(), (delta = time - ADV_START), delta < wait) {
                  continue; // spin
              }
              risk_timer_started = 1;
              // start the risk update timer
              // start timer when time delta is a multiple of I/2
              // add an additional wait time to prevent problems at start
              // Risk Timer
              uint8_t risk_timer_handle = RISK_TIMER_HANDLE;
              // Interval is the same as the advertising interval
              sl_sleeptimer_timer_handle_t risk_timer;
              sc = sl_sleeptimer_start_periodic_timer_ms(&risk_timer,
                                                         PER_ADV_INTERVAL,
                                                         sl_timer_on_expire,
                                                         &risk_timer_handle,
                                         RISK_TIMER_PRIORT, 0);
              if (sc) {
                  log_errorf("Error starting risk timer: 0x%x\r\n", sc);
              } else {
                  log_info("Risk timer started at %f ms."
                            "delta=%f ms; "
                            "handle=0x%02x\r\n",
                           time, delta, risk_timer);
              }
        }
#endif

        // Do not remove this call: Silicon Labs components process action routine
        // must be called from the super loop.
        sl_system_process_action();

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
        // Let the CPU go to sleep if the system allows it.
        sl_power_manager_sleep();
#endif
    }
#endif // SL_CATALOG_KERNEL_PRESENT
}
