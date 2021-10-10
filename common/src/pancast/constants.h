#ifndef PANCAST__H
#define PANCAST__H

// PANCAST CONSTANTS

// Constants related to the original PanCast design

#include <stdint.h>

// Simple Data Types
typedef uint32_t beacon_id_t;
typedef uint32_t dongle_id_t;
typedef uint32_t beacon_timer_t;
typedef uint32_t dongle_timer_t;
typedef uint64_t beacon_location_id_t;
// for counting epochs
typedef uint32_t beacon_epoch_counter_t;
typedef uint32_t dongle_epoch_counter_t;

// Epoch calculation
// This matches the formula provided in the original PanCast paper
// where
// t is the initial clock setting stored in the beacon
// C is the current clock value of the beacon
#define epoch_i(t, C) (beacon_epoch_counter_t)((t - C) / BEACON_EPOCH_LENGTH)

#endif
