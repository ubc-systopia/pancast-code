#ifndef COMMON_CONSTANTS__H
#define COMMON_CONSTANTS__H

// CONSTANTS
// Constants and assumptions that are impmentation-specific, or
// otherwise not directly related to PanCast

#include <stdint.h>
#include "riskinfo.h"
#include "settings.h"

#define BITS_PER_BYTE       8
#define Kbps    1000

// maximum size of a secret key in bytes - used for data containing
#define SK_MAX_SIZE 2048
// maximum size of a public key
#define PK_MAX_SIZE 512
// number of bytes used from hash for eph. id - should be 15 in prod. (currently not supported)
#define BEACON_EPH_ID_HASH_LEN 14
/*
 * size of containers used internally
 * XXX: do not change this without updating the logic for storing encounters
 * in dongles, which encodes rssi in the last byte of eph_id
 */
#define BEACON_EPH_ID_SIZE 16

// Simple Data Types
typedef uint32_t beacon_id_t;
typedef uint32_t dongle_id_t;
typedef uint32_t beacon_timer_t;
typedef uint32_t dongle_timer_t;
typedef uint64_t beacon_location_id_t;
typedef int8_t   dongle_rssi_t;
// for counting epochs
typedef uint32_t beacon_epoch_counter_t;
typedef uint32_t dongle_epoch_counter_t;

// Epoch calculation
// This matches the formula provided in the original PanCast paper
// where
// t is the initial clock setting stored in the beacon
// C is the current clock value of the beacon
#define epoch_i(t, C) (beacon_epoch_counter_t)((t - C) / BEACON_EPOCH_LENGTH)

// size of a secret or public key
typedef uint32_t key_size_t;

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

// Broadcasting constants
// This defines a pseudo-unique identifier for filtering out bluetooth packets
// received during operation.
#define BROADCAST_SERVICE_ID 0x2222
static const beacon_id_t BEACON_SERVICE_ID_MASK = 0xffff0000;

// Risk Broadcast

#define PER_ADV_SIZE 250
#define PACKET_HEADER_LEN (sizeof(rpi_ble_hdr))
#define MAX_PAYLOAD_SIZE (PER_ADV_SIZE - PACKET_HEADER_LEN)              // S
#define MAX_NUM_PACKETS_PER_FILTER (((MAX_FILTER_SIZE-1) / MAX_PAYLOAD_SIZE) + 1)

#endif /* COMMON_CONSTANTS__H */
