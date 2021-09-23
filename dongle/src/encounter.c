#include <stdio.h>
#include <string.h>

#include "encounter.h"
#include "dongle.h"

#define LOG_LEVEL__INFO
#include "common/src/util/log.h"
#include "common/src/util/util.h"

void _display_encounter_(dongle_encounter_entry *entry)
{
    log_info("Encounter data:\r\n");
    log_infof(" t_d: %lu,", entry->dongle_time);
    log_infof(" b: %lu,", entry->beacon_id);
    log_infof(" t_b: %lu,", entry->beacon_time);
    log_infof(" loc: %llu\r\n", entry->location_id);
    display_eph_id_of(entry);
}

void display_eph_id_of(dongle_encounter_entry *entry)
{
    info_bytes(entry->eph_id.bytes, BEACON_EPH_ID_HASH_LEN, "Eph. ID");
}
