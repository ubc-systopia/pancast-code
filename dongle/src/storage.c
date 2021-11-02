#define LOG_LEVEL__INFO

#include "storage.h"

#include <string.h>

#include "common/src/platform/gecko.h"
#include "common/src/util/log.h"
#include "common/src/util/util.h"

#define prev_multiple(k, n) ((n) - ((n) % (k)))
#define next_multiple(k, n) ((n) + ((k) - ((n) % (k)))) // buggy

#ifdef DONGLE_PLATFORM__ZEPHYR
static bool _flash_page_info_(const struct flash_pages_info *info, void *data)
{
#define sto ((dongle_storage *)data)
  if (!sto->num_pages) {
    sto->page_size = info->size;
  } else if (info->size != sto->page_size) {
    log_errorf("differing page sizes! (%u and %u)\r\n", info->size,
               sto->page_size);
  }
  sto->num_pages++;
  return true;
#undef sto
}
#endif

#define st (*sto)

void dongle_storage_erase(dongle_storage *sto, storage_addr_t offset)
{
  log_debugf("erasing page at 0x%x\r\n", (offset));
#ifdef DONGLE_PLATFORM__ZEPHYR
  flash_erase(st.dev, (offset), st.page_size);
#else
  MSC_ErasePage((uint32_t *)offset);
#endif
  log_debugf("%s", "erased.\r\n");
  st.numErasures++;
}

#define erase(addr) dongle_storage_erase(sto, addr)

void pre_erase(dongle_storage *sto, size_t write_size)
{
// Erase before write
#define page_num(o) ((o) / st.page_size)
  if ((st.off % st.page_size) == 0) {
    erase(st.off);
  } else if (page_num(st.off + write_size) > page_num(st.off)) {
#undef page_num
    erase(next_multiple(st.page_size, st.off));
  }
}

int _flash_read_(dongle_storage *sto, void *data, size_t size)
{
  log_debugf("reading %d bytes from flash at address 0x%x\r\n", size, st.off);
#ifdef DONGLE_PLATFORM__ZEPHYR
  return flash_read(st.dev, st.off, data, size);
#else
  memcpy(data, (uint32_t *)st.off, size);
  return 0;
#endif
}

int _flash_write_(dongle_storage *sto, void *data, size_t size)
{
  log_debugf("writing %d bytes to flash at address 0x%x\r\n", size, st.off);
#ifdef DONGLE_PLATFORM__ZEPHYR
  return flash_write(st.dev, st.off, data, size)
         ? log_errorf("%s", "Error writing flash\r\n"),
         1 : 0;
#else
  MSC_WriteWord((uint32_t *)st.off, data, (uint32_t)size);
  return 0;
#endif
}

void dongle_storage_get_info(dongle_storage *sto)
{
#ifdef DONGLE_PLATFORM__ZEPHYR
  log_infof("%s", "Getting flash information...\r\n");
  st.num_pages = 0;
  st.min_block_size = flash_get_write_block_size(st.dev);
  flash_page_foreach(st.dev, _flash_page_info_, sto);
#else
  st.num_pages = FLASH_DEVICE_NUM_PAGES;
  st.min_block_size = FLASH_DEVICE_BLOCK_SIZE;
  st.page_size = FLASH_DEVICE_PAGE_SIZE;
#endif
  st.total_size = st.num_pages * st.page_size;
}

// Upper-bound of size of encounter log, in bytes
#define TARGET_FLASH_LOG_SIZE (st.total_size - FLASH_OFFSET)

size_t dongle_storage_max_log_count(dongle_storage *sto)
{
  return MAX_LOG_COUNT;
}

void dongle_storage_init_device(dongle_storage *sto)
{
#ifdef DONGLE_PLATFORM__ZEPHYR
  st.dev = device_get_binding(DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL);
#else
  MSC_ExecConfig_TypeDef execConfig = MSC_EXECCONFIG_DEFAULT;
  st.mscExecConfig = execConfig;
  MSC_ExecConfigSet(&st.mscExecConfig);
  MSC_Init();
#endif
}

