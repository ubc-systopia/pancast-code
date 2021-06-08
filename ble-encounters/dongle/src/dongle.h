#ifndef DONGLE__H
#define DONGLE__H

#include "../../common/src/pancast.h"

typedef struct {
    beacon_eph_id_t *eph;
    beacon_location_id_t *loc;
    beacon_id_t *b;
    beacon_timer_t *t;
} encounter_broadcast_t;

static void dongle_scan(void);

#endif