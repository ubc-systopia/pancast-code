#ifndef DONGLE__H
#define DONGLE__H

#include "../../common/src/pancast.h"

#define FLASH_OFFSET        0x21000

typedef struct {
    beacon_eph_id_t *eph;
    beacon_location_id_t *loc;
    beacon_id_t *b;
    beacon_timer_t *t;
} encounter_broadcast_t;

#define padded(size) size + (flash_min_block_size - (size % flash_min_block_size))

#define ENCOUNTER_ENTRY_SIZE                        \
        (padded(BEACON_EPH_ID_HASH_LEN)             \
        + padded(sizeof(beacon_location_id_t))      \
        + padded(sizeof(beacon_id_t))               \
        + padded(sizeof(beacon_timer_t))            \
        + padded(sizeof(dongle_timer_t)))

typedef uint64_t enctr_entry_counter_t;

#define ENCOUNTER_LOG_OFFSET(i) (NV_STATE + (i * ENCOUNTER_ENTRY_SIZE))

static void dongle_scan(void);

#endif