void dongle_storage_init(dongle_storage *sto)
{
  log_debugf("%s", "Initializing storage...\r\n");
  st.off = 0;
  st.encounters.tail = 0;
  st.encounters.head = 0;
  st.total_encounters = 0;
  st.numErasures = 0;
  dongle_storage_init_device(sto);
  dongle_storage_get_info(sto);
  log_infof("Pages: %d, Page Size: %u\r\n", st.num_pages, st.page_size);
  st.map.config = FLASH_OFFSET;
  if (FLASH_OFFSET % st.page_size != 0) {
    log_errorf("Storage area start addr %u is not page (%u) aligned!\r\n",
        FLASH_OFFSET, st.page_size);
  }
}

#define cf (*cfg)

void dongle_storage_load_config(dongle_storage *sto, dongle_config_t *cfg)
{
  log_debugf("%s", "Loading config...\r\n");
  st.off = st.map.config;
#define read(size, dst) (_flash_read_(sto, dst, size), st.off += size)
  read(sizeof(dongle_id_t), &cf.id);
  read(sizeof(dongle_timer_t), &cf.t_init);
  read(sizeof(key_size_t), &cf.backend_pk_size);
  if (cf.backend_pk_size > PK_MAX_SIZE) {
    log_errorf("Key size read for backend pubkey (%u > %u)\r\n",
        cf.backend_pk_size, PK_MAX_SIZE);
    cf.backend_pk_size = PK_MAX_SIZE;
  }
  read(PK_MAX_SIZE, &cf.backend_pk);
  read(sizeof(key_size_t), &cf.dongle_sk_size);
  if (cf.dongle_sk_size > SK_MAX_SIZE) {
    log_errorf("Key size read for dongle privkey (%u > %u)\r\n",
        cf.dongle_sk_size, SK_MAX_SIZE);
    cf.dongle_sk_size = SK_MAX_SIZE;
  }
  read(SK_MAX_SIZE, &cf.dongle_sk);
  st.map.otp = st.off;
  // push onto the next blank page
  st.map.stat = next_multiple(st.page_size,
                           st.map.otp + (NUM_OTP * sizeof(dongle_otp_t)));
  st.map.log = st.map.stat + st.page_size;
  st.map.log_end = st.map.log + FLASH_LOG_SIZE;
#undef read
  log_debugf("%s", "Config loaded.\r\n");
  log_infof("    Flash offset:    %u\r\n", st.map.config);
  log_infof("    OTP offset:      %u\r\n", st.map.otp);
  log_infof("    Stat offset:     %u\r\n", st.map.stat);
  log_infof("    Log offset:      %u-%u\r\n", st.map.log, st.map.log_end);
}

void dongle_storage_save_config(dongle_storage *sto, dongle_config_t *cfg)
{
  log_debugf("%s", "Saving config\r\n");
  st.off = st.map.config;
#define write(data, size) (pre_erase(sto, size), _flash_write_(sto, data, size), st.off += size)
  write(&cf.id, sizeof(dongle_id_t));
  write(&cf.t_init, sizeof(dongle_timer_t));
  write(&cf.backend_pk_size, sizeof(key_size_t));
  write(&cf.backend_pk, PK_MAX_SIZE);
  write(&cf.dongle_sk_size, sizeof(key_size_t));
  write(&cf.dongle_sk, SK_MAX_SIZE);
#undef write
  log_debugf("%s", "saved.\r\n");
}

#undef cf

#define OTP(i) (st.map.otp + (i * sizeof(dongle_otp_t)))

void dongle_storage_load_otp(dongle_storage *sto, int i, dongle_otp_t *otp)
{
  st.off = OTP(i), _flash_read_(sto, otp, sizeof(dongle_otp_t));
}

void dongle_storage_save_otp(dongle_storage *sto, otp_set otps)
{
  log_debugf("%s", "Saving OTPs\r\n");
  for (int i = 0; i < NUM_OTP; i++) {
    st.off = OTP(i);
    pre_erase(sto, sizeof(dongle_otp_t));
    _flash_write_(sto, &otps[i], sizeof(dongle_otp_t));
  }
  log_debugf("%s", "Saved.\r\n");
}

int otp_is_used(dongle_otp_t *otp)
{
  return !((otp->flags & 0x0000000000000001) >> 0);
}

