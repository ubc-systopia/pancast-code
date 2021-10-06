#include <stdio.h>
#include <string.h>

#include "encounter.h"
#include "dongle.h"

#define LOG_LEVEL__INFO
#include "common/src/util/log.h"
#include "common/src/util/util.h"

// compares two ephemeral ids
int compare_eph_id(beacon_eph_id_t *a, beacon_eph_id_t *b)
{
#define A (a->bytes)
#define B (b->bytes)
    for (int i = 0; i < BEACON_EPH_ID_HASH_LEN; i++)
    {
        if (A[i] != B[i])
            return 1;
    }
#undef B
#undef A
    return 0;
}

void display_eph_id(beacon_eph_id_t *id)
{
    log_infof("EID: "
  "\\x%02x\\x%02x\\x%02x\\x%02x\\x%02x\\x%02x\\x%02x\\x%02x\\x%02x\\x%02x\\x%02x\\x%02x\\x%02x\\x%02x\r\n",
    id->bytes[0], id->bytes[1], id->bytes[2], id->bytes[3], id->bytes[4],
    id->bytes[5], id->bytes[6], id->bytes[7], id->bytes[8], id->bytes[9],
    id->bytes[10], id->bytes[11], id->bytes[12], id->bytes[13]);
}

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
    display_eph_id(&entry->eph_id);
}
