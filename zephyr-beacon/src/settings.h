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
 * #time units between statistic reports
 */
#define BEACON_REPORT_INTERVAL 20

/*
 * clock resolution in ms - this corresponds to a single time 'unit'
 */
#define BEACON_TIMER_RESOLUTION 60000
#define PAYLOAD_ALTERNATE_TIMER  1000
#define LED_TIMER_MS 1000

/*
 * number of time units (in minutes) in one epoch
 */
#define BEACON_EPOCH_LENGTH 15

/*
 * Tx power config limits for Nordic beacon
 */
#define MIN_TX_POWER -40
#define MAX_TX_POWER -20

#endif /* COMMON_SETTINGS__H */
