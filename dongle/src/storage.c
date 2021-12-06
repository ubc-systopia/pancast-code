#define LOG_LEVEL__INFO

#include "storage.h"

#include <string.h>

#include "common/src/platform/gecko.h"
#include "common/src/util/log.h"
#include "common/src/util/util.h"

#define prev_multiple(k, n) ((n) - ((n) % (k)))
#define next_multiple(k, n) ((n) + ((k) - ((n) % (k)))) // buggy

extern dongle_timer_t stat_start;
extern dongle_timer_t dongle_time;

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

void dongle_storage_erase(dongle_storage *sto, storage_addr_t offset)
{
  log_debugf("erasing page at 0x%x\r\n", (offset));
#ifdef DONGLE_PLATFORM__ZEPHYR
  flash_erase(sto->dev, (offset), sto->page_size);
#else
  MSC_ErasePage((uint32_t *)offset);
#endif
  log_debugf("%s", "erased.\r\n");
  sto->numErasures++;
}

void pre_erase(dongle_storage *sto, storage_addr_t off, size_t write_size)
{
// Erase before write
#define page_num(o) ((o) / sto->page_size)
  if ((off % sto->page_size) == 0) {
    dongle_storage_erase(sto, off);
  } else if (page_num(off + write_size) > page_num(off)) {
#undef page_num
    dongle_storage_erase(sto, next_multiple(sto->page_size, off));
  }
}

int _flash_read_(dongle_storage *sto, storage_addr_t off, void *data, size_t size)
{
  log_debugf("size: %d bytes, addr: 0x%x, flash off: 0x%0x\r\n",
      size, off, sto->map.config);
#ifdef DONGLE_PLATFORM__ZEPHYR
  return flash_read(sto->dev, off, data, size);
#else
  memcpy(data, (uint32_t *)off, size);
  return 0;
#endif
}

int _flash_write_(dongle_storage *sto, storage_addr_t off, void *data, size_t size)
{
  log_debugf("size: %d bytes, addr: 0x%x, flash off: 0x%0x\r\n",
      size, off, sto->map.config);
#ifdef DONGLE_PLATFORM__ZEPHYR
  return flash_write(sto->dev, off, data, size)
         ? log_errorf("%s", "Error writing flash\r\n"),
         1 : 0;
#else
  return MSC_WriteWord((uint32_t *)off, data, (uint32_t)size);
#endif
}

void dongle_storage_get_info(dongle_storage *sto)
{
#ifdef DONGLE_PLATFORM__ZEPHYR
  log_infof("%s", "Getting flash information...\r\n");
  sto->num_pages = 0;
  sto->min_block_size = flash_get_write_block_size(sto->dev);
  flash_page_foreach(sto->dev, _flash_page_info_, sto);
#else
  sto->num_pages = FLASH_DEVICE_NUM_PAGES;
  sto->min_block_size = FLASH_DEVICE_BLOCK_SIZE;
  sto->page_size = FLASH_DEVICE_PAGE_SIZE;
#endif
  sto->total_size = sto->num_pages * sto->page_size;
}

// Upper-bound of size of encounter log, in bytes
#define TARGET_FLASH_LOG_SIZE (sto->total_size - FLASH_OFFSET)

size_t dongle_storage_max_log_count(dongle_storage *sto)
{
  return MAX_LOG_COUNT;
}

void dongle_storage_init_device(dongle_storage *sto)
{
#ifdef DONGLE_PLATFORM__ZEPHYR
  sto->dev = device_get_binding(DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL);
#else
  MSC_ExecConfig_TypeDef execConfig = MSC_EXECCONFIG_DEFAULT;
  sto->mscExecConfig = execConfig;
  MSC_ExecConfigSet(&sto->mscExecConfig);
  MSC_Init();
#endif
}

