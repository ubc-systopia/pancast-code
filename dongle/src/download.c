#include "download.h"


#include "cuckoofilter-gadget/cf-gadget.h"
#include "led.h"
#include "stats.h"
#include "nvm3_lib.h"
#include "telemetry.h"
#include "test.h"
#include "common/src/util/log.h"
#include "common/src/util/util.h"
#include "common/src/test.h"
#include "common/src/riskinfo.h"

extern dongle_stats_t *stats;
extern float dongle_hp_timer;
extern dongle_config_t config;
extern dongle_timer_t dongle_time;
float payload_start_ticks = 0, payload_end_ticks = 0;
extern dongle_timer_t last_download_start_time;
extern enctr_bitmap_t enctr_bmap;

//int32_t prev_chunkid = -1;

download_t *download;
cf_t cf;

float dongle_download_estimate_loss(download_t *d)
{
#if 1
  if (!d)
    return 0;

  if (d->n_total_packets == 0 && d->packet_buffer.num_distinct == 0)
    return 0;

  int8_t max_count = d->packet_buffer.chunk_arr[0].counts[0];
  for (uint32_t c = 0; c < d->packet_buffer.numchunks; c++) {
    for (uint32_t i = 1; i < MAX_NUM_PACKETS_PER_FILTER; i++) {
      if (d->packet_buffer.chunk_arr[c].counts[i] > max_count) {
          max_count = d->packet_buffer.chunk_arr[c].counts[i];
      }
    }
  }
  float val = 100 * (1 - (((float) d->n_total_packets) /
              (max_count * d->packet_buffer.num_distinct)));
  log_expf("DWNLD EST LOSS #total pkts: %d max cnt: %d #distinct: %d "
      "est: %.02f\r\n", d->n_total_packets, max_count,
      d->packet_buffer.num_distinct, val);
  return val;
#else
  /*
   * The transmitter's payload update does not align perfectly with
   * the BLE's periodic interval at the lower layer. Consequently, we
   * do not receive each packet exactly once from the transmitter.
   * However, as long as we receive each packet at least once, there
   * will be no loss for the application. Therefore, count a loss
   * only if some packet is never received
   */
  int num_errs = 0;
  int actual_pkts_per_filter = ((TEST_FILTER_LEN-1)/MAX_PAYLOAD_SIZE)+1;
  for (uint32_t c = 0; c < d->packet_buffer.numchunks; c++) {
    for (int i = 0; i < actual_pkts_per_filter; i++) {
      if (d->packet_buffer.chunk_arr[c].counts[i] == 0)
        num_errs++;
    }
  }

  return ((float) num_errs/actual_pkts_per_filter) * 100;
#endif
}

static inline void dongle_download_reset()
{
  memset(download, 0, sizeof(download_t));
}

void dongle_download_init()
{
  dongle_download_reset();
  memset(&cf, 0, sizeof(cf_t));
}

void dongle_download_start()
{
  download->is_active = 1;
#if MODE__STAT
  stats->stat_ints.payloads_started++;
#endif
}

void dongle_download_fail(download_fail_reason *reason __attribute__((unused)))
{
  if (download->is_active) {
#if MODE__STAT
    stats->stat_ints.payloads_failed++;
    dongle_update_download_stats(stats->all_download_stats, download);
    dongle_update_download_stats(stats->failed_download_stats, download);
    *reason = *reason + 1;
#endif

//    dongle_download_info();
    dongle_download_reset();
  }
}

uint32_t debug_chunkid = 0;

