#include "download.h"


#include "cuckoofilter-gadget/cf-gadget.h"
#include "stats.h"
#include "telemetry.h"
#include "common/src/util/log.h"
#include "common/src/util/util.h"
#include "common/src/test.h"
#include "common/src/pancast/riskinfo.h"

extern dongle_stats_t stats;
extern downloads_stats_t download_stats;
extern float dongle_hp_timer;
extern dongle_storage storage;

download_t download;
cf_t cf;
uint32_t num_buckets;


float dongle_download_esimtate_loss(download_t *d)
{
#if 1
  uint32_t max_count = d->packet_buffer.counts[0];
  for (int i = 1; i < MAX_NUM_PACKETS_PER_FILTER; i++) {
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
  int actual_pkts_per_filter = ((TEST_FILTER_LEN-1)/MAX_PACKET_SIZE)+1;
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
  download_stats.payloads_started++;
}

void dongle_download_fail(download_fail_reason *reason)
{
  if (download.is_active) {
    download_stats.payloads_failed++;
    dongle_update_download_stats(download_stats.all_download_stats, download);
    dongle_update_download_stats(download_stats.failed_download_stats, download);
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

#undef MAX_EPH_ID_SIZE

  log_infof("num buckets: %lu\r\n", num_buckets);
 // hexdumpn(&download.packet_buffer.buffer.data, 1728, "filter", 0, 0, 0, 0, 0);
  // num_buckets = 4; // for testing (num_buckets cannot be 0)
  if (lookup(id, download.packet_buffer.buffer.data, num_buckets)) {
    log_infof("====== LOG MATCH [%d]!!! ====== \r\n", i);
    download.n_matches++;
  } else {
    log_infof("%s", "No match for id\r\n");
//    hexdumpn(id, BEACON_EPH_ID_HASH_LEN, "checking id");
  }
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

//    log_bytes(printf, printf, data, data_len, "data");
  if (data_len < PACKET_HEADER_LEN) {
    log_telemf("%02x,%.0f,%d,%d\r\n", TELEM_TYPE_PERIODIC_PKT_DATA,
               dongle_hp_timer, rssi, data_len);
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

  stats.total_periodic_data_size += data_len;
  stats.num_periodic_data++;
  stat_add(data_len, stats.periodic_data_size);
  stat_add(rssi, stats.periodic_data_rssi);

  log_infof("%02x %.0f %d %d dwnld active: %d pktseq: %u "
      "chunkid: %u, chunknum: %u chunklen: %u data len: %u\r\n",
      TELEM_TYPE_PERIODIC_PKT_DATA, dongle_hp_timer, rssi, data_len,
      download.is_active, rbh->pkt_seq, rbh->chunkid,
      download.packet_buffer.chunk_num, (uint32_t) rbh->chunklen,
      (uint32_t) download.packet_buffer.buffer.data_len);
  if (download.is_active &&
      rbh->chunkid != download.packet_buffer.chunk_num) {
    // forced to switch chunks

    dongle_download_fail(&download_stats.switch_chunk);

    log_debugf("Downloading chunk %lu\r\n", rbh->chunkid);
    download.packet_buffer.chunk_num = rbh->chunkid;
  } else if (download.is_active
      && rbh->chunklen != download.packet_buffer.buffer.data_len) {
    log_errorf("Chunk length mismatch: previous: %lu, new: %lu\r\n",
               download.packet_buffer.buffer.data_len, rbh->chunklen);
  }

  if (rbh->pkt_seq >= MAX_NUM_PACKETS_PER_FILTER) {
    log_errorf("Error: sequence number %d out of bounds %d\r\n",
        rbh->pkt_seq, MAX_NUM_PACKETS_PER_FILTER);
    return;
  }

  if (!download.is_active) {
    dongle_download_start();
    download.packet_buffer.buffer.data_len = rbh->chunklen;
  }

  download.n_total_packets++;
  int prev = download.packet_buffer.counts[rbh->pkt_seq];
  download.packet_buffer.counts[rbh->pkt_seq]++;
  // duplicate packet
  if (prev > 0)
    return;

  // this is an unseen packet
  download.packet_buffer.num_distinct++;
  uint8_t len = data_len - PACKET_HEADER_LEN;
  memcpy(
    download.packet_buffer.buffer.data + (rbh->pkt_seq*MAX_PACKET_SIZE),
    data + PACKET_HEADER_LEN, len);
  download.packet_buffer.received += len;
  log_debugf("download progress: %.2f\r\n",
    ((float) download.packet_buffer.received
     / download.packet_buffer.buffer.data_len) * 100);
  if (download.packet_buffer.received >= download.packet_buffer.buffer.data_len) {
    // there may be extra data in the packet
    dongle_download_complete();
  }
}

void dongle_download_complete()
{
  log_infof("%s", "Download complete!\r\n");
  int is_loss = 0;
  int actual_pkts_per_filter = ((TEST_FILTER_LEN-1)/MAX_PACKET_SIZE)+1;
  for (int i = 0; i < actual_pkts_per_filter; i++) {
    if (download.packet_buffer.counts[i] <= 0)
      is_loss = 1;
  }

  if (is_loss) {
    log_debugf("#distinct: %d, #total: %d, count/pkts: ",
        download.packet_buffer.num_distinct, download.n_total_packets);
    for (int i = 0; i < actual_pkts_per_filter; i++) {
      if (download.packet_buffer.counts[i] <= 0)
        log_debugf("%d ", i);
      //printf("%d ", download.packet_buffer.counts[i]);
    }
    log_debugf("%s", "\r\n");
  }

  download_stats.payloads_complete++;
  // compute latency
  double lat = download.time;
  dongle_update_download_stats(download_stats.all_download_stats, download);
  dongle_update_download_stats(download_stats.complete_download_stats.download_stats, download);
  stat_add(lat, download_stats.complete_download_stats.periodic_data_avg_payload_lat);

  // check the content using cuckoofilter decoder

  uint64_t filter_len = download.packet_buffer.buffer.data_len;

  if (filter_len > download.packet_buffer.received) {
    log_errorf("%s", "Filter length mismatch (%lu/%lu)\r\n",
        download.packet_buffer.received, filter_len);
    dongle_download_fail(&download_stats.cuckoo_fail);
    return;
  }

  if (is_loss) {
    log_debugf("len: %lu, ", filter_len);
    log_debugf("chunk: %lu, sz(u64): %u\r\n",
        download.packet_buffer.chunk_num, sizeof(uint64_t));
  }
  // now we know the payload is the correct size

  if (filter_len > 0) {
    dongle_update_download_time();
  }

  num_buckets = cf_gadget_num_buckets(filter_len);

  if (num_buckets == 0) {
    log_debugf("%s", "num buckets is 0!!!\r\n");
    dongle_download_fail(&download_stats.cuckoo_fail);
    return;
  }

#ifdef CUCKOOFILTER_FIXED_TEST

  uint8_t *filter = download.packet_buffer.buf;

  //hexdump(filter, filter_len + 8);

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
//  dongle_storage_load_all_encounter(&storage, dongle_download_check_match);
#endif /* CUCKOOFILTER_FIXED_TEST */

  if (download.n_matches > 0) {
    dongle_led_notify();
  }
  download_stats.total_matches = download.n_matches;

  dongle_download_reset();
}

void dongle_download_info()
{
  log_infof("Total time: %.0f ms\r\n", download.time);
  log_infof("Packets Received: %lu\r\n", download.n_total_packets);
  log_infof("Data bytes downloaded: %lu\r\n", download.packet_buffer.received);
}
