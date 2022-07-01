#include "test.h"

#include "storage.h"
#include "encounter.h"
#include "stats.h"

#include "common/src/util/log.h"
#include "common/src/util/util.h"
#include "common/src/test.h"
#include "cuckoofilter-gadget/cf-gadget.h"

extern dongle_config_t config;
extern dongle_timer_t dongle_time;
extern dongle_timer_t report_time;
extern dongle_stats_t stats;

#if 0
#if TEST_DONGLE
enctr_entry_counter_t non_report_entry_count = 0;

int test_errors = 0;
#define TEST_MAX_ENCOUNTERS 16
int test_encounters = 0;
int total_test_encounters = 0;
/*
 * a simple fifo cache which can be used for matching encounter entries
 */
dongle_encounter_entry_t test_encounter_list[TEST_MAX_ENCOUNTERS];

void dongle_test_encounter(encounter_broadcast_t *enc)
{
  if (*enc->b == TEST_BEACON_ID) {
    test_encounters++;
  }
#define test_en (test_encounter_list[total_test_encounters])
  memcpy(&test_en.location_id, enc->loc, sizeof(beacon_location_id_t));
  test_en.beacon_id = *enc->b;
  test_en.beacon_time_start = *enc->t;
  test_en.dongle_time_start = dongle_time;
  test_en.eph_id = *enc->eph;
  log_debugf("Test Encounter: (index=%d)\r\n", total_test_encounters);
  //_display_encounter_(&test_en);
#undef test_en
  total_test_encounters = (total_test_encounters + 1) % TEST_MAX_ENCOUNTERS;
}

uint8_t compare_encounter_entry(dongle_encounter_entry_t a, dongle_encounter_entry_t b)
{
  uint8_t res = 0;
  log_debugf("%s", "comparing ids\r\n");
#define check(val, idx) res |= (val << idx)
  check(!(a.beacon_id == b.beacon_id), 0);
  log_debugf("%s", "comparing beacon time\r\n");
  check(!(a.beacon_time_start == b.beacon_time_start), 1);
  log_debugf("%s", "comparing dongle time\r\n");
  check(!(a.dongle_time_start == b.dongle_time_start), 2);
  log_debugf("%s", "comparing location ids\r\n");
  check(!(a.location_id == b.location_id), 3);
  log_debugf("%s", "comparing eph. ids\r\n");
  check(compare_eph_id(&a.eph_id, &b.eph_id), 4);
#undef check
  return res;
}

int test_compare_entry_idx(enctr_entry_counter_t i, dongle_encounter_entry_t *entry)
{
  //log_infof("%.4lu.", i);
  //_display_encounter_(entry);
  log_debugf("%s", "comparing logged encounter against test record\r\n");
  dongle_encounter_entry_t test_en = test_encounter_list[i];
  uint8_t comp = compare_encounter_entry(*entry, test_en);
  if (comp) {
    log_infof("FAILED: entry mismatch (index=%lu)\r\n", (uint32_t)i);
    log_infof("Comp=%u\r\n", comp);
    test_errors++;
    log_infof("%s", "Entry from log:\r\n");
    _display_encounter_(entry);
    log_infof("%s", "Test:\r\n");
    _display_encounter_(&test_en);
    return 0;
  } else {
    log_debugf("%s", "Entries MATCH\r\n");
  }
  return 1;
}

int test_check_entry_age(enctr_entry_counter_t i,
  dongle_encounter_entry_t *entry,
  uint32_t num_buckets __attribute__((unused)))
{
  if ((dongle_time - entry->dongle_time_start) > DONGLE_MAX_LOG_AGE) {
    log_infof("FAILED: Encounter at index %lu is too old (age=%lu)\r\n",
              (uint32_t)i, (uint32_t)entry->dongle_time_start);
    test_errors++;
    return 0;
  }
  return 1;
}

