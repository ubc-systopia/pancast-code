#ifndef DONGLE_STATS__H
#define DONGLE_STATS__H

#include <stdint.h>

#include "storage.h"
#include "common/src/util/stats.h"


typedef struct {
  uint8_t storage_checksum; // zero for valid stat data

  dongle_timer_t start;
  uint32_t num_obs_ids;
  uint32_t num_scan_results;
  uint32_t num_periodic_data;
  uint32_t num_periodic_data_error;
  uint32_t total_packets_rx;
  uint32_t total_crc_fail;
  uint32_t total_periodic_data_size; // bytes
  double total_periodic_data_time;   // seconds

  stat_t scan_rssi;
  stat_t encounter_rssi;
  stat_t periodic_data_size;
  stat_t periodic_data_rssi;

  double periodic_data_avg_thrpt;

} dongle_stats_t;

typedef struct {
  stat_t pkt_duplication;
  stat_t n_bytes;
  stat_t syncs_lost;
  stat_t est_pkt_loss;
} download_stats_t;

typedef int download_fail_reason;

typedef struct {
  download_stats_t download_stats;
  stat_t periodic_data_avg_payload_lat; // download time
} complete_download_stats_t;

typedef struct {
    int payloads_started;
    int payloads_complete;
    int payloads_failed;
    download_fail_reason cuckoo_fail;
    download_fail_reason switch_chunk;
    download_stats_t all_download_stats;
    download_stats_t failed_download_stats;
    complete_download_stats_t complete_download_stats;
} downloads_stats_t;

void dongle_stats_init(dongle_storage *sto);
void dongle_stats();
void dongle_download_stats();
void dongle_download_stats_init();

#endif
