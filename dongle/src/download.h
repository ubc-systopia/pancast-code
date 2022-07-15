#ifndef DONGLE_DOWNLOAD__H
#define DONGLE_DOWNLOAD__H

#include <stdint.h>

#include "common/src/constants.h"
#include "storage.h"

typedef struct {
  int is_active;
  double time;
  uint32_t n_syncs_lost;
  uint32_t n_total_packets;
  uint32_t n_corrupt_packets;
  uint32_t n_matches;
  struct {
    // number of unique packets seen
    int num_distinct;

    // number of bytes received
    uint32_t received;

    // the current chunk being downloaded
    uint32_t cur_chunkid;
    uint32_t numchunks;

    // track of seq# recvd in each chunk
    struct {
      // map of sequence number to packet count for that number
      // used to track completion of the download
      int8_t counts[MAX_NUM_PACKETS_PER_FILTER];
    } chunk_arr[MAX_NUM_CHUNKS];

    // actual received payload
    struct {
      uint64_t data_len;
      uint8_t data[CF_SIZE_BYTES];
    } buffer;

  } packet_buffer;
} download_t;

typedef struct enctr_bitmap {
  uint8_t *match_status;
} enctr_bitmap_t;

// Count packet duplication
#define dongle_download_duplication(s, d) \
  do {  \
    for (uint32_t c = 0; c < d.packet_buffer.numchunks; c++) { \
      for (int i = 0; i < (int) MAX_NUM_PACKETS_PER_FILTER; i++) { \
        uint32_t count = d.packet_buffer.chunk_arr[c].counts[i]; \
        if (count > 0) { \
          stat_add(count, s.pkt_duplication); \
        } \
      } \
    } \
  } while (0)

#define dongle_update_download_stats(s, d) \
  do {  \
    stat_add(d.packet_buffer.received, s.n_bytes); \
    stat_add(d.n_syncs_lost, s.syncs_lost); \
    dongle_download_duplication(s, d); \
    float loss_est = dongle_download_estimate_loss(&d); \
    stat_add(loss_est, s.est_pkt_loss);  \
  } while (0)

void dongle_download_init();
void dongle_download_info();
void dongle_download_complete();
void dongle_download_fail();

void dongle_on_periodic_data(uint8_t *data, uint8_t data_len, int8_t rssi);
void dongle_on_periodic_data_error(int8_t rssi);
void dongle_on_sync_lost();

int dongle_download_complete_status();

void dongle_init_bitmap(enctr_bitmap_t *enctr_bmap);
void dongle_print_bitmap_all(enctr_bitmap_t *enctr_bmap);
void dongle_reset_bitmap_all(enctr_bitmap_t *enctr_bmap);
void dongle_reset_bitmap_bit(enctr_bitmap_t *enctr_bmap, uint32_t bmap_idx,
    uint32_t bmap_off);
void dongle_reset_bitmap_byte(enctr_bitmap_t *enctr_bmap, uint32_t bmap_idx);
void dongle_set_bitmap_bit(enctr_bitmap_t *enctr_bmap, uint32_t bmap_idx,
    uint32_t bmap_off);
int dongle_has_bitmap_bit_set(enctr_bitmap_t *enctr_bmap);

#endif
