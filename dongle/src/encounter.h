#ifndef DONGLE_ENCOUNTER__H
#define DONGLE_ENCOUNTER__H

// Small utility API around the abstract encounter
// representation. Should be used in testing.

#include "dongle.h"

int compare_eph_id(beacon_eph_id_t *a, beacon_eph_id_t *b);
void _display_encounter_(dongle_encounter_entry *entry);
void display_eph_id_of(dongle_encounter_entry *entry);
void display_eph_id(beacon_eph_id_t *id);

#endif