int dongle_download_check_match(
    enctr_entry_counter_t i __attribute__((unused)),
    dongle_encounter_entry_t *entry, uint32_t num_buckets)
{
  // pad the stored id in case backend entry contains null byte at end
#define MAX_EPH_ID_SIZE 15
  uint8_t id[MAX_EPH_ID_SIZE];
  memset(id, 0x00, MAX_EPH_ID_SIZE);

  memcpy(id, &entry->eph_id, BEACON_EPH_ID_HASH_LEN);
//  char dbuf[64];

  uint64_t idx1 = 0, idx2 = 0;
  int res1 = 0, res2 = 0;
  uint32_t fpp = 0;

  if (lookup(id, download->packet_buffer.buffer.data, num_buckets,
        &idx1, &idx2, &fpp, &res1, &res2)) {
#if 0
    memset(dbuf, 0, 64);
    sprintf(dbuf, "hit %02lu %02x %02x %0x %0x 0x%08lx", debug_chunkid,
        (int) idx1, (int) idx2, res1, res2, fpp);
    hexdumpen(id, MAX_EPH_ID_SIZE, dbuf, entry->beacon_id,
        (uint32_t) entry->location_id, (uint16_t) i,
        (uint32_t) entry->beacon_time_start, entry->beacon_time_int,
        (uint32_t) entry->dongle_time_start, entry->dongle_time_int,
        (int8_t) entry->rssi, (uint32_t) ENCOUNTER_LOG_OFFSET(i));
#endif
    download->n_matches++;

    uint32_t bmap_idx = 0, bmap_off = 0;
    ENCOUNTER_BITMAP_OFFSET(i, &bmap_idx, &bmap_off);
    dongle_set_bitmap_bit(&enctr_bmap, bmap_idx, bmap_off);

  } else {
#if 0
    memset(dbuf, 0, 64);
    sprintf(dbuf, "miss %02lu %02x %02x %0x %0x 0x%08lx", debug_chunkid,
        (int) idx1, (int) idx2, res1, res2, fpp);
    hexdumpen(id, MAX_EPH_ID_SIZE, dbuf, entry->beacon_id,
        (uint32_t) entry->location_id, (uint16_t) i,
        (uint32_t) entry->beacon_time_start, entry->beacon_time_int,
        (uint32_t) entry->dongle_time_start, entry->dongle_time_int,
        (int8_t) entry->rssi, (uint32_t) ENCOUNTER_LOG_OFFSET(i));
#endif
  }

#undef MAX_EPH_ID_SIZE
  return 1;
}

void dongle_init_bitmap(enctr_bitmap_t *enctr_bmap)
{
  if (!enctr_bmap)
    return;

  enctr_bmap->match_status =
    malloc(sizeof(uint8_t) * (MAX_LOG_COUNT/BITS_PER_BYTE));

  memset(enctr_bmap->match_status, 0,
      sizeof(uint8_t) * (MAX_LOG_COUNT/BITS_PER_BYTE));
}

void dongle_print_bitmap_all(enctr_bitmap_t *enctr_bmap)
{
  if (!enctr_bmap || !enctr_bmap->match_status)
    return;

  int multiple = 16;
  for (unsigned int i = 0; i < NUM_BYTES_LOG_BITMAP; i++) {
    if (i / multiple > 0 && i % multiple == 0) {
      printf("\r\n");
    }

    printf("%02x ", enctr_bmap->match_status[i]);
  }
  int count = dongle_count_bitmap_bit_set(enctr_bmap);
  printf("#match: %d %ld", count, download->n_matches);
  printf("\r\n");
}

void dongle_reset_bitmap_all(enctr_bitmap_t *enctr_bmap)
{
  if (!enctr_bmap || !enctr_bmap->match_status)
    return;

  memset(enctr_bmap->match_status, 0,
      sizeof(uint8_t) * (MAX_LOG_COUNT/BITS_PER_BYTE));
}

void dongle_reset_bitmap_bit(enctr_bitmap_t *enctr_bmap, uint32_t bmap_idx,
    uint32_t bmap_off)
{
  if (!enctr_bmap || !enctr_bmap->match_status)
    return;

  uint8_t val = enctr_bmap->match_status[bmap_idx];
  uint8_t mask = 0xff ^ (1 << bmap_off);
  val = val & mask;
  enctr_bmap->match_status[bmap_idx] = val;
}

void dongle_reset_bitmap_byte(enctr_bitmap_t *enctr_bmap, uint32_t bmap_idx)
{
  if (!enctr_bmap || !enctr_bmap->match_status)
    return;

  enctr_bmap->match_status[bmap_idx] = 0;
}

void dongle_set_bitmap_bit(enctr_bitmap_t *enctr_bmap, uint32_t bmap_idx,
    uint32_t bmap_off)
{
  if (!enctr_bmap || !enctr_bmap->match_status)
    return;

  uint8_t val = enctr_bmap->match_status[bmap_idx];
  val = val | (1 << bmap_off);
  enctr_bmap->match_status[bmap_idx] = val;
}

int dongle_has_bitmap_bit_set(enctr_bitmap_t *enctr_bmap)
{
  if (!enctr_bmap || !enctr_bmap->match_status)
    return -1;

  for (unsigned int i = 0; i < NUM_BYTES_LOG_BITMAP; i++) {
    if (enctr_bmap->match_status[i] != 0)
      return 1;
  }

  return 0;
}

/*
 * adapted from:
 * https://en.wikipedia.org/wiki/Hamming_weight#Efficient_implementation
 */
