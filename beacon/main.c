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

extern sl_sleeptimer_timer_handle_t hp_timer;

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

    // High-Precision clock
    uint8_t hp_timer_handle = HP_TIMER_HANDLE;
    sc = sl_sleeptimer_start_periodic_timer_ms(&hp_timer,
                                     TIMER_1MS,
                                     sl_timer_on_expire,
                                     &hp_timer_handle,
                                   HP_TIMER_PRIORT, 0);

    // Main timer (for main clock)
    uint8_t main_timer_handle = MAIN_TIMER_HANDLE;
    sl_sleeptimer_timer_handle_t timer;
    sc = sl_sleeptimer_start_periodic_timer_ms(&timer,
                               TIMER_1MS * BEACON_TIMER_RESOLUTION,
                                               sl_timer_on_expire,
                                               &main_timer_handle,
                               MAIN_TIMER_PRIORT, 0);
    while (1)
    {
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
