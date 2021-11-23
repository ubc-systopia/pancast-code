#ifndef PANCAST__H
#define PANCAST__H

/*
 * =================
 * PANCAST CONSTANTS
 * =================
 */

/*
 * Constants common to all beacons and dongles
 */

#include <stdint.h>

#define BITS_PER_BYTE       8

/*
 * =================
 * Simple data types
 * =================
 */
typedef uint32_t beacon_id_t;
typedef uint32_t beacon_timer_t;
typedef uint64_t beacon_location_id_t;

/*
 * =================
 * Crypto key types
 * =================
 */

/*
 * maximum size of a secret key in bytes - used for data containing
 */
#define SK_MAX_SIZE 2048
/*
 * maximum size of a public key
 */
#define PK_MAX_SIZE 512

/*
 * size of a secret or public key
 */
typedef uint32_t key_size_t;

/*
 * Public key
 */
typedef struct
{
    uint8_t bytes[PK_MAX_SIZE];
} pubkey_t;

/*
 * Secret Key
 */
typedef struct
{
    uint8_t bytes[SK_MAX_SIZE];
} seckey_t;

/*
 * ==================
 * epoch counter type
 * ==================
 */
typedef uint32_t beacon_epoch_counter_t;

/*
 * Maximum size of a raw data payload to be sent over
 * legacy BLE advertising in a single packet
 */
#define MAX_BROADCAST_SIZE 31

/*
 * number of bytes used from hash for eph. id
 * should be 15 in prod. (currently not supported)
 */
#define BEACON_EPH_ID_HASH_LEN 14

/*
 * Total size of a Beacon's broadcast as raw bytes 'on the wire'
 * MUST be less than or equal to MAX_BROADCAST_SIZE
 */
#define ENCOUNTER_BROADCAST_SIZE                             \
    (BEACON_EPH_ID_HASH_LEN + sizeof(beacon_location_id_t) + \
     sizeof(beacon_id_t) + sizeof(beacon_timer_t))

/*
 * Representation of a BLE advertising payload
 */
typedef struct
{
    uint8_t bytes[MAX_BROADCAST_SIZE];
} encounter_broadcast_raw_t;

/*
 * size of containers used internally
 */
#define BEACON_EPH_ID_SIZE 16

/*
 * Ephemeral ID Type
 * Container size is set to an aligned value for implementation purposes;
 * Actual ID is the first BEACON_EPH_ID_HASH_LEN bytes of the array.
 */
typedef struct
{
    uint8_t bytes[BEACON_EPH_ID_SIZE];
} beacon_eph_id_t;

/*
 * Epoch calculation
 * t: the initial clock setting stored in the beacon
 * C: the current clock value of the beacon
 */
#define epoch_i(t, C) (beacon_epoch_counter_t)((t - C) / BEACON_EPOCH_LENGTH)

#endif