int dongle_count_bitmap_bit_set(enctr_bitmap_t *enctr_bmap)
{
  if (!enctr_bmap || !enctr_bmap->match_status)
    return -1;

  int count = 0;
  for (unsigned int i = 0; i < NUM_BYTES_LOG_BITMAP; i++) {
    if (enctr_bmap->match_status[i] == 0)
      continue;

    int a = enctr_bmap->match_status[i];
    int b0 = (a >> 0) & 0x55;
    int b1 = (a >> 1) & 0x55;
    int c = b0 + b1;
    int d0 = (c >> 0) & 0x33;
    int d2 = (c >> 2) & 0x33;
    int e = d0 + d2;
    int f0 = (e >> 0) & 0x0f;
    int f4 = (e >> 4) & 0x0f;
    int g = f0 + f4;
    count += g;
    log_infof("[%d] in: %d 0x%02x #ones: %d\r\n", i, a, a, g);
  }

  return count;
}

void dongle_on_sync_lost()
{
  if (download->is_active) {
    log_infof("%s", "Download failed - lost sync.\r\n");
    download->n_syncs_lost++;
  }
}

void dongle_on_periodic_data_error(int8_t rssi __attribute__((unused)))
{
#if MODE__STAT
  stats->stat_ints.num_periodic_data_error++;
  stat_add(rssi, stats->stat_grp.periodic_data_rssi);
  download->n_corrupt_packets++;
#endif
}

static int download_one_chunk_complete(download_t *download, uint32_t chunkid)
{
  if (!download)
    return -1;

  for (uint32_t j = 0; j < MAX_NUM_PACKETS_PER_FILTER; j++) {
    if (download->packet_buffer.chunk_arr[chunkid].counts[j] <= 0)
      return 0;
  }

  return 1;
}

#if 0
int download_prev_chunk_complete(download_t *download, uint32_t chunkid)
{
  if (!download)
    return -1;

  for (uint32_t j = 0; j < MAX_NUM_PACKETS_PER_FILTER; j++) {
    if (download->packet_buffer.chunk_arr[chunkid].counts[j] == 0)
      return 0;
  }

  return 1;
}

int download_all_chunks_complete_2(download_t *download)
{
  if (!download)
    return -1;

  int8_t *status = download->packet_buffer.chunk_complete;
  int nchunks = download->packet_buffer.numchunks;
  int nbytes = (nchunks-1)/BITS_PER_BYTE + 1;
  int nbyte_off = nchunks % BITS_PER_BYTE;
  int i = 0, j = 0;

  for (i = 0; i < nbytes - 1; i++) {
    if (status[i] != (int8_t) 0xff) {
      log_infof("%d inc[%d]: 0x%02x\r\n", prev_chunkid, i, status[i]);
      return 0;
    }
  }

  for (j = 0; j < nbyte_off; j++) {
    if ((status[i] & (1 << j)) != 1) {
      log_expf("%d inc[%d][%d]: 0x%02x\r\n", prev_chunkid, i, j, status[i]);
      return 0;
    }
  }

  return 1;
}
#endif

static int download_all_chunks_complete(download_t *download)
{
  if (!download)
    return -1;

  for (uint32_t i = 0; i < download->packet_buffer.numchunks; i++) {
    if (download_one_chunk_complete(download, i) == 0)
      return 0;
  }

#if 0
  for (uint32_t i = 0; i < download->packet_buffer.numchunks; i++) {
    log_expf("[%d] 0x%08lx 0x%08lx\r\n", i,
        *((uint32_t *) &download->packet_buffer.chunk_arr[i].counts[0]),
        *((uint32_t *) &download->packet_buffer.chunk_arr[i].counts[3]));
  }
#endif

  return 1;
}

