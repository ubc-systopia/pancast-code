#ifndef DONGLE_ENCOUNTER__H
#define DONGLE_ENCOUNTER__H

// Small utility API around the abstract encounter
// representation. Should be used in testing.

#include "dongle.h"

void _display_encounter_(dongle_encounter_entry *entry);
void display_eph_id_of(dongle_encounter_entry *entry);

#endif