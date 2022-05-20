#ifndef COMMON_SETTINGS__H
#define COMMON_SETTINGS__H

#include "include/constants.h"

/*
 * Broadcasting constants
 * This defines a pseudo-unique identifier for
 * filtering out BLE packets received during operation.
 */
#define BROADCAST_SERVICE_ID 0x2222
static const beacon_id_t BEACON_SERVICE_ID_MASK = 0xffff0000;

/*
 * ==============
 * CONFIGURATIONS
 * ==============
 */

/*
 * 0 - load config from storage
 * 1 - use test config
 */
#define MODE__NRF_BEACON_TEST_CONFIG  1

/*
 * #time units between statistic reports
 */
#define BEACON_REPORT_INTERVAL 10

/*
 * uncomment to set global params for testing
 */
//#define PANCAST__TEST

#ifndef PANCAST__TEST

#include "include/settings.h"

#else

/*
 * Override global default settings
 */

// clock resolution in ms - this corresponds to a single time 'unit'
#define BEACON_TIMER_RESOLUTION 60000
#define PAYLOAD_ALTERNATE_TIMER  1000
#define LED_TIMER 1000

/*
 * number of time units (in minutes) in one epoch
 */
#define BEACON_EPOCH_LENGTH 15

/*
 * Tx power config limits for Nordic beacon
 */
#define MIN_TX_POWER -40
#define MAX_TX_POWER -20

#endif /* PANCAST__TEST */

#define FINGERPRINT_BITS    27

#define NUM_CF_BUCKETS      128
#define ENTRIES_PER_BUCKET  4

#endif /* COMMON_SETTINGS__H */
