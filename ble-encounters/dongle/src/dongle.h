#ifndef DONGLE__H
#define DONGLE__H

#include "../../common/src/pancast.h"

#define FLASH_WORD_SIZE     8
#define FLASH_OFFSET        0x21000

typedef struct {
    beacon_eph_id_t *eph;
    beacon_location_id_t *loc;
    beacon_id_t *b;
    beacon_timer_t *t;
} encounter_broadcast_t;

#define prev_multiple(k,n) ((n) - ((n) % (k)))
#define next_multiple(k,n) ((n) + ((k) - ((n) % (k))))

#define padded(size) next_multiple(flash_min_block_size, size)

#define ENCOUNTER_ENTRY_SIZE                        \
        next_multiple(FLASH_WORD_SIZE,			\
	padded(sizeof(beacon_location_id_t))       \
        + padded(sizeof(beacon_id_t))               \
        + padded(sizeof(beacon_timer_t))            \
        + padded(sizeof(dongle_timer_t))	    \
	+ padded(BEACON_EPH_ID_HASH_LEN))

typedef uint64_t enctr_entry_counter_t;

typedef struct {
    uint64_t flags;
    uint64_t val;
} dongle_otp_t;

typedef uint64_t flash_check_t;

static void dongle_scan(void);

#endif