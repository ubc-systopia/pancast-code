#ifndef PANCAST__H
#define PANCAST__H

// Contains most of the assumptions and constants related to the PanCast system
// Including data-type size, timing parameters etc.
// Some of these may be adjusted for testing purposes.

//#define PANCAST__TEST // uncomment to set global params for testing

#include <stdint.h>

// Constants

// maximum size of a secret key in bytes - used for data containing
#define SK_MAX_SIZE 44
// maximum size of a public key
#define PK_MAX_SIZE 800
// number of bytes used from hash for eph. id - should be 15 in prod. (currently not supported)
#define BEACON_EPH_ID_HASH_LEN 14
// size of containers used internally
#define BEACON_EPH_ID_SIZE 16

// number of timer cycles in one epoch - should correspond to 15 min in prod.
#define BEACON_EPOCH_LENGTH 15
// Beacon clock resolution in ms - should be 1 min in prod.
#ifdef PANCAST__TEST
#define BEACON_TIMER_RESOLUTION 100000
#else
#define BEACON_TIMER_RESOLUTION 60000 // 1 min
#endif
// Dongle clock resolution in ms - should be 1 min in prod.
#ifdef PANCAST__TEST
#define DONGLE_TIMER_RESOLUTION 1000
#else
#define DONGLE_TIMER_RESOLUTION 60000 // 1 min
#endif
// Maximum age of an encounter in the dongle log, in time units. Should correspond to 14 days
#ifdef PANCAST__TEST
#define DONGLE_MAX_LOG_AGE 60
#else
#define DONGLE_MAX_LOG_AGE (14 * 24 * 60) // 14 days
#endif
// (Approx) number of time units between each report written to output
#ifdef PANCAST__TEST
#define DONGLE_REPORT_INTERVAL 60
#define BEACON_REPORT_INTERVAL 60
#else
#define DONGLE_REPORT_INTERVAL 10 // 10 min
#define BEACON_REPORT_INTERVAL 10 // 10 min
#endif

// Simple Data Types
typedef uint32_t beacon_id_t;
typedef uint32_t dongle_id_t;
typedef uint32_t beacon_timer_t;
typedef uint32_t dongle_timer_t;
typedef uint64_t beacon_location_id_t;
// size of a secret or public key
typedef uint32_t key_size_t;
// for counting epochs
typedef uint32_t beacon_epoch_counter_t;
typedef uint32_t dongle_epoch_counter_t;

// Ephemeral ID Type
// Container size is set to an aligned value for implementation purposes
// Actual ID is the first BEACON_EPH_ID_HASH_LEN bytes of the array.
typedef struct
{
    uint8_t bytes[BEACON_EPH_ID_SIZE];
} beacon_eph_id_t;

// Crypto. Key Types
// Public key
typedef struct
{
    uint8_t bytes[PK_MAX_SIZE];
} pubkey_t;
// Secret Key
typedef struct
{
    uint8_t bytes[SK_MAX_SIZE];
} seckey_t;
// Beacon Secret Key
typedef seckey_t beacon_sk_t;

// Maximum size of a raw data payload to be sent over legacy BLE advertising
// in a single packet
#define MAX_BROADCAST_SIZE 31

// Total size of a Beacon's broadcast as raw bytes 'on the wire'
// MUST be less than or equal to MAX_BROADCAST_SIZE
#define ENCOUNTER_BROADCAST_SIZE                             \
    (BEACON_EPH_ID_HASH_LEN + sizeof(beacon_location_id_t) + \
     sizeof(beacon_id_t) + sizeof(beacon_timer_t))

// Representation of a BLE advertising payload
typedef struct
{
    uint8_t bytes[MAX_BROADCAST_SIZE];
} encounter_broadcast_raw_t;

// Epoch calculation
// This matches the formula provided in the original PanCast paper
// where
// t is the initial clock setting stored in the beacon
// C is the current clock value of the beacon
#define epoch_i(t, C) (beacon_epoch_counter_t)((t - C) / BEACON_EPOCH_LENGTH)

// E_min: the number of time units for which a particular ephemeral id must be
// (continuously) observed before it is logged in dongle storage
#define DONGLE_ENCOUNTER_MIN_TIME 5 // corresponds to 5 minutes in production

// number of OTPs given to user and present in the dongle
#define NUM_OTP 16

// Broadcasting constants
// This defines a pseudo-unique identifier for filtering out bluetooth packets
// received during operation.
#define BROADCAST_SERVICE_ID 0x2222
static const beacon_id_t BEACON_SERVICE_ID_MASK = 0xffff0000;

// Risk Broadcast
#define RISK_BROADCAST_LEN_SIZE 8 // number of bytes used to indicate data len

#define MAX_FILTER_SIZE 2048 // 2kb
#define PER_ADV_SIZE 250
#define PACKET_HEADER_LEN (2*sizeof(uint32_t) + 8)
#define MAX_PACKET_SIZE (PER_ADV_SIZE - PACKET_HEADER_LEN)              // S
#define MAX_NUM_PACKETS_PER_FILTER ((MAX_FILTER_SIZE / MAX_PACKET_SIZE) + 1)

#endif
