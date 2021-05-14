#define BEACON_EPH_ID_HASH_LEN 14

typedef struct {
    uint8_t val[BEACON_EPH_ID_HASH_LEN];
} beacon_eph_id_t;

typedef uint64_t beacon_location_id_t;

typedef uint32_t beacon_timer_t;

typedef uint32_t beacon_id_t;

typedef struct {
    beacon_eph_id_t eph;
    beacon_location_id_t loc;
    beacon_id_t b;
    beacon_timer_t t;
} encounter_broadcast_t;

#define ENCOUNTER_BROADCAST_SIZE (      \
    BEACON_EPH_ID_HASH_LEN              \
       + sizeof(beacon_location_id_t)   \
       + sizeof(beacon_id_t)            \
       + sizeof(beacon_timer_t)         )