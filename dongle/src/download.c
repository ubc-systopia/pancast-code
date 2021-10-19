#include "download.h"


#include "cuckoofilter-gadget/cf-gadget.h"
#include "stats.h"
#include "telemetry.h"
#include "common/src/util/log.h"
#include "common/src/util/util.h"

extern dongle_stats_t stats;
extern downloads_stats_t download_stats;
extern float dongle_hp_timer;
extern dongle_storage storage;

download_t download;
cf_t cf;
uint32_t num_buckets;


float dongle_download_esimtate_loss(download_t *d)
{
  uint32_t max_count = d->packet_buffer.counts[0];
  for (int i = 1; i < MAX_NUM_PACKETS_PER_FILTER; i++) {
    if (d->packet_buffer.counts[i] > max_count) {
        max_count = d->packet_buffer.counts[i];
    }
  }
  return 100 * (1 - (((float) d->n_total_packets)
                / (max_count * d->packet_buffer.num_distinct)));
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
  log_infof("%s", "Download started! (chunk=%lu)\r\n",
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
                                dongle_encounter_entry *entry)
{
  // pad the stored id in case backend entry contains null byte at end
#define MAX_EPH_ID_SIZE 15
  uint8_t id[MAX_EPH_ID_SIZE];
  memset(id, 0x00, MAX_EPH_ID_SIZE);
#undef MAX_EPH_ID_SIZE

  memcpy(id, &entry->eph_id, BEACON_EPH_ID_HASH_LEN);

  hexdumpn(id, 15, "checking id");

  log_debugf("num buckets: %lu\r\n", num_buckets);
  //hexdumpn(&download.packet_buffer.buffer, 1736, "filter");
  // num_buckets = 4; // for testing (num_buckets cannot be 0)
  if (lookup(id, &download.packet_buffer.buffer, num_buckets)) {
    log_infof("%s", "====== LOG MATCH!!! ====== \r\n");
  } else {
    log_infof("%s", "No match for id\r\n");
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
//        log_errorf("%s", "not enough data to read sequence numbers\r\n");
//        log_errorf("%s", "len: %d\r\n", data_len);
    download.n_corrupt_packets++;
    return;
  }

  // extract sequence numbers
  uint32_t seq, chunk, chunk_len;
  memcpy(&seq, data, sizeof(uint32_t));
  memcpy(&chunk, data + sizeof(uint32_t), sizeof(uint32_t));
  // TODO: this is actually and 8 byte field so fix
  memcpy(&chunk_len, data + 2*sizeof(uint32_t), 8);

  log_telemf("%02x,%.0f,%d,%d,%lu,%lu,%lu\r\n", TELEM_TYPE_PERIODIC_PKT_DATA,
                     dongle_hp_timer, rssi, data_len, seq, chunk, chunk_len);

  stats.total_periodic_data_size += data_len;
  stats.num_periodic_data++;
  stat_add(data_len, stats.periodic_data_size);
  stat_add(rssi, stats.periodic_data_rssi);

  if (download.is_active && chunk != download.packet_buffer.chunk_num) {
    // forced to switch chunks

    dongle_download_fail(&download_stats.switch_chunk);

    log_debugf("Downloading chunk %lu\r\n", chunk);
    download.packet_buffer.chunk_num = chunk;
  } else if (download.is_active
      && chunk_len != download.packet_buffer.buffer.data_len) {
    log_errorf("Chunk length mismatch: previous: %lu, new: %lu\r\n",
               download.packet_buffer.buffer.data_len, chunk_len);
  }
  if (seq >= MAX_NUM_PACKETS_PER_FILTER) {
    log_errorf("Error: sequence number %d out of bounds %d\r\n", seq,
        MAX_NUM_PACKETS_PER_FILTER);
  } else {
    if (!download.is_active) {
      dongle_download_start();
      download.packet_buffer.buffer.data_len = chunk_len;
    }
    download.n_total_packets++;
    int prev = download.packet_buffer.counts[seq];
    download.packet_buffer.counts[seq]++;
    if (prev == 0) {
      // this is an unseen packet
      download.packet_buffer.num_distinct++;
      uint8_t len = data_len - PACKET_HEADER_LEN;
      memcpy(download.packet_buffer.buffer.data + (seq * MAX_PACKET_SIZE),
             data + PACKET_HEADER_LEN, len);
      download.packet_buffer.received += len;
      log_infof("download progress: %.2f\r\n",
        ((float) download.packet_buffer.received
         /download.packet_buffer.buffer.data_len) * 100);
      if (download.packet_buffer.received
          >= download.packet_buffer.buffer.data_len) {
        // there may be extra data in the packet
        dongle_download_complete();
      }
    }
  }
}

void dongle_download_complete()
{
  log_infof("%s", "Download complete!\r\n");
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

  log_infof("filter len: %lu\r\n", filter_len);

  // now we know the payload is the correct size

  num_buckets = cf_gadget_num_buckets(filter_len);

  if (num_buckets == 0) {
    log_errorf("%s", "num buckets is 0!!!\r\n");
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
  dongle_storage_load_all_encounter(&storage, dongle_download_check_match);
#endif

  dongle_download_reset();
}

void dongle_download_info()
{
  log_infof("Total time: %.0f ms\r\n", download.time);
  log_infof("Packets Received: %lu\r\n", download.n_total_packets);
  log_infof("Data bytes downloaded: %lu\r\n", download.packet_buffer.received);
}