void dongle_on_periodic_data(uint8_t *data, uint8_t data_len, int8_t rssi __attribute__((unused)))
{

  if (data_len < sizeof(rpi_ble_hdr)) {
#if 0
    log_debugf("%02x %.0f %d %d dwnld active: %d "
      "data len: %u rcvd: %u\r\n",
      TELEM_TYPE_PERIODIC_PKT_DATA, dongle_hp_timer, rssi, data_len,
      download->is_active, download->packet_buffer.cur_chunkid,
      (uint32_t) download->packet_buffer.buffer.data_len,
      download->packet_buffer.received);
#endif
    if (data_len > 0)
      download->n_corrupt_packets++;

    return;
  }

  /*
   * XXX: something is wrong with directly reading from data,
   * and we are forced to copy the data into a local buffer first
   * and print from it
   */
  char buf[PER_ADV_SIZE];
  memset(buf, 0, PER_ADV_SIZE);
  memcpy(buf, data, data_len);
  // extract seq number, chunk number, and chunk len
  rpi_ble_hdr *rbh = (rpi_ble_hdr *) buf;

#if MODE__STAT
  stat_add(data_len, stats->stat_grp.periodic_data_size);
  stat_add(rssi, stats->stat_grp.periodic_data_rssi);
#endif

  if (rbh->pkt_seq >= MAX_NUM_PACKETS_PER_FILTER ||
      (int32_t) rbh->chunklen < 0) {
    log_errorf("seq#: %d, max pkts: %d, chunklen: %d\r\n",
        rbh->pkt_seq, MAX_NUM_PACKETS_PER_FILTER, rbh->chunklen);
    return;
  }

  if (!download->is_active) {
    dongle_download_start();
  }

  uint32_t num_buckets = 0;

#if 0
  /*
   * new chunk detected, before downloading it, check if previous
   * chunk was fully downloaded. if so, run CF lookups.
   */
  if (prev_chunkid != -1 && prev_chunkid != (int32_t) rbh->chunkid) {
    int pb_idx = prev_chunkid / BITS_PER_BYTE;
    int pb_off = prev_chunkid % BITS_PER_BYTE;
    int pstatus = (download->packet_buffer.chunk_complete[pb_idx] &
      (1 << pb_off));
    for (unsigned int i = 0; i < MAX_NUM_PACKETS_PER_FILTER; i++) {
      download->packet_buffer.chunk_arr[prev_chunkid].counts[i] +=
        download->packet_buffer.chunk_prev_counts[i];
    }

    if (download_prev_chunk_complete(download, prev_chunkid) == 1) {
      log_expf("pc: %ld %d p#: 0x%08lx 0x%08lx n#: 0x%08lx 0x%08lx\r\n",
        prev_chunkid, (pstatus != 0),
        *((int32_t *) &download->packet_buffer.chunk_arr[prev_chunkid].counts[0]),
        *((int32_t *) &download->packet_buffer.chunk_arr[prev_chunkid].counts[4]),
        *((int32_t *) &download->packet_buffer.chunk_prev_counts[0]),
        *((int32_t *) &download->packet_buffer.chunk_prev_counts[3])
        );
//      debug_chunkid = rbh->chunkid;
      num_buckets =
        cf_gadget_num_buckets(download->packet_buffer.buffer.data_len);

      if (num_buckets == 0) {
        dongle_download_fail(&stats->stat_ints.cuckoo_fail);
      } else {
        // check existing log entries against the new filter
        dongle_storage_load_encounter(config.en_tail,
          num_encounters_current(config.en_head, config.en_tail),
          dongle_download_check_match, num_buckets);
      }

      int byte_idx = prev_chunkid / BITS_PER_BYTE;
      int byte_off = prev_chunkid % BITS_PER_BYTE;
      download->packet_buffer.chunk_complete[byte_idx] |= (1 << byte_off);
      memset(download->packet_buffer.chunk_prev_counts, 0,
          sizeof(uint8_t) * MAX_NUM_PACKETS_PER_FILTER);

      memset(download->packet_buffer.buffer.data, 0, CF_SIZE_BYTES);
      download->packet_buffer.buffer.data_len = 0;
      memset(&cf, 0, sizeof(cf_t));
    }
  }

  prev_chunkid = rbh->chunkid;
#endif

  download->packet_buffer.cur_chunkid = rbh->chunkid;
  download->packet_buffer.buffer.data_len = rbh->chunklen;
  download->packet_buffer.numchunks = rbh->numchunks;

  download->n_total_packets++;
  int prev = download->packet_buffer.chunk_arr[rbh->chunkid].counts[rbh->pkt_seq];
  download->packet_buffer.chunk_arr[rbh->chunkid].counts[rbh->pkt_seq]++;
//  download->packet_buffer.chunk_prev_counts[rbh->pkt_seq]++;

  // duplicate packet
  if (prev > 0)
    return;

  // this is an unseen packet
  download->packet_buffer.num_distinct++;
  uint8_t len = data_len - sizeof(rpi_ble_hdr);
  memcpy(download->packet_buffer.buffer.data + (rbh->pkt_seq*MAX_PAYLOAD_SIZE),
    data + sizeof(rpi_ble_hdr), len);
  download->packet_buffer.received += len;

#if 0
  log_expf("%.0f %d %d active: %d chunkid: [%u/%u]/%u, pkt: %u "
      "chunklen: %u/%u rcvd: %u\r\n",
      dongle_hp_timer, rssi, data_len, download->is_active,
      rbh->chunkid, download->packet_buffer.cur_chunkid, rbh->numchunks,
      rbh->pkt_seq, (uint32_t) rbh->chunklen,
      (uint32_t) download->packet_buffer.buffer.data_len,
      download->packet_buffer.received
      );
#endif

#if 1
  if (download_one_chunk_complete(download, rbh->chunkid)) {
    // check the content using cuckoofilter decoder

    //  bitdump(download->packet_buffer.buffer.data,
    //      download->packet_buffer.buffer.data_len, "risk chunk");

    debug_chunkid = rbh->chunkid;
    num_buckets =
      cf_gadget_num_buckets(download->packet_buffer.buffer.data_len);

    if (num_buckets == 0) {
      dongle_download_fail(&stats->stat_ints.cuckoo_fail);
      return;
    }

#ifdef CUCKOOFILTER_FIXED_TEST
    run_fixed_cf_test(download, num_buckets);
#else

    // check existing log entries against the new filter
    dongle_storage_load_encounter(config.en_tail,
        num_encounters_current(config.en_head, config.en_tail),
        dongle_download_check_match, num_buckets);

#endif /* CUCKOOFILTER_FIXED_TEST */

    memset(download->packet_buffer.buffer.data, 0, CF_SIZE_BYTES);
    download->packet_buffer.buffer.data_len = 0;
    memset(&cf, 0, sizeof(cf_t));
  }
#endif

  // TODO: replace with download_all_chunks_complete_2
  if (download_all_chunks_complete(download)) {

    dongle_print_bitmap_all(&enctr_bmap);
    nvm3_save_enctr_bmap(&enctr_bmap);

    if (dongle_has_bitmap_bit_set(&enctr_bmap)) {
      dongle_led_notify();
    }
    // there may be extra data in the packet
    dongle_download_complete();
  }
}

