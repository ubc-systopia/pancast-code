#ifndef COMMON_PANCAST_SETTINGS__H
#define COMMON_PANCAST_SETTINGS__H

// PANCAST SETTINGS

// These are the recommended settings for the system, based
// on the original PanCast white paper.

#define BEACON_TIMER_RESOLUTION 60000       // 1 min
#define DONGLE_TIMER_RESOLUTION 60000       // 1 min
#define BEACON_EPOCH_LENGTH 15
#define DONGLE_MAX_LOG_AGE (14 * 24 * 60)   // 14 days
#define DONGLE_ENCOUNTER_MIN_TIME 5

#endif