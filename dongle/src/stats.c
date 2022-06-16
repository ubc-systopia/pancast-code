#include "stats.h"
#include "storage.h"
#include "download.h"
#include "common/src/util/log.h"

dongle_stats_t stats;
extern dongle_config_t config;

void dongle_stats_reset()
{
  memset(&stats, 0, sizeof(dongle_stats_t));
}

extern void dongle_encounter_report();

void dongle_stats_init(void)
{
  // must call dongle_config_load before this
  char statbuf[1024];
  memset(statbuf, 0, 1024);

  dongle_storage_read_stat(statbuf, sizeof(dongle_stats_t));
  memcpy(&stats, statbuf, sizeof(dongle_stats_t));

  if (!stats.storage_checksum) {
    log_infof("%s", "Existing Statistics Found\r\n");
    dongle_encounter_report();
    dongle_stats();
    dongle_download_stats();
  } else {
    dongle_stats_reset();
  }
#if MODE__SL_DONGLE_TEST_CONFIG
  dongle_stats_reset();
  dongle_storage_save_stat(&config, &stats, sizeof(dongle_stats_t));
#endif
}

void dongle_stats()
{
  double xput = ((double) stats.periodic_data_size.mu * stats.periodic_data_size.n
      * BITS_PER_BYTE) / stats.total_periodic_data_time / Kbps;
  log_expf("[Legacy adv] #ephids: %d, #scan results: %lu\r\n",
      stats.enctr_rssi.n, stats.scan_rssi.n);
  stat_show(stats.scan_rssi, "[Legacy adv] Scan RSSI", "");
  stat_show(stats.enctr_rssi, "[Legacy adv] Enctr RSSI", "");
  stat_show(stats.periodic_data_rssi, "[Period adv] Data RSSI", "");
  stat_show(stats.periodic_data_size, "[Period adv] Pkt size", "bytes");
  log_expf("[Period adv] #rcvd: %f, #error: %lu, #bytes: %f"
      ", time: %f s, xput: %f Kbps\r\n",
      stats.periodic_data_size.n, stats.num_periodic_data_error,
      (stats.periodic_data_size.mu * stats.periodic_data_size.n),
      stats.total_periodic_data_time, xput);
}

void dongle_download_show_stats(download_stats_t * stats, char *name)
{
  log_expf("%s:\r\n", name);
  stat_show(stats->pkt_duplication, "Packet Duplication", "packet copies");
  stat_show(stats->est_pkt_loss, "Estimated loss rate", "% packets");
  stat_show(stats->n_bytes, "Bytes Received", "bytes");
  stat_show(stats->syncs_lost, "Syncs Lost", "syncs");
}

void dongle_download_stats()
{
  // ignore printing stats if no downloads even started
  // may be because beacon not configured to broadcast risk data yet
  if (stats.payloads_started == 0)
    return;

  log_expf("[Risk] started: %d completed: %d failed: %d "
      "decode fail: %d chunk switch: %d hwrx: %d crc fail: %d id matches: %d\r\n",
      stats.payloads_started, stats.payloads_complete, stats.payloads_failed,
      stats.cuckoo_fail, stats.switch_chunk, stats.total_hw_rx,
      stats.total_hw_crc_fail, stats.total_matches);

  stat_show(stats.completed_periodic_data_avg_payload_lat,
              "[Risk] payload download time", "ms");
  dongle_download_show_stats(&stats.completed_download_stats,
                             "=== [Risk] completed ===");

  if (!(stats.failed_download_stats.pkt_duplication.n == 0 &&
      stats.failed_download_stats.n_bytes.n == 0 &&
      stats.failed_download_stats.syncs_lost.n == 0 &&
      stats.failed_download_stats.est_pkt_loss.n == 0)) {
    dongle_download_show_stats(&stats.failed_download_stats,
        "=== [Risk] failed ===");
    dongle_download_show_stats(&stats.all_download_stats,
        "=== [Risk] overall ===");
  }
}
