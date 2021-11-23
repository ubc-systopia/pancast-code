#ifndef COMMON_SETTINGS__H
#define COMMON_SETTINGS__H

/*
 * ==============
 * CONFIGURATIONS
 * ==============
 */

/*
 * 0 - init test config
 * 1 - load config from storage
 */
#define MODE__NRF_BEACON_TEST_CONFIG  1

/*
 * #time units between statistic reports
 */
#define BEACON_REPORT_INTERVAL 10

/*
 * uncomment to set global params for testing
 */
#define PANCAST__TEST

#ifndef PANCAST__TEST

#include "include/settings.h"

#else

/*
 * Override global default settings
 */

// clock resolution in ms - this corresponds to a single time 'unit'
#define BEACON_TIMER_RESOLUTION 60000
#define DONGLE_TIMER_RESOLUTION 60000

/*
 * number of time units (in minutes) in one epoch
 */
#define BEACON_EPOCH_LENGTH 15

#endif /* PANCAST__TEST */

#endif /* COMMON_SETTINGS__H */
