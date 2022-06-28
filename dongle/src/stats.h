#ifndef DONGLE_STATS__H
#define DONGLE_STATS__H

#include <stdint.h>

#include "dongle.h"
#include "storage.h"

#include "common/src/util/stats.h"

typedef int download_fail_reason;

typedef struct {
  stat_t pkt_duplication;
  stat_t n_bytes;
  stat_t syncs_lost;
  stat_t est_pkt_loss;
} download_stats_t;

typedef struct {
  /*
   * last report time
   */
  dongle_timer_t last_report_time;
  /*
   * last periodic adv. download successful complete time
   */
  dongle_timer_t last_download_end_time;
  /*
   * number of periodic adv. channel scans where scan returned an error
   */
  uint32_t num_periodic_data_error;
  /*
   * hw counters for total #pkts rcvd, crc failures
   */
  uint32_t total_hw_rx;
  uint32_t total_hw_crc_fail;
  /*
   * # of periodic payload downloads started
   */
  int payloads_started;
  /*
   * # of periodic payload downloads completed
   */
  int payloads_complete;
  /*
   * # of periodic payload downloads failed (e.g., due to pkt loss, corruption)
   */
  int payloads_failed;
  /*
   * # of ephids matching with the risk data entries
   */
  int total_matches;
  /*
   * in-memory count of new encounters logged since last reboot
   */
  enctr_entry_counter_t total_encounters;
  uint64_t numErasures;
  /*
   * # of periodic payload download failures with incorrect CF payload
   */
  download_fail_reason cuckoo_fail;
  /*
   * # of periodic payload download failures due to missed chunk
   */
  download_fail_reason switch_chunk;
  double total_periodic_data_time;   // seconds
} stat_ints_t;

typedef struct {
  uint8_t storage_checksum; // zero for valid stat data
  /*
   * integer fields of the statistics
   */
  stat_ints_t stat_ints;
  struct {
    /*
     * rssi stats for all legacy adv. scans invoked by dongle for pancast
     * (regardless of whether the ephemeral id is unique or not)
     */
    stat_t scan_rssi;
    /*
     * rssi stats for unique encounters on legacy adv. scans
     * invoked by dongle for pancast
     */
    stat_t enctr_rssi;
    /*
     * periodic data payload size stats
     */
    stat_t periodic_data_size;
    /*
     * periodic data rssi stats
     */
    stat_t periodic_data_rssi;
    /*
     * stats for download latency for periodic data (completed downloads only)
     */
    stat_t completed_periodic_data_avg_payload_lat;
  } stat_grp;
  /*
   * detailed stats for periodic data downloads
   */
  download_stats_t all_download_stats;
  download_stats_t failed_download_stats;
  download_stats_t completed_download_stats;
} dongle_stats_t;

void dongle_stats_reset();
//void dongle_stats_init();
void dongle_encounter_report(dongle_config_t *cfg, dongle_stats_t *stats);
void dongle_stats(dongle_stats_t *stats);

#endif
