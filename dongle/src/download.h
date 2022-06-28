#ifndef DONGLE_DOWNLOAD__H
#define DONGLE_DOWNLOAD__H

#include <stdint.h>

#include "common/src/constants.h"

typedef struct {
  int is_active;
  double time;
  uint32_t n_syncs_lost;
  uint32_t n_total_packets;
  uint32_t n_corrupt_packets;
  uint32_t n_matches;
  struct {
    // map of sequence number to packet count for that number
    // used to track completion of the download
    uint32_t counts[MAX_NUM_PACKETS_PER_FILTER];

    // number of unique packets seen
    int num_distinct;

    // number of bytes received
    uint32_t received;

    // the current chunk being downloaded
    uint32_t chunk_num;

    // actual received payload
    struct {
      uint64_t data_len;
      uint8_t data[MAX_FILTER_SIZE];
    } buffer;

  } packet_buffer;
} download_t;

// Count packet duplication
#define dongle_download_duplication(s, d) \
  for (int i = 0; i < (int) MAX_NUM_PACKETS_PER_FILTER; i++) { \
    uint32_t count = d.packet_buffer.counts[i]; \
    if (count > 0) { \
      stat_add(count, s.pkt_duplication); \
    } \
  }

#define dongle_update_download_stats(s, d) \
  stat_add(d.packet_buffer.received, s.n_bytes); \
  stat_add(d.n_syncs_lost, s.syncs_lost); \
  dongle_download_duplication(s, d) \
  stat_add(dongle_download_esimtate_loss(&d), s.est_pkt_loss)

void dongle_download_init();
void dongle_download_info();
void dongle_download_complete();
void dongle_download_fail();

void dongle_on_periodic_data(uint8_t *data, uint8_t data_len, int8_t rssi);
void dongle_on_periodic_data_error(int8_t rssi);
void dongle_on_sync_lost();

int dongle_download_complete_status();

#endif
