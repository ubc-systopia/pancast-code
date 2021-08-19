/***************************************************************************//**
 * @file main.c
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

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include "sl_sleeptimer.h"
#include "sl_iostream.h"
#include "em_gpio.h"

#define DATA_SIZE 250
#define READ_SIZE 8
#define NUM_READS 31
#define MS_DELAY 1
#define TICK_DELAY 27
#define LOOP_DELAY 1920 // Delay in ticks, ~32 ticks per ms

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

int main(void) {
  // Initialize Silicon Labs device, system, service(s) and protocol stack(s).
  // Note that if the kernel is present, processing task(s) will be created by
  // this call.
  sl_system_init();

  // Initialize the application. For example, create periodic timer(s) or
  // task(s) if the kernel is present.
  app_init();

  // Set pin PB01 for output
  GPIO_PinModeSet(gpioPortB, 1, gpioModePushPull, 0);

  // Set up timers
  sl_status_t sc = sl_sleeptimer_init();

  // Main timer (for main clock)
  uint8_t main_timer_handle = MAIN_TIMER_HANDLE;
  sl_sleeptimer_timer_handle_t timer;
      sc = sl_sleeptimer_start_periodic_timer_ms(&timer,
                                 TIMER_1MS * BEACON_TIMER_RESOLUTION,
                                                 sl_timer_on_expire,
                                                 &main_timer_handle,
                                 MAIN_TIMER_PRIORT, 0);
      if (sc != SL_STATUS_OK) {
              printf("Error starting main timer \r\n", timer);
          } else {
              printf("Main Timer Started. handle=0x%02x\r\n", timer);
          }


#if defined(SL_CATALOG_KERNEL_PRESENT)
  // Start the kernel. Task(s) created in app_init() will start running.
  sl_system_kernel_start();
#else // SL_CATALOG_KERNEL_PRESENT
  while (1) {
    // Do not remove this call: Silicon Labs components process action routine
    // must be called from the super loop.
    sl_system_process_action();

    // Application process.
    app_process_action();

    /* Beacon application code starts here */

    int rlen = 0;

    // Start timer
    uint64_t start_time = sl_sleeptimer_get_tick_count64();

    GPIO_PinOutSet(gpioPortB, 1);

    uint8_t buf[DATA_SIZE];

    for (int i = 0; i < NUM_READS; i++) {

    	int len = 0;
    	int off =0;
    	int tot_len = 0;
    	int loops = 0;

#define LOOP_BREAK 1000
    	while (tot_len < READ_SIZE) {

    		len = read(SL_IOSTREAM_STDIN, &buf[(i*READ_SIZE) + tot_len], READ_SIZE - tot_len);
    		if (len < 0) {
    			continue;
    		}
    		tot_len = tot_len + len;
    		off = off + len;
    		loops++;
    		if (loops == LOOP_BREAK) {
    			break;
    		}
    	}

    	// ADD DELAY
//    	uint64_t start_delay = sl_sleeptimer_get_tick_count64();
//    	uint64_t end_delay = sl_sleeptimer_get_tick_count64();
//
//    	while ((end_delay - start_delay) < TICK_DELAY) {
//    		end_delay = sl_sleeptimer_get_tick_count64();
//    	}

    	add_delay_ticks(TICK_DELAY);

    	if (tot_len < 0) {
       		printf("errno: %u, i: %d\r\n", errno, i);
    	}
    	else {
    		rlen = rlen + tot_len;
    	}
    }

    if (NUM_READS * READ_SIZE < DATA_SIZE) {
    	int last_rlen = read(SL_IOSTREAM_STDIN, &buf[NUM_READS * READ_SIZE], DATA_SIZE-(NUM_READS * READ_SIZE));

       	if (last_rlen < 0) {
       		printf("errno: %u\r\n", errno);
       	}
       	else {
       	    rlen = rlen + last_rlen;
       	}
    }

    GPIO_PinOutClear(gpioPortB, 1);

    uint32_t seq;
    memcpy(&seq, &buf[0], sizeof(uint32_t)); // extract sequence number
    printf("sequence: %lu\r\n", seq);

    update_risk_data(rlen, &buf[0]);

    // End timer
    uint64_t end_time = sl_sleeptimer_get_tick_count64();
    uint32_t ms = sl_sleeptimer_tick_to_ms(end_time-start_time);
    printf("READ: %d, TIME: %lu\r\n", rlen, ms);

       // Print all data in buffer
//       for (int i= 0; i < DATA_SIZE; i++){
//       	printf("%d, ", buf[i]);
//       }
//       printf("\r\n");

    // ADD LOOP DELAY
//    uint64_t start_delay = sl_sleeptimer_get_tick_count64();
//    uint64_t end_delay = sl_sleeptimer_get_tick_count64();
//
//    while ((end_delay - start_delay) < LOOP_DELAY) {
//        end_delay = sl_sleeptimer_get_tick_count64();
//    }

    add_delay_ms(70);

    // End timer
    end_time = sl_sleeptimer_get_tick_count64();
    ms = sl_sleeptimer_tick_to_ms(end_time-start_time);
    printf("READ: %d, LOOP TIME: %lu\r\n", rlen, ms);

    /* Application code ends here */

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
    // Let the CPU go to sleep if the system allows it.
   // sl_power_manager_sleep();
#endif
  }
#endif // SL_CATALOG_KERNEL_PRESENT
}
