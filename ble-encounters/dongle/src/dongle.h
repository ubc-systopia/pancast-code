#ifndef DONGLE__H
#define DONGLE__H

#include "../../common/src/pancast.h"

typedef struct {
    beacon_eph_id_t *eph;
    beacon_location_id_t *loc;
    beacon_id_t *b;
    beacon_timer_t *t;
} encounter_broadcast_t;

typedef uint64_t enctr_entry_counter_t;

typedef struct {
    uint64_t flags;
    uint64_t val;
} dongle_otp_t;

typedef struct {
	dongle_id_t                 id;
	dongle_timer_t              t_init;
	key_size_t                  backend_pk_size;        // size of backend public key
	pubkey_t                    backend_pk;             // Backend public key
	key_size_t                  dongle_sk_size;         // size of secret key
	seckey_t                    dongle_sk;              // Secret Key
} dongle_config_t;

typedef struct {
	beacon_location_id_t 		location_id;
	beacon_id_t			beacon_id;
	beacon_timer_t			beacon_time;
	dongle_timer_t			dongle_time;
	beacon_eph_id_t			eph_id;
} dongle_encounter_entry;


static void dongle_scan(void);

#endif