void dongle_storage_init(dongle_storage *sto)
{
  log_debugf("%s", "Initializing storage...\r\n");
  sto->encounters.tail = 0;
  sto->encounters.head = 0;
  sto->total_encounters = 0;
  sto->numErasures = 0;
  dongle_storage_init_device(sto);
  dongle_storage_get_info(sto);
  log_infof("Pages: %d, Page Size: %u\r\n", sto->num_pages, sto->page_size);
  sto->map.config = FLASH_OFFSET;
  if (FLASH_OFFSET % sto->page_size != 0) {
    log_errorf("Storage area start addr %u is not page (%u) aligned!\r\n",
        FLASH_OFFSET, sto->page_size);
  }
}

void dongle_storage_load_config(dongle_storage *sto, dongle_config_t *cfg)
{
  log_debugf("%s", "Loading config...\r\n");
  storage_addr_t off = sto->map.config;
#define read(size, dst) (_flash_read_(sto, off, dst, size), off += size)
  read(sizeof(dongle_id_t), &cfg->id);
  read(sizeof(dongle_timer_t), &cfg->t_init);
  read(sizeof(key_size_t), &cfg->backend_pk_size);
  log_debugf("bknd key off: %u, size: %u\r\n", off, cfg->backend_pk_size);
  if (cfg->backend_pk_size > PK_MAX_SIZE) {
    log_errorf("Key size read for backend pubkey (%u > %u)\r\n",
        cfg->backend_pk_size, PK_MAX_SIZE);
    cfg->backend_pk_size = PK_MAX_SIZE;
  }
  read(cfg->backend_pk_size, &cfg->backend_pk);
  hexdumpn(cfg->backend_pk.bytes, 16, "   Server PK", 0, 0, 0, 0);
  // slide through the extra space for a pubkey
  off += PK_MAX_SIZE - cfg->backend_pk_size;

  read(sizeof(key_size_t), &cfg->dongle_sk_size);
  log_debugf("dongle key off: %u, size: %u\r\n", off, cfg->dongle_sk_size);
  if (cfg->dongle_sk_size > SK_MAX_SIZE) {
    log_errorf("Key size read for dongle privkey (%u > %u)\r\n",
        cfg->dongle_sk_size, SK_MAX_SIZE);
    cfg->dongle_sk_size = SK_MAX_SIZE;
  }
  read(cfg->dongle_sk_size, &cfg->dongle_sk);
  hexdumpn(cfg->dongle_sk.bytes, 16, "Si Dongle SK", 0, 0, 0, 0);
  // slide through the extra space for a pubkey
  off += SK_MAX_SIZE - cfg->dongle_sk_size;

  read(sizeof(uint32_t), &cfg->en_tail);
  read(sizeof(uint32_t), &cfg->en_head);
  log_infof("head: %u\r\n", cfg->en_head);
  log_infof("tail: %u\r\n", cfg->en_tail);


  sto->map.otp = off;
  // push onto the next blank page
  sto->map.stat = next_multiple(sto->page_size,
                           sto->map.otp + (NUM_OTP * sizeof(dongle_otp_t)));
  sto->map.log = sto->map.stat + sto->page_size;
  sto->map.log_end = sto->map.log + FLASH_LOG_SIZE;
#undef read
  log_debugf("%s", "Config loaded.\r\n");
  log_infof("    Flash offset:    %u\r\n", sto->map.config);
  log_infof("    OTP offset:      %u\r\n", sto->map.otp);
  log_infof("    Stat offset:     %u\r\n", sto->map.stat);
  log_infof("    Log offset:      %u-%u\r\n", sto->map.log, sto->map.log_end);
}