int dongle_storage_match_otp(dongle_storage *sto, uint64_t val)
{
  dongle_otp_t otp;
  for (int i = 0; i < NUM_OTP; i++) {
    dongle_storage_load_otp(sto, i, &otp);
    if (otp.val == val && !otp_is_used(&otp)) {
      otp.flags &= 0xfffffffffffffffe;
      st.off = OTP(i),
      _flash_write_(sto, &otp, sizeof(dongle_otp_t));
      return i;
    }
  }
  return -1;
}

#define inc_head(_) (st.encounters.head = (st.encounters.head + 1) % MAX_LOG_COUNT)
#define inc_tail(_) (st.encounters.tail = (st.encounters.tail + 1) % MAX_LOG_COUNT)

void _log_increment_(dongle_storage *sto)
{
  inc_head();
  // FORCED_DELETION
  // If the head catches, the tail, can either opt to block or delete.
  // We delete since newer records are preferred.
  if (st.encounters.head == st.encounters.tail) {
    log_debugf("Head caught up; idx=%lu\r\n", (uint32_t)st.encounters.head);
    inc_tail();
  }
}

enctr_entry_counter_t dongle_storage_num_encounters_current(dongle_storage *sto)
{
  log_debugf("tail: %lu\r\n", (uint32_t)st.encounters.tail);
  log_debugf("head: %lu\r\n", (uint32_t)st.encounters.head);
  enctr_entry_counter_t result;
  if (st.encounters.head >= st.encounters.tail) {
    result = st.encounters.head - st.encounters.tail;
  } else {
    result = MAX_LOG_COUNT;
  }
  log_debugf("result: %lu\r\n", (uint32_t)result);
  return result;
}

enctr_entry_counter_t dongle_storage_num_encounters_total(dongle_storage *sto)
{
  return st.total_encounters;
}

// Adjust encounter indexing to ensure that the oldest log entry satisfies the
// age criteria
void _delete_old_encounters_(dongle_storage *sto, dongle_timer_t cur_time)
{
  log_debugf("%s", "deleting...\r\n");
  dongle_encounter_entry en;
  enctr_entry_counter_t i = 0;
  enctr_entry_counter_t num = dongle_storage_num_encounters_current(sto);
#define age (cur_time - en.dongle_time)
#define old (age > DONGLE_MAX_LOG_AGE)
  do {
    if (i >= num) {
        break;
    }
    log_debugf("tail: %lu\r\n", (uint32_t)st.encounters.tail);
    log_debugf("head: %lu\r\n", (uint32_t)st.encounters.head);
    // tail is updated during loop, so reference first index every time
    dongle_storage_load_single_encounter(sto, 0, &en);
    log_debugf("age: %lu\r\n", (uint32_t) age);
    if (old) {
      log_debugf("%s", "incrementing tail\r\n");
      inc_tail();
      i++;
    } else {
      log_debugf("%s", "break\r\n");
      break;
    }
  } while (st.encounters.tail != st.encounters.head);
#undef old
#undef age
}

#define ENCOUNTER_LOG_OFFSET(j) \
    (st.map.log +               \
     (((st.encounters.tail + j) % MAX_LOG_COUNT) * ENCOUNTER_ENTRY_SIZE))

void dongle_storage_load_encounter(dongle_storage *sto,
                                   enctr_entry_counter_t i, dongle_encounter_cb cb)
{
  log_debugf("loading log entries starting at (virtual) index %lu\r\n", (uint32_t)i);
  enctr_entry_counter_t num = dongle_storage_num_encounters_current(sto);
  dongle_encounter_entry en;
  do {
    if (i >= num)
      break;

    dongle_storage_load_single_encounter(sto, i, &en);
    i++;
  } while (cb(i - 1, &en));
}

void dongle_storage_load_all_encounter(dongle_storage *sto, dongle_encounter_cb cb)
{
  dongle_storage_load_encounter(sto, 0, cb);
}

void dongle_storage_load_single_encounter(dongle_storage *sto,
                                          enctr_entry_counter_t i, dongle_encounter_entry *en)
{
  enctr_entry_counter_t num = dongle_storage_num_encounters_current(sto);
  if (i >= num) {
    log_errorf("Index for encounter log (%lu) is too large\r\n", (uint32_t)i);
    return;
  }
  st.off = ENCOUNTER_LOG_OFFSET(i);
#define read(size, dst) _flash_read_(sto, dst, size), st.off += size
  read(sizeof(beacon_id_t), &en->beacon_id);
  read(sizeof(beacon_location_id_t), &en->location_id);
  read(sizeof(beacon_timer_t), &en->beacon_time);
  read(sizeof(dongle_timer_t), &en->dongle_time);
  read(BEACON_EPH_ID_SIZE, &en->eph_id);
#undef read
}

