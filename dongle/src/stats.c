#include "stats.h"
#include "storage.h"

#include "common/src/util/log.h"

dongle_stats_t stats;

void dongle_stats_reset()
{
    memset(&stats, 0, sizeof(dongle_stats_t));
}

void dongle_stats_init(dongle_storage *sto)
{
    // must call dongle_config_load before this
    dongle_storage_read_stat(sto, &stats, sizeof(dongle_stats_t));
    if (!stats.storage_checksum)
    {
        log_info("Existing Statistics Found\r\n");
        dongle_stats(sto);
    }
    else
    {
        dongle_stats_reset();
    }
}

void stat_compute_thrpt()
{
    stats.periodic_data_avg_thrpt =
        ((double)stats.total_periodic_data_size * 0.008) /
        stats.total_periodic_data_time;
}

void dongle_stats(dongle_storage *sto)
{
    log_info("\r\n");
    log_info("Statistics:\r\n");
    log_infof("    Distinct Eph. IDs observed:          %d\r\n", stats.num_obs_ids);
    log_infof("    Legacy Scan Results:                 %lu\r\n", stats.num_scan_results);
    log_infof("    Periodic Pkts. Received:             %lu\r\n", stats.num_periodic_data);
    stat_show(stats.periodic_data_size, "Periodic Pkt. Size", "bytes");
    log_infof("    Periodic Pkts. Received (error):     %lu\r\n", stats.num_periodic_data_error);
    log_infof("    Total Bytes Transferred:             %lu\r\n", stats.total_periodic_data_size);
    log_infof("    Total Time (s):                      %f\r\n", stats.total_periodic_data_time);
    stat_compute_thrpt(&stats);
    log_infof("    Avg. Throughput (kb/s)               %f\r\n", stats.periodic_data_avg_thrpt);
    stat_show(stats.scan_rssi, "Legacy Scan RSSI", "");
    stat_show(stats.encounter_rssi, "Logged Encounter RSSI", "");
    stat_show(stats.periodic_data_rssi, "Periodic Data RSSI", "");
    dongle_storage_save_stat(sto, &stats, sizeof(dongle_stats_t));
    dongle_stats_reset();
}