void dongle_storage_save_config(dongle_storage *sto, dongle_config_t *cfg)
{
  log_debugf("%s", "Saving config\r\n");
  storage_addr_t off = sto->map.config;
  int total_size = sizeof(dongle_id_t) + sizeof(dongle_timer_t) +
    sizeof(key_size_t)*2 + PK_MAX_SIZE + SK_MAX_SIZE;

  pre_erase(sto, off, total_size);

#define write(data, size) \
  (_flash_write_(sto, off, data, size), off += size)

  write(&cfg->id, sizeof(dongle_id_t));
  write(&cfg->t_init, sizeof(dongle_timer_t));
  write(&cfg->backend_pk_size, sizeof(key_size_t));
  write(&cfg->backend_pk, PK_MAX_SIZE);
  write(&cfg->dongle_sk_size, sizeof(key_size_t));
  write(&cfg->dongle_sk, SK_MAX_SIZE);

#undef write

  log_debugf("off: %u, size: %u\r\n", sto->map.config, total_size);
}

void dongle_storage_save_cursor(dongle_storage *sto, dongle_config_t *cfg)
{
  storage_addr_t off = sto->map.config;
  int total_size = sizeof(dongle_id_t) + sizeof(dongle_timer_t) +
    sizeof(key_size_t)*2 + PK_MAX_SIZE + SK_MAX_SIZE +
	sizeof(enctr_entry_counter_t)*2;

  pre_erase(sto, off, total_size);

#define write(data, size) \
  (_flash_write_(sto, off, data, size), off += size)

 write(cfg, sizeof(dongle_config_t));

#undef write
}

#define OTP(i) (sto->map.otp + (i * sizeof(dongle_otp_t)))

void dongle_storage_load_otp(dongle_storage *sto, int i, dongle_otp_t *otp)
{
  _flash_read_(sto, OTP(i), otp, sizeof(dongle_otp_t));
}

void dongle_storage_save_otp(dongle_storage *sto, otp_set otps)
{
  log_debugf("%s", "Saving OTPs\r\n");
  storage_addr_t off = OTP(0);
  pre_erase(sto, off, (NUM_OTP * sizeof(dongle_otp_t)));

#define write(data, size) \
  (_flash_write_(sto, off, data, size), off += size)

  for (int i = 0; i < NUM_OTP; i++) {
    write(&otps[i], sizeof(dongle_otp_t));
  }

#undef write

  log_infof("off: %u, size: %u\r\n", OTP(0), (NUM_OTP * sizeof(dongle_otp_t)));
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
      _flash_write_(sto, OTP(i), &otp, sizeof(dongle_otp_t));
      return i;
    }
  }
  return -1;
}

#define inc_head(_) \
  (sto->encounters.head = (sto->encounters.head + 1) % MAX_LOG_COUNT)

#define inc_tail(_) \
  (sto->encounters.tail = (sto->encounters.tail + 1) % MAX_LOG_COUNT)

void _log_increment_(dongle_storage *sto, dongle_config_t *cfg)
{
  inc_head();
  // FORCED_DELETION
  // If the head catches, the tail, can either opt to block or delete.
  // We delete since newer records are preferred.
  if (sto->encounters.head == sto->encounters.tail) {
    log_errorf("Encounter storage full; idx=%lu\r\n", (uint32_t)sto->encounters.head);
    inc_tail();
  }
  // save head and tail to flash
  cfg->en_head = sto->encounters.head;
  cfg->en_tail = sto->encounters.tail;
  dongle_storage_save_cursor(sto, cfg);
}

enctr_entry_counter_t dongle_storage_num_encounters_current(dongle_storage *sto)
{
  log_debugf("tail: %lu\r\n", (uint32_t)sto->encounters.tail);
  log_debugf("head: %lu\r\n", (uint32_t)sto->encounters.head);
  enctr_entry_counter_t result;
  if (sto->encounters.head >= sto->encounters.tail) {
    result = sto->encounters.head - sto->encounters.tail;
  } else {
    result = MAX_LOG_COUNT - (sto->encounters.tail - sto->encounters.head);
  }
  log_debugf("result: %lu\r\n", (uint32_t)result);
  return result;
}

