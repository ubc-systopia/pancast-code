#ifndef COMMON_SETTINGS__H
#define COMMON_SETTINGS__H

/*
 * CONFIGURATION PARAMETERS
 */


/*
 * number of time units between each report written to output
 */
#define DONGLE_REPORT_INTERVAL 60
#define BEACON_REPORT_INTERVAL 60

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
 * size of cuckoofilter in bytes
 */
#define MAX_FILTER_SIZE 2048 // 2kb
#define MAX_NUM_CHUNKS  64

#endif /* COMMON_SETTINGS__H */
