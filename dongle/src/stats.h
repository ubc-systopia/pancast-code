#ifndef DONGLE_STATS__H
#define DONGLE_STATS__H

#include <stdint.h>

#include "storage.h"
#include "common/src/util/stats.h"


typedef struct
{
    uint8_t storage_checksum; // zero for valid stat data

    uint32_t num_obs_ids;
    uint32_t num_scan_results;
    uint32_t num_periodic_data;
    uint32_t num_periodic_data_error;
    uint32_t total_periodic_data_size; // bytes
    double total_periodic_data_time;   // seconds

    stat_t scan_rssi;
    stat_t encounter_rssi;
    stat_t periodic_data_size;
    stat_t periodic_data_rssi;

    double periodic_data_avg_thrpt;

} dongle_stats_t;

void dongle_stats_init(dongle_storage *sto);
void dongle_stats(dongle_storage *sto);

#endif