void dongle_download_complete()
{
  // now we know the payload is the correct size

  stats->stat_ints.last_download_end_time = dongle_time;

  payload_end_ticks = dongle_hp_timer;

  log_expf("[%u] Download complete! last dnwld time: %lu "
      "data len: %lu curr dwnld lat: [%.02f, %.02f - %.02f]: %.02f\r\n",
      dongle_time, stats->stat_ints.last_download_end_time,
      (uint32_t) download->packet_buffer.buffer.data_len,
      payload_start_ticks, dongle_hp_timer,
      payload_end_ticks,
      (double) (dongle_hp_timer - payload_start_ticks));

  int actual_pkts_per_filter = ((TEST_FILTER_LEN-1)/MAX_PAYLOAD_SIZE)+1;
  for (uint32_t c = 0; c < download->packet_buffer.numchunks; c++) {
    for (int i = 0; i < actual_pkts_per_filter; i++) {

      if (download->packet_buffer.chunk_arr[c].counts[i] > 0)
        continue;

      log_errorf("[%d:%d] count: %d #distinct: %d total: %d\r\n",
          c, i, download->packet_buffer.chunk_arr[c].counts[i],
          download->packet_buffer.num_distinct, download->n_total_packets);
    }
  }

#if MODE__STAT
  /*
   * XXX: increment global stats, not assignment
   */
  stats->stat_ints.total_matches = download->n_matches;
  stats->stat_ints.payloads_complete++;

  // compute latency
  double lat = (double) (payload_end_ticks - payload_start_ticks);
  dongle_update_download_stats(stats->all_download_stats, download);
  dongle_update_download_stats(stats->completed_download_stats, download);
  stat_add(lat, stats->stat_grp.completed_periodic_data_avg_payload_lat);
  nvm3_save_stat(stats);
#endif

  dongle_download_reset();
}

int dongle_download_complete_status()
{
#if 0
  if (dongle_time - stats->stat_ints.last_download_time >= RETRY_DOWNLOAD_INTERVAL ||
		  stats->stat_ints.last_download_time == 0) {
    return 0;
  }
  return 1;
#endif

  // successfully downloaded risk payload for the one download interval
  if (stats->stat_ints.last_download_end_time >=
      last_download_start_time) {
    return 1;
  }

  return 0;
}

void dongle_download_info()
{
  log_infof("Total time: %.0f ms\r\n", download->time);
  log_infof("Packets Received: %lu\r\n", download->n_total_packets);
  log_infof("Data bytes downloaded: %lu\r\n", download->packet_buffer.received);
}
