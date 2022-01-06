/***************************************************************************//**
 * @file
 * @brief Application interface provided to main().
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#ifndef APP_H
#define APP_H

#include "em_common.h"

/**************************************************************************//**
 * Macros
 *****************************************************************************/
#define SCAN_PHY 1 // 1M PHY
#define SCAN_WINDOW 320
#define SCAN_INTERVAL 320
#define SCAN_MODE 0 // passive scan

#define SYNC_SKIP 0
#define SYNC_TIMEOUT 500 // Unit: 10 ms
#define SYNC_FLAGS 0

#define TIMER_1S 32768

#define TEST_DURATION 1800000

typedef struct {
	uint64_t start_ticks;
	uint64_t end_ticks;
	uint64_t diff;
	uint64_t diff_ms;
} timertest_t;

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
void app_init(void);

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
void app_process_action(void);

#endif // APP_H
