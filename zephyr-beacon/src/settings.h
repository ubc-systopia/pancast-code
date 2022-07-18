#ifndef COMMON_SETTINGS__H
#define COMMON_SETTINGS__H

#include <constants.h>

/*
 * =========================
 * CONFIGURATIONS PARAMETERS
 * =========================
 */

/*
 * 0 - load config from storage
 * 1 - use test config
 */
#define MODE__NRF_BEACON_TEST_CONFIG  0

/*
 * button to reset beacon state
 * 1 - enable use of button to reset beacon state
 * 0 - disable button
 */
#define BEACON_BUTTON_RESET 0

/*
 * #time units between statistic reports
 */
#define BEACON_REPORT_INTERVAL 60

/*
 * clock resolution in ms - this corresponds to a single time 'unit'
 */
#define BEACON_TIMER_RESOLUTION 60000
#define PAYLOAD_ALTERNATE_TIMER  1000
#define LED_TIMER_MS 2000

/*
 * number of time units (in minutes) in one epoch
 */
#define BEACON_EPOCH_LENGTH 15

/*
 * Tx power config limits for Nordic beacon
 */
#define MIN_TX_POWER -10
#define MAX_TX_POWER 0

/*
 * Advertising interval settings
 * Zephyr-recommended values are used
 *
 * unit is 0.625 ms, i.e.,
 * if min_interval = 0x320, actual interval is 500ms
 */
//#define BEACON_ADV_MIN_INTERVAL BT_GAP_ADV_FAST_INT_MIN_1
//#define BEACON_ADV_MAX_INTERVAL BT_GAP_ADV_FAST_INT_MAX_1
#define BEACON_ADV_MIN_INTERVAL 0x320
#define BEACON_ADV_MAX_INTERVAL 0x640

#endif /* COMMON_SETTINGS__H */