void dongle_storage_load_encounters_from_time(dongle_storage *sto,
    dongle_timer_t min_time, dongle_encounter_cb cb)
{
  log_debugf("loading log entries starting at time %lu\r\n", (uint32_t)min_time);
  enctr_entry_counter_t num = dongle_storage_num_encounters_current(sto);
  dongle_encounter_entry en;
  enctr_entry_counter_t j = 0;
  for (enctr_entry_counter_t i = 0; i < num; i++) {
    log_debugf("i = %lu\r\n", (uint32_t) i);
    // Can be optimized to track timestamps and avoid extra loads
    dongle_storage_load_single_encounter(sto, i, &en);
    if (en.dongle_time >= min_time) {
      if (!cb(j, &en)) {
        break;
      }
      j++;
    }
  }
}

void dongle_storage_log_encounter(dongle_storage *sto,
    beacon_location_id_t *location_id, beacon_id_t *beacon_id,
    beacon_timer_t *beacon_time, dongle_timer_t *dongle_time,
    beacon_eph_id_t *eph_id)
{
  enctr_entry_counter_t num = dongle_storage_num_encounters_current(sto);
  storage_addr_t start = ENCOUNTER_LOG_OFFSET(st.encounters.head - st.encounters.tail);
  st.off = start;
  log_debugf("write log; existing entries: %lu, offset: 0x%x\r\n",
             (uint32_t)num, st.off);
  // TODO: save erased into memory in case the cursor has wrapped around
  // currently reads corrupted data once the max size is reached
  // can probably be done with a page buffer, but may lose up to page
  // of data if dongle is stopped
  pre_erase(sto, ENCOUNTER_ENTRY_SIZE);
#define write(data, size) \
  _flash_write_(sto, data, size), st.off += size
  write(beacon_id, sizeof(beacon_id_t));
  write(location_id, sizeof(beacon_location_id_t));
  write(beacon_time, sizeof(beacon_timer_t));
  write(dongle_time, sizeof(dongle_timer_t));
  write(eph_id, BEACON_EPH_ID_SIZE);
  log_debugf("total size: %u (entry size=%d)\r\n", st.off - start, ENCOUNTER_ENTRY_SIZE);
#undef write
  st.total_encounters++;
  _log_increment_(sto);
  num = dongle_storage_num_encounters_current(sto);
  log_debugf("log now contains %lu entries\r\n", (uint32_t)num);
  _delete_old_encounters_(sto, *dongle_time);
  num = dongle_storage_num_encounters_current(sto);
  log_debugf("after deletion, contains %lu entries\r\n", (uint32_t)num);
}

int dongle_storage_print(dongle_storage *sto, storage_addr_t addr, size_t len)
{
  if (len > DONGLE_STORAGE_MAX_PRINT_LEN) {
    log_errorf("%s", "Cannot print that many bytes of flash");
    return 1;
  }
  uint8_t data[DONGLE_STORAGE_MAX_PRINT_LEN];
  st.off = addr;
  _flash_read_(sto, data, len);
  print_bytes(data, len, "Flash data");
  return 0;
}

void dongle_storage_save_stat(dongle_storage *sto, void * stat, size_t len)
{
  dongle_storage_erase(sto, st.map.stat);
  st.off = st.map.stat;
  _flash_write_(sto, stat, len);
}

void dongle_storage_read_stat(dongle_storage *sto, void * stat, size_t len)
{
  st.off = st.map.stat;
  _flash_read_(sto, stat, len);
}

void dongle_storage_info(dongle_storage *sto)
{
  log_infof("Total flash space:       %lu bytes\r\n",
      (uint32_t) FLASH_LOG_SIZE);
  log_infof("Maximum enctr entries:   %lu\r\n",
      (uint32_t) MAX_LOG_COUNT);
}

void dongle_storage_clean_log(dongle_storage *sto, dongle_timer_t cur_time)
{
  _delete_old_encounters_(sto, cur_time);
}

#undef block_align
#undef st
#undef align
#undef next_multiple
#undef prev_multiple
