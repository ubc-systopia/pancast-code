#include "stats.h"
#include "storage.h"
#include "download.h"
#include "common/src/util/log.h"

#include "nvm3_lib.h"

dongle_stats_t stats;
extern dongle_config_t config;

void dongle_stats_reset()
{
  memset(&stats, 0, sizeof(dongle_stats_t));
}

extern void dongle_encounter_report();

/*
 * Note: must call dongle_config_load before this
 */
void dongle_stats_init(void)
{
  dongle_storage_read_stat(&stats, sizeof(dongle_stats_t));
  log_expf("flash stat checksum: 0x%0x id 0x%0x id[0]: 0x%0x id[3]: 0x%0x\r\n",
      stats.storage_checksum, DONGLE_STORAGE_STAT_CHKSUM,
      ((uint8_t *) &config.id)[0], ((uint8_t *) &config.id)[3]);

  if (stats.storage_checksum == DONGLE_STORAGE_STAT_CHKSUM) {
    nvm3_load_stat(&stats);
    dongle_encounter_report();
    dongle_stats();
    dongle_download_stats();
  } else {
    dongle_stats_reset();
    stats.storage_checksum = DONGLE_STORAGE_STAT_CHKSUM;
    dongle_storage_save_stat(&config, &stats, sizeof(dongle_stats_t));
    nvm3_save_stat(&stats);
  }

#if MODE__SL_DONGLE_TEST_CONFIG
  dongle_stats_reset();
  stats.storage_checksum = DONGLE_STORAGE_STAT_CHKSUM;
  dongle_storage_save_stat(&config, &stats, sizeof(dongle_stats_t));
#endif
}

void dongle_stats()
{
  double xput = ((double) stats.stat_grp.periodic_data_size.mu *
      stats.stat_grp.periodic_data_size.n * BITS_PER_BYTE) /
    stats.stat_ints.total_periodic_data_time / Kbps;
  stat_show(stats.stat_grp.scan_rssi, "[Legacy adv] Scan RSSI", "");
  stat_show(stats.stat_grp.enctr_rssi, "[Legacy adv] Enctr RSSI", "");
  stat_show(stats.stat_grp.periodic_data_rssi, "[Period adv] Data RSSI", "");
  stat_show(stats.stat_grp.periodic_data_size, "[Period adv] Pkt size", "bytes");
  log_expf("[Period adv] #rcvd: %f, #error: %lu, #bytes: %f"
      ", time: %f s, xput: %f Kbps\r\n",
      stats.stat_grp.periodic_data_size.n, stats.stat_ints.num_periodic_data_error,
      (stats.stat_grp.periodic_data_size.mu * stats.stat_grp.periodic_data_size.n),
      stats.stat_ints.total_periodic_data_time, xput);
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
  if (stats.stat_ints.payloads_started == 0)
    return;

  log_expf("[Risk] started: %d completed: %d failed: %d "
      "decode fail: %d chunk switch: %d hwrx: %d crc fail: %d id matches: %d\r\n",
      stats.stat_ints.payloads_started, stats.stat_ints.payloads_complete,
      stats.stat_ints.payloads_failed, stats.stat_ints.cuckoo_fail,
      stats.stat_ints.switch_chunk, stats.stat_ints.total_hw_rx,
      stats.stat_ints.total_hw_crc_fail, stats.stat_ints.total_matches);

  stat_show(stats.stat_grp.completed_periodic_data_avg_payload_lat,
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
