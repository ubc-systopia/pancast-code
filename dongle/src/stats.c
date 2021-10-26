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
  stat_compute_thrpt(&stats);
  log_infof("[Legacy adv] #ephids: %d, #scan results: %lu\r\n",
      stats.num_obs_ids, stats.num_scan_results);
  stat_show(stats.scan_rssi, "[Legacy adv] Scan RSSI", "");
  stat_show(stats.encounter_rssi, "[Legacy adv] Logged Encounter RSSI", "");
  stat_show(stats.periodic_data_rssi, "[Period adv] Data RSSI", "");
  stat_show(stats.periodic_data_size, "[Period adv] Pkt size", "bytes");
  log_infof("[Period adv] #rcvd: %lu, #error: %lu, #bytes: %lu"
      ", time: %f, xput: %f\r\n",
      stats.num_periodic_data, stats.num_periodic_data_error,
      stats.total_periodic_data_size, stats.total_periodic_data_time,
      stats.periodic_data_avg_thrpt);
  dongle_storage_save_stat(sto, &stats, sizeof(dongle_stats_t));
  dongle_stats_reset();
}

void dongle_download_show_stats(download_stats_t * stats, char *name)
{
  log_infof("%s:\r\n", name);
  stat_show(stats->pkt_duplication, "Packet Duplication", "packet copies");
  stat_show(stats->est_pkt_loss, "Estimated loss rate", "% packets");
  stat_show(stats->n_bytes, "Bytes Received", "bytes");
  stat_show(stats->syncs_lost, "Syncs Lost", "syncs");
}

void dongle_download_stats()
{
  // ignore printing stats if no downloads even started
  // may be because beacon not configured to broadcast risk data yet
  if (download_stats.payloads_started == 0)
    return;

  log_infof("[Risk broadcast] #downloads: %d, completed: %d, failed: %d"
      ", decode fail: %d, chunk switch: %d\r\n",
      download_stats.payloads_started, download_stats.payloads_complete,
      download_stats.payloads_failed, download_stats.cuckoo_fail,
      download_stats.switch_chunk
      );
  stat_show(download_stats.complete_download_stats.periodic_data_avg_payload_lat,
              "[Risk broadcast] download time", "ms");
  dongle_download_show_stats(&download_stats.complete_download_stats.download_stats,
                             "=== [Risk broadcast] completed ===");
  dongle_download_show_stats(&download_stats.failed_download_stats,
      "=== [Risk broadcast] failed ===");
  dongle_download_show_stats(&download_stats.all_download_stats,
      "=== [Risk broadcast] overall ===");

  dongle_download_stats_init();
}
