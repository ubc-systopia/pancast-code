#include "download.h"


#include "cuckoofilter-gadget/cf-gadget.h"
#include "led.h"
#include "stats.h"
#include "telemetry.h"
#include "common/src/util/log.h"
#include "common/src/util/util.h"
#include "common/src/test.h"
#include "common/src/pancast/riskinfo.h"

extern dongle_stats_t stats;
extern float dongle_hp_timer;
extern dongle_storage storage;
extern dongle_timer_t dongle_time;
float payload_start_ticks = 0, payload_end_ticks = 0;

download_t download;
cf_t cf;
uint32_t num_buckets;

#ifdef CUCKOOFILTER_FIXED_TEST
// Ephemeral IDs known to be in the test filter
static char *TEST_ID_EXIST_1 = "\x08\xb5\xec\x97\xaa\x06\xf8\x82\x27\xeb\x4e\x5a\x83\x72\x5b";
static char *TEST_ID_EXIST_2 = "\x3d\xbd\xb9\xc4\xf4\xe0\x9f\x1d\xc4\x30\x66\xda\xb8\x25\x3a";
// not in filter
static char *TEST_ID_NEXIST_1 = "blablablablabla";
static char *TEST_ID_NEXIST_2 = "tralalalalalala";
#endif


float dongle_download_esimtate_loss(download_t *d)
{
#if 1
  uint32_t max_count = d->packet_buffer.counts[0];
  for (uint32_t i = 1; i < MAX_NUM_PACKETS_PER_FILTER; i++) {
    if (d->packet_buffer.counts[i] > max_count) {
        max_count = d->packet_buffer.counts[i];
    }
  }
  return 100 * (1 - (((float) d->n_total_packets)
                / (max_count * d->packet_buffer.num_distinct)));
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
  for (int i = 0; i < actual_pkts_per_filter; i++) {
    if (d->packet_buffer.counts[i] == 0)
      num_errs++;
  }

  return ((float) num_errs/actual_pkts_per_filter) * 100;
#endif
}

void dongle_download_reset()
{
  memset(&download, 0, sizeof(download_t));
}

void dongle_download_init()
{
  dongle_download_reset();
  memset(&cf, 0, sizeof(cf_t));
  num_buckets = 0;
}

void dongle_download_start()
{
  download.is_active = 1;
  log_debugf("Download started! (chunk=%lu)\r\n",
      download.packet_buffer.chunk_num);
  stats.payloads_started++;
}

void dongle_download_fail(download_fail_reason *reason)
{
  if (download.is_active) {
    stats.payloads_failed++;
    dongle_update_download_stats(stats.all_download_stats, download);
    dongle_update_download_stats(stats.failed_download_stats, download);
    *reason = *reason + 1;
//    dongle_download_info();
    dongle_download_reset();
  }
}

int dongle_download_check_match(enctr_entry_counter_t i,
    dongle_encounter_entry_t *entry)
{
  // pad the stored id in case backend entry contains null byte at end
#define MAX_EPH_ID_SIZE 15
  uint8_t id[MAX_EPH_ID_SIZE];
  memset(id, 0x00, MAX_EPH_ID_SIZE);

  memcpy(id, &entry->eph_id, BEACON_EPH_ID_HASH_LEN);

  log_debugf("num buckets: %lu\r\n", num_buckets);
  if (lookup(id, download.packet_buffer.buffer.data, num_buckets)) {
    hexdumpen(id, MAX_EPH_ID_SIZE, " hit", entry->beacon_id,
        (uint32_t) entry->location_id, (uint16_t) i,
        (uint32_t) entry->beacon_time_start, entry->beacon_time_int,
        (uint32_t) entry->dongle_time_start, entry->dongle_time_int,
        (int8_t) entry->rssi);
    download.n_matches++;
  } else {
    hexdumpen(id, MAX_EPH_ID_SIZE, "miss", entry->beacon_id,
        (uint32_t) entry->location_id, (uint16_t) i,
        (uint32_t) entry->beacon_time_start, entry->beacon_time_int,
        (uint32_t) entry->dongle_time_start, entry->dongle_time_int,
        (int8_t) entry->rssi);
  }

#undef MAX_EPH_ID_SIZE
  return 1;
}

