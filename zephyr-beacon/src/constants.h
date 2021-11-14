#ifndef COMMON_CONSTANTS__H
#define COMMON_CONSTANTS__H

/*
 * Constants and assumptions that are impmentation-specific,
 * or otherwise not directly related to PanCast
 */

#include <stdint.h>
#include "include/constants.h"

/*
 * Beacon Secret Key
 */
typedef seckey_t beacon_sk_t;

/*
 * Broadcasting constants
 * This defines a pseudo-unique identifier for
 * filtering out BLE packets received during operation.
 */
#define BROADCAST_SERVICE_ID 0x2222
static const beacon_id_t BEACON_SERVICE_ID_MASK = 0xffff0000;

/*
 * Tx power config limits for Nordic beacon
 */
#define MIN_TX_POWER -40
#define MAX_TX_POWER 4

#endif /* COMMON_CONSTANTS__H */
