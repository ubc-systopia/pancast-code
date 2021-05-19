#include <stdint.h>

#define CONFIG_BT_EXT_ADV 0 // set to 1 to allow extended advertising

#define BEACON_SK_SIZE 512 // 4096 bits

typedef struct {
	uint8_t bytes[BEACON_SK_SIZE];
} beacon_sk_t;

// hard-coded beacon information for development purposes
#define BEACON_ID		0x012345678
#define BEACON_LOC_ID	0x0123456789abcdef

#define BEACON_EPOCH_LENGTH			15	 // number of timer cycles in one epoch should correspond to 15 min in prod.
#define BEACON_TIMER_RESOLUTION		5000 // in ms TODO: should be 1 min in prod.
#define DONGLE_TIMER_RESOLUTION		5000 // in ms TODO: should be 1 min in prod.

typedef uint32_t beacon_epoch_counter_t;
typedef uint32_t dongle_epoch_counter_t;

#define BEACON_EPH_ID_HASH_LEN 14		// number of trailing bytes used

typedef struct {
    uint8_t bytes[BEACON_EPH_ID_HASH_LEN];
} beacon_eph_id_t;

typedef uint64_t beacon_location_id_t;

typedef uint32_t beacon_timer_t;
typedef uint32_t dongle_timer_t;

typedef uint32_t beacon_id_t;

typedef struct {
    beacon_eph_id_t *eph;
    beacon_location_id_t *loc;
    beacon_id_t *b;
    beacon_timer_t *t;
} encounter_broadcast_t;

#define ENCOUNTER_BROADCAST_SIZE (      \
    BEACON_EPH_ID_HASH_LEN              \
       + sizeof(beacon_location_id_t)   \
       + sizeof(beacon_id_t)            \
       + sizeof(beacon_timer_t)         )

#define MAX_BROADCAST_SIZE 31

typedef struct {
    uint8_t bytes[MAX_BROADCAST_SIZE];
} encounter_broadcast_raw_t;

// epoch calculation
#define epoch_i(t, C) (beacon_epoch_counter_t)((t - C)/BEACON_EPOCH_LENGTH)

