#ifndef COMMON_SETTINGS__H
#define COMMON_SETTINGS__H

// CONFIGURATION
// Params for system operation

// #define MODE__TEST_CONFIG    // load test config rather than flash
#define TEST_DONGLE      1      // build dongle in test mode
#define BEACON_MODE__NETWORK    // comment to build as non-network beacon

#define GLOBAL_TX_POWER 10  // default
#define MIN_TX_POWER -3 // default
#define MAX_TX_POWER 100 // device maximum is 8.5 dbm

// number of time units between each report written to output
#define DONGLE_REPORT_INTERVAL 60
#define BEACON_REPORT_INTERVAL 60

// number of OTPs given to user and present in the dongle
#define NUM_OTP 16

// The following settings have default values that are specific to PanCast
// but can be manually overridden here.
//#define PANCAST__TEST       // uncomment to set global params for testing
#ifndef PANCAST__TEST
#include "pancast/settings.h"
#else

// clock resolution in ms - this corresponds to a single time 'unit'
#define BEACON_TIMER_RESOLUTION 100000
#define DONGLE_TIMER_RESOLUTION 1000

// number of time units in one epoch
#define BEACON_EPOCH_LENGTH 15

// Maximum age of an encounter in the dongle log, in time units. 
#define DONGLE_MAX_LOG_AGE 60

// E_min: the number of time units for which a particular ephemeral id must
// be continuously observed before it is logged by the dongle
#define DONGLE_ENCOUNTER_MIN_TIME 5

#endif

#if TEST_DONGLE
#define MODE__TEST_CONFIG
#endif

#endif
