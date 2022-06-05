#ifndef COMMON_SETTINGS__H
#define COMMON_SETTINGS__H

/*
 * CONFIGURATION PARAMETERS
 */


/*
 * beacon Tx power
 */
#define GLOBAL_TX_POWER 5
#define MIN_TX_POWER -3
#define MAX_TX_POWER 10 // device maximum is 8.5 dbm

/*
 * number of time units between each report written to output
 */
#define DONGLE_REPORT_INTERVAL 60
#define BEACON_REPORT_INTERVAL 60

/*
 * number of OTPs given to user and present in the dongle
 */
#define NUM_OTP 16

/*
 * clock resolution in ms - this corresponds to a single time 'unit'
 */
#define BEACON_TIMER_RESOLUTION 60000
#define DONGLE_TIMER_RESOLUTION 60000

/*
 * number of time units in one epoch
 */
#define BEACON_EPOCH_LENGTH 15

/*
 * maximum age of an encounter in the dongle log, in time units.
 */
#define DONGLE_MAX_LOG_AGE (14 * 24 * 60)

#endif /* COMMON_SETTINGS__H */
