#include "test.h"

#include "storage.h"

extern dongle_storage storage;
extern dongle_config_t config;
extern enctr_entry_counter_t non_report_entry_count;
extern dongle_timer_t dongle_time;
extern dongle_timer_t report_time;

int test_errors = 0;
#define TEST_MAX_ENCOUNTERS 64
int test_encounters = 0;
int total_test_encounters = 0;
dongle_encounter_entry test_encounter_list[TEST_MAX_ENCOUNTERS];

int test_compare_entry_idx(enctr_entry_counter_t i, dongle_encounter_entry *entry)
{
    //log_infof("%.4lu.", i);
    //_display_encounter_(entry);
    log_debug("comparing logged encounter against test record\r\n");
    dongle_encounter_entry test_en = test_encounter_list[i];
    uint8_t comp = compare_encounter_entry(*entry, test_en);
    if (comp)
    {
        log_infof("FAILED: entry mismatch (index=%lu)\r\n", (uint32_t)i);
        log_infof("Comp=%u\r\n", comp);
        test_errors++;
        log_info("Entry from log:\r\n");
        _display_encounter_(entry);
        log_info("Test:\r\n");
        _display_encounter_(&test_en);
        return 0;
    }
    else
    {
        log_debug("Entries MATCH\r\n");
    }
    return 1;
}

int test_check_entry_age(enctr_entry_counter_t i, dongle_encounter_entry *entry)
{
    if ((dongle_time - entry->dongle_time) > DONGLE_MAX_LOG_AGE)
    {
        log_infof("FAILED: Encounter at index %lu is too old (age=%lu)\r\n",
                  (uint32_t)i, (uint32_t)entry->dongle_time);
        test_errors++;
        return 0;
    }
    return 1;
}

void dongle_test()
{
    // Run Tests
    log_info("\r\n");
    log_info("Tests:\r\n");
    test_errors = 0;
#define FAIL(msg)                          \
    log_infof("    FAILURE: %s\r\n", msg); \
    test_errors++

    log_info("    ? Testing that OTPs are loaded\r\n");
    int otp_idx = dongle_storage_match_otp(&storage, TEST_OTPS[7].val);
    if (otp_idx != 7)
    {
        FAIL("Index 7 Not loaded correctly");
    }
    otp_idx = dongle_storage_match_otp(&storage, TEST_OTPS[0].val);
    if (otp_idx != 0)
    {
        FAIL("Index 0 Not loaded correctly");
    }
    log_info("    ? Testing that OTP cannot be re-used\r\n");
    otp_idx = dongle_storage_match_otp(&storage, TEST_OTPS[7].val);
    if (otp_idx >= 0)
    {
        FAIL("Index 7 was found again");
    }
    // Restore OTP data
    // Need to re-save config as the shared page must be erased
    dongle_storage_save_config(&storage, &config);
    dongle_storage_save_otp(&storage, TEST_OTPS);

    log_info("    ? Testing that correct number of encounters were logged\r\n");
    int numExpected = (DONGLE_REPORT_INTERVAL / DONGLE_ENCOUNTER_MIN_TIME);
    // Tolerant expectation provided to account for timing differences
    int tolExpected = numExpected + 1;
    if (test_encounters != numExpected && test_encounters != tolExpected)
    {
        FAIL("Wrong number of encounters.");
        log_infof("Encounters logged in window: %d; Expected: %d or %d\r\n",
                  test_encounters, numExpected, tolExpected);
    }

    log_info("    ? Testing that logged encounters are correct\r\n");
    enctr_entry_counter_t num = dongle_storage_num_encounters_total(&storage);
    if (num > non_report_entry_count)
    {
        // There are new entries logged
        dongle_storage_load_encounters_from_time(&storage, report_time,
                                                 test_compare_entry_idx);
    }
    else
    {
        log_error("Cannot test, no new encounters logged.\r\n");
    }

    log_info("    ? Testing that old encounters are deleted\r\n");
    dongle_storage_clean_log(&storage, dongle_time);
    num = dongle_storage_num_encounters_current(&storage);
    if (dongle_time <= DONGLE_MAX_LOG_AGE + DONGLE_ENCOUNTER_MIN_TIME)
    {
        log_error("Cannot test, not enough time has elapsed.\r\n");
    }
    else if (num < 1)
    {
        log_error("Cannot test, no encounters stored.\r\n");
    }
    else
    {
        dongle_storage_load_all_encounter(&storage, test_check_entry_age);
    }

    if (test_errors)
    {
        log_info("\r\n");
        log_infof("    x Tests Failed: status = %d\r\n", test_errors);
    }
    else
    {
        log_info("\r\n");
        log_info("    ✔ Tests Passed\r\n");
    }
#undef FAIL
    test_encounters = 0;
    total_test_encounters = 0;
}