void dongle_on_sync_lost()
{
  log_telemf("%02x,%.0f\r\n", TELEM_TYPE_PERIODIC_SYNC_LOST, dongle_hp_timer);
  if (download.is_active) {
    log_infof("%s", "Download failed - lost sync.\r\n");
    download.n_syncs_lost++;
  }
}

void dongle_on_periodic_data_error(int8_t rssi)
{
  log_telemf("%02x,%.0f\r\n", TELEM_TYPE_PERIODIC_PKT_ERROR, dongle_hp_timer);
#ifdef MODE__STAT
  stats.num_periodic_data_error++;
  stat_add(rssi, stats.periodic_data_rssi);
  download.n_corrupt_packets++;
#endif
}

void dongle_on_periodic_data(uint8_t *data, uint8_t data_len, int8_t rssi)
{

  if (data_len < sizeof(rpi_ble_hdr)) {
    log_infof("%02x %.0f %d %d dwnld active: %d "
      "data len: %u rcvd: %u\r\n",
      TELEM_TYPE_PERIODIC_PKT_DATA, dongle_hp_timer, rssi, data_len,
      download.is_active, download.packet_buffer.chunk_num,
      (uint32_t) download.packet_buffer.buffer.data_len,
      download.packet_buffer.received);

    download.n_corrupt_packets++;
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

  stat_add(data_len, stats.periodic_data_size);
  stat_add(rssi, stats.periodic_data_rssi);

  log_debugf("%02x %.0f %d %d dwnld active: %d pktseq: %u "
      "chunkid: %u, chunknum: %u chunklen: %u data len: %u rcvd: %u\r\n",
      TELEM_TYPE_PERIODIC_PKT_DATA, dongle_hp_timer, rssi, data_len,
      download.is_active, rbh->pkt_seq, rbh->chunkid,
      download.packet_buffer.chunk_num, (uint32_t) rbh->chunklen,
      (uint32_t) download.packet_buffer.buffer.data_len,
      download.packet_buffer.received);

  if (download.is_active) {
    if (rbh->chunkid != download.packet_buffer.chunk_num) {
      log_errorf("forced chunk switch, prev: %u new: %u\r\n",
          download.packet_buffer.chunk_num, rbh->chunkid);
      dongle_download_fail(&stats.switch_chunk);
      download.packet_buffer.chunk_num = rbh->chunkid;
    }

    if (rbh->chunklen != download.packet_buffer.buffer.data_len) {
      log_errorf("chunk len mismatch, prev: %u new: %u\r\n",
          download.packet_buffer.buffer.data_len, rbh->chunklen);
    }
  }

  if (rbh->pkt_seq >= MAX_NUM_PACKETS_PER_FILTER || (int64_t) rbh->chunklen < 0) {
    log_errorf("seq#: %d, max pkts: %d, chunklen: %d\r\n",
        rbh->pkt_seq, MAX_NUM_PACKETS_PER_FILTER, rbh->chunklen);
    return;
  }

  if (!download.is_active) {
    dongle_download_start();
  }
  download.packet_buffer.buffer.data_len = rbh->chunklen;

  download.n_total_packets++;
  int prev = download.packet_buffer.counts[rbh->pkt_seq];
  download.packet_buffer.counts[rbh->pkt_seq]++;
  // duplicate packet
  if (prev > 0)
    return;

  // this is an unseen packet
  download.packet_buffer.num_distinct++;
  uint8_t len = data_len - sizeof(rpi_ble_hdr);
  memcpy(download.packet_buffer.buffer.data + (rbh->pkt_seq*MAX_PAYLOAD_SIZE),
    data + sizeof(rpi_ble_hdr), len);
  download.packet_buffer.received += len;
  if (download.packet_buffer.received >= download.packet_buffer.buffer.data_len) {
    // there may be extra data in the packet
    dongle_download_complete();
  }
}

void dongle_download_complete()
{
  log_infof("[%u] Download complete! last dnwld time: %u "
      "data len: %d curr dwnld lat: %f\r\n",
      dongle_time, stats.last_download_time, download.packet_buffer.buffer.data_len,
      (double) (dongle_hp_timer - payload_start_ticks));
  int actual_pkts_per_filter = ((TEST_FILTER_LEN-1)/MAX_PAYLOAD_SIZE)+1;
  for (int i = 0; i < actual_pkts_per_filter; i++) {
    if (download.packet_buffer.counts[i] > 0)
      continue;

    log_errorf("[%d] chunkid: %d count: %d #distinct: %d total: %d\r\n",
        i, download.packet_buffer.chunk_num, download.packet_buffer.counts[i],
        download.packet_buffer.num_distinct, download.n_total_packets);
  }

  // now we know the payload is the correct size

  dongle_update_download_time();

  num_buckets = cf_gadget_num_buckets(download.packet_buffer.buffer.data_len);

  if (num_buckets == 0) {
    log_debugf("%s", "num buckets is 0!!!\r\n");
    dongle_download_fail(&stats.cuckoo_fail);
    return;
  }

  payload_end_ticks = dongle_hp_timer;

  // check the content using cuckoofilter decoder

//  bitdump(download.packet_buffer.buffer.data,
//      download.packet_buffer.buffer.data_len, "risk chunk");

#ifdef CUCKOOFILTER_FIXED_TEST

  uint8_t *filter = download.packet_buffer.buf;

  int status = 0;

  // these are the test cases for the fixed test filter
  // these should exist
  if (!lookup(TEST_ID_EXIST_1, filter, num_buckets)) {
    log_errorf("Cuckoofilter test failed: %s should exist\r\n",
               TEST_ID_EXIST_1);
    status += 1;
  }
  if (!lookup(TEST_ID_EXIST_2, filter, num_buckets)) {
    log_errorf("Cuckoofilter test failed: %s should exist\r\n",
               TEST_ID_EXIST_2);
    status += 1;
  }

  // these shouldn't
  if (lookup(TEST_ID_NEXIST_1, filter, num_buckets)) {
    log_errorf("Cuckoofilter test failed: %s should NOT exist\r\n",
               TEST_ID_NEXIST_1);
    status += 1;
  }
  if (lookup(TEST_ID_NEXIST_2, filter, num_buckets)) {
    log_errorf("Cuckoofilter test failed: %s should NOT exist\r\n",
               TEST_ID_NEXIST_2);
    status += 1;
  }


  if (!status) {
    log_infof("Cuckoofilter test passed\r\n");
  }

#else

  // check existing log entries against the new filter
  dongle_storage_load_encounter(&storage, storage.encounters.tail,
      dongle_download_check_match);

#endif /* CUCKOOFILTER_FIXED_TEST */

  if (download.n_matches > 0) {
    dongle_led_notify();
  }

  /*
   * XXX: increment global stats, not assignment
   */
  stats.total_matches = download.n_matches;
  stats.payloads_complete++;
  // compute latency
  double lat = (double) (payload_end_ticks - payload_start_ticks);
  dongle_update_download_stats(stats.all_download_stats, download);
  dongle_update_download_stats(stats.completed_download_stats, download);
  stat_add(lat, stats.completed_periodic_data_avg_payload_lat);

  dongle_download_reset();
}

void dongle_download_info()
{
  log_infof("Total time: %.0f ms\r\n", download.time);
  log_infof("Packets Received: %lu\r\n", download.n_total_packets);
  log_infof("Data bytes downloaded: %lu\r\n", download.packet_buffer.received);
}
