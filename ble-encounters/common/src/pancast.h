#define CONFIG_BT_EXT_ADV 0 // set to 1 to allow extended advertising

// hard-coded beacon information for development purposes
#define BEACON_ID		0x012345678
#define BEACON_LOC_ID	0x0123456789abcdef

#define BEACON_EPH_ID_HASH_LEN 14

typedef struct {
    uint8_t bytes[BEACON_EPH_ID_HASH_LEN];
} beacon_eph_id_t;

typedef uint64_t beacon_location_id_t;

typedef uint32_t beacon_timer_t;

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

static int encode_encounter(encounter_broadcast_raw_t*, encounter_broadcast_t*);
static int decode_encounter(encounter_broadcast_t*, encounter_broadcast_raw_t*);