enctr_entry_counter_t dongle_storage_num_encounters_total(dongle_storage *sto)
{
  return sto->total_encounters;
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
  do {
    if (i >= num) {
        break;
    }

    uint32_t old_tail = sto->encounters.tail;
    // tail is updated during loop, so reference first index every time
    dongle_storage_load_single_encounter(sto, 0, &en);

    if (age <= DONGLE_MAX_LOG_AGE)
      break;

    // delete old logs
    inc_tail();
    log_debugf("[%u] age: %u > %u, head: %u, tail: %u -> %u\r\n", i, age,
        DONGLE_MAX_LOG_AGE, sto->encounters.head, old_tail, sto->encounters.tail);

    i++;
  } while (sto->encounters.tail != sto->encounters.head);
#undef age
}

#define ENCOUNTER_LOG_OFFSET(j) \
    (sto->map.log +               \
     (((sto->encounters.tail + j) % MAX_LOG_COUNT) * ENCOUNTER_ENTRY_SIZE))

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
  storage_addr_t off = ENCOUNTER_LOG_OFFSET(i);
#define read(size, dst) _flash_read_(sto, off, dst, size), off += size
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

void dongle_storage_log_encounter(dongle_storage *sto, dongle_config_t *cfg,
    beacon_location_id_t *location_id, beacon_id_t *beacon_id,
    beacon_timer_t *beacon_time, dongle_timer_t *dongle_time,
    beacon_eph_id_t *eph_id, int8_t rssi)
{
  enctr_entry_counter_t num1, num2, num3;
  num1 = dongle_storage_num_encounters_current(sto);
  storage_addr_t start =
    ENCOUNTER_LOG_OFFSET(sto->encounters.head - sto->encounters.tail);
  storage_addr_t off = start;

  // TODO: save erased into memory in case the cursor has wrapped around
  // currently reads corrupted data once the max size is reached
  // can probably be done with a page buffer, but may lose up to page
  // of data if dongle is stopped
  pre_erase(sto, off, ENCOUNTER_ENTRY_SIZE);

#define write(data, size) _flash_write_(sto, off, data, size), off += size

  write(beacon_id, sizeof(beacon_id_t));
  write(location_id, sizeof(beacon_location_id_t));
  write(beacon_time, sizeof(beacon_timer_t));
  write(dongle_time, sizeof(dongle_timer_t));

  // store RSSI in the last byte slot of ephemeral ID. This byte is currently
  // unused as we only use 14 byte ephemeral IDs
  eph_id->bytes[BEACON_EPH_ID_SIZE-1] = rssi;

  write(eph_id, BEACON_EPH_ID_SIZE);

#undef write

  sto->total_encounters++;
  _log_increment_(sto, cfg);
  num2 = dongle_storage_num_encounters_current(sto);
  _delete_old_encounters_(sto, *dongle_time);
  num3 = dongle_storage_num_encounters_current(sto);
  log_debugf("time: %u, %u #entries: %lu -> %lu -> %lu, "
      "H: %lu, T: %lu, off: %u, size: %u %u\r\n",
      *dongle_time, stat_start, num1, num2, num3,
      sto->encounters.head, sto->encounters.tail,
      start, off - start, ENCOUNTER_ENTRY_SIZE);
}

int dongle_storage_print(dongle_storage *sto, storage_addr_t addr, size_t len)
{
  if (len > DONGLE_STORAGE_MAX_PRINT_LEN) {
    log_errorf("%s", "Cannot print that many bytes of flash");
    return 1;
  }
  uint8_t data[DONGLE_STORAGE_MAX_PRINT_LEN];
  _flash_read_(sto, addr, data, len);
  print_bytes(data, len, "Flash data");
  return 0;
}

void dongle_storage_save_stat(dongle_storage *sto, void * stat, size_t len)
{
  dongle_storage_erase(sto, sto->map.stat);
  _flash_write_(sto, sto->map.stat, stat, len);
}

void dongle_storage_read_stat(dongle_storage *sto, void * stat, size_t len)
{
  _flash_read_(sto, sto->map.stat, stat, len);
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

#undef next_multiple
#undef prev_multiple
