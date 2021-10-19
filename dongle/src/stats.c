#include "stats.h"
#include "storage.h"
#include "download.h"
#include "common/src/util/log.h"

dongle_stats_t stats;
downloads_stats_t download_stats;

void dongle_stats_reset()
{
  memset(&stats, 0, sizeof(dongle_stats_t));
}

void dongle_stats_init(dongle_storage *sto)
{
    // must call dongle_config_load before this
  dongle_storage_read_stat(sto, &stats, sizeof(dongle_stats_t));
  if (!stats.storage_checksum) {
    log_infof("%s", "Existing Statistics Found\r\n");
    dongle_stats(sto);
  } else {
    dongle_stats_reset();
  }
}

void dongle_download_stats_init()
{
  memset(&download_stats, 0, sizeof(downloads_stats_t));
}

void stat_compute_thrpt()
{
  stats.periodic_data_avg_thrpt =
      ((double)stats.total_periodic_data_size * 0.008) /
      stats.total_periodic_data_time;
}


void dongle_stats(dongle_storage *sto)
{
  log_infof("%s", "\r\n");
  log_infof("%s", "Statistics:\r\n");
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

void dongle_download_show_stats(download_stats_t * stats, char *name)
{
  log_infof("Download Statistics (%s):\r\n", name);
  stat_show(stats->pkt_duplication, "    Packet Duplication", "packet copies");
  stat_show(stats->est_pkt_loss, "    Estimated loss rate", "% packets");
  stat_show(stats->n_bytes, "    Bytes Received", "bytes");
  stat_show(stats->syncs_lost, "    Syncs Lost", "syncs");
}

void dongle_download_stats()
{
  log_infof("%s", "\r\n");
  log_infof("%s", "Risk Broadcast:\r\n");

  log_infof("    Downloads Started: %d\r\n", download_stats.payloads_started);
  log_infof("    Downloads Completed: %d\r\n", download_stats.payloads_complete);
  log_infof("    Downloads Failed: %d\r\n", download_stats.payloads_failed);
  log_infof("        decode fail:  %d\r\n", download_stats.cuckoo_fail);
  log_infof("        chunk switch: %d\r\n", download_stats.switch_chunk);
  dongle_download_show_stats(&download_stats.complete_download_stats.download_stats,
                             "completed");
  stat_show(download_stats.complete_download_stats.periodic_data_avg_payload_lat,
              "    Download time", "ms");
  dongle_download_show_stats(&download_stats.failed_download_stats, "failed");
  dongle_download_show_stats(&download_stats.all_download_stats, "overall");

  dongle_download_stats_init();
}