void dongle_test()
{
  // Run Tests
  log_infof("%s", "=== Tests: ===\r\n");
  test_errors = 0;
#define FAIL(msg)                          \
  log_infof("    FAILURE: %s\r\n", msg); \
  test_errors++

  log_infof("%s", "    OTPs loaded?\r\n");
  int otp_idx = dongle_storage_match_otp(&storage, TEST_OTPS[7].val);
  if (otp_idx != 7) {
    FAIL("Index 7 Not loaded correctly");
  }
  otp_idx = dongle_storage_match_otp(&storage, TEST_OTPS[0].val);
  if (otp_idx != 0) {
    FAIL("Index 0 Not loaded correctly");
  }
  log_infof("%s", "    OTP reused?\r\n");
  otp_idx = dongle_storage_match_otp(&storage, TEST_OTPS[7].val);
  if (otp_idx >= 0) {
    FAIL("Index 7 was found again");
  }
  // Restore OTP data
  // Need to re-save config as the shared page must be erased
  dongle_storage_save_config(&storage, &config);

  log_infof("#encounters logged: %d, expected: %d\r\n", test_encounters,
      (DONGLE_REPORT_INTERVAL / BEACON_EPOCH_LENGTH));
#if 0
  log_infof("%s", "    ? correct #encounters logged?\r\n");
  int numExpected = (DONGLE_REPORT_INTERVAL / BEACON_EPOCH_LENGTH);
  // Tolerant expectation provided to account for timing differences
  int tolExpected = numExpected + 1;
  if (test_encounters != numExpected && test_encounters != tolExpected) {
    FAIL("Wrong number of encounters.");
    log_infof("Encounters logged in window: %d; Expected: %d or %d\r\n",
              test_encounters, numExpected, tolExpected);
  }
#endif

  log_infof("%s", "    logged encounters correct?\r\n");
  if (stats.stat_ints.total_encounters > non_report_entry_count) {
    // There are new entries logged
    dongle_storage_load_encounters_from_time(&storage, report_time,
                                             test_compare_entry_idx);
    non_report_entry_count = stats.stat_ints.total_encounters;
  } else {
    log_errorf("%s", "no new encounters logged.\r\n");
  }

  log_infof("%s", "    old encounters deleted?\r\n");
  num = num_encounters_current(config.en_head, config.en_tail);
  _delete_old_encounters(&config, dongle_time, num);
  if (dongle_time <= DONGLE_MAX_LOG_AGE + BEACON_EPOCH_LENGTH) {
    log_errorf("%s", "not enough time has elapsed.\r\n");
  } else if (num < 1) {
    log_errorf("%s", "no encounters stored.\r\n");
  } else {
    dongle_storage_load_encounter(config.en_tail,
        num_encounters_current(config.en_head, config.en_tail),
        test_check_entry_age, 0);
//    dongle_storage_load_all_encounter(&storage, test_check_entry_age);
  }

  if (test_errors) {
    log_infof("%s", "\r\n");
    log_infof("    x Tests Failed: status = %d\r\n", test_errors);
  } else {
    log_infof("%s", "\r\n");
    log_infof("%s", "    ✔ Tests Passed\r\n");
  }
#undef FAIL
  test_encounters = 0;
  total_test_encounters = 0;
}
#endif /* TEST_DONGLE */
#endif

void dongle_test_enctr_storage(void)
{
  uint8_t byte_arr[BEACON_EPH_ID_HASH_LEN] = {1, 2, 3, 4, 5, 6, 7, 8,
    9, 10, 11, 12, 0, 0};
  uint16_t *last_two = (uint16_t *) &byte_arr[BEACON_EPH_ID_HASH_LEN-2];

  dongle_encounter_entry_t test_en = (dongle_encounter_entry_t) {
    .beacon_id = TEST_BEACON_ID+1,
    .location_id = TEST_BEACON_ID+1,
    .beacon_time_start = 10,
    .beacon_time_int = 2,
    .dongle_time_start = 11,
    .dongle_time_int = 2,
    .rssi = -64
  };

//  int max_enctrs = dongle_storage_max_log_count(&storage);
  int i;
  int start_offset = ENCOUNTERS_PER_PAGE * 3;
  int max_enctrs = ENCOUNTERS_PER_PAGE * TARGET_FLASH_LOG_NUM_PAGES - start_offset;
  config.en_head = start_offset;
  log_infof("update off: %u cnt: %u\r\n", start_offset, max_enctrs);
  for (i = 0; i < max_enctrs; i++) {
    *last_two = i;
    memcpy(&test_en.eph_id.bytes, byte_arr, BEACON_EPH_ID_HASH_LEN);
#if 0
    hexdumpen(&test_en.eph_id, BEACON_EPH_ID_HASH_LEN, "test",
        test_en.beacon_id, (uint32_t) test_en.location_id,
        (uint16_t) (start_offset+i),
        (uint32_t) test_en.beacon_time_start, test_en.beacon_time_int,
        (uint32_t) test_en.dongle_time_start, test_en.dongle_time_int,
        (int8_t) test_en.rssi, (uint32_t) ENCOUNTER_LOG_OFFSET((start_offset+i)));
#endif
    dongle_storage_log_encounter(&config, &dongle_time, &test_en);
  }
}

#ifdef CUCKOOFILTER_FIXED_TEST
// Ephemeral IDs known to be in the test filter
static char *TEST_ID_EXIST_1 = "\x08\xb5\xec\x97\xaa\x06\xf8\x82\x27\xeb\x4e\x5a\x83\x72\x5b";
static char *TEST_ID_EXIST_2 = "\x3d\xbd\xb9\xc4\xf4\xe0\x9f\x1d\xc4\x30\x66\xda\xb8\x25\x3a";
// not in filter
static char *TEST_ID_NEXIST_1 = "blablablablabla";
static char *TEST_ID_NEXIST_2 = "tralalalalalala";

void run_fixed_cf_test(download_t *download, uint32_t num_buckets)
{
  uint8_t *filter = download->packet_buffer.buffer.data;

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

}
#endif
