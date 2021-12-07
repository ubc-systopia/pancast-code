#ifndef COMMON_PANCAST_SETTINGS__H
#define COMMON_PANCAST_SETTINGS__H

/*
 * ================
 * PANCAST SETTINGS
 * ================
 */

/*
 * Recommended settings for the system,
 * based on the original PanCast white paper.
 */

#define BEACON_TIMER_RESOLUTION 60000       // ms
#define DONGLE_TIMER_RESOLUTION 60000       // ms
#define PAYLOAD_ALTERNATE_TIMER  1000       // ms

#define BEACON_EPOCH_LENGTH 15              // minutes
#define DONGLE_MAX_LOG_AGE_DAYS 14          // days
#define DONGLE_MAX_LOG_AGE  \
  ((DONGLE_MAX_LOG_AGE_DAYS) * 24 * 60) / (BEACON_EPOCH_LENGTH)
#define DONGLE_ENCOUNTER_MIN_TIME 1         // minutes

#endif
