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

void dongle_storage_erase(dongle_storage *sto, storage_addr_t offset)
{
  log_debugf("erasing page at 0x%x\r\n", (offset));
  int status = MSC_ErasePage((uint32_t *)offset);
  if (status != 0) {
    log_debugf("error erasing page: 0x%x", status);
  }
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
  memcpy(data, (uint32_t *)off, size);
  return 0;
}

int _flash_write_(dongle_storage *sto, storage_addr_t off, void *data, size_t size)
{
  log_debugf("size: %d bytes, addr: 0x%x, flash off: 0x%0x\r\n",
      size, off, sto->map.config);
  return MSC_WriteWord((uint32_t *)off, data, (uint32_t)size);
}

// Upper-bound of size of encounter log, in bytes
#define TARGET_FLASH_LOG_SIZE (sto->total_size - FLASH_OFFSET)

// Number of encounters in each page, with no encounters stored across pages
#define ENCOUNTERS_PER_PAGE (sto->page_size / ENCOUNTER_ENTRY_SIZE)

size_t dongle_storage_max_log_count(dongle_storage *sto)
{
  size_t encounter_log_size = sto->map.log_end - sto->map.log;
  return (encounter_log_size / sto->page_size) * ENCOUNTERS_PER_PAGE;
}

void dongle_storage_init_device(dongle_storage *sto)
{
  MSC_ExecConfig_TypeDef execConfig = MSC_EXECCONFIG_DEFAULT;
  sto->mscExecConfig = execConfig;
  MSC_ExecConfigSet(&sto->mscExecConfig);
  MSC_Init();
}

void dongle_storage_init(dongle_storage *sto)
{
  log_debugf("%s", "Initializing storage...\r\n");
  dongle_storage_init_device(sto);
  sto->encounters.tail = 0;
  sto->encounters.head = 0;
  sto->total_encounters = 0;
  sto->numErasures = 0;
  sto->num_pages = FLASH_DEVICE_NUM_PAGES;
  sto->min_block_size = FLASH_DEVICE_BLOCK_SIZE;
  sto->page_size = FLASH_DEVICE_PAGE_SIZE;
  sto->total_size = sto->num_pages * sto->page_size;
  log_infof("Pages: %d, Page Size: %u\r\n", sto->num_pages, sto->page_size);
  sto->map.config = sto->total_size - sto->page_size;
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
  read(sizeof(dongle_timer_t), &cfg->t_cur);
  read(sizeof(key_size_t), &cfg->backend_pk_size);
  log_debugf("bknd key off: %u, size: %u\r\n", off, cfg->backend_pk_size);
  if (cfg->backend_pk_size > PK_MAX_SIZE) {
    log_errorf("Key size read for backend pubkey (%u > %u)\r\n",
        cfg->backend_pk_size, PK_MAX_SIZE);
    cfg->backend_pk_size = PK_MAX_SIZE;
  }
  read(cfg->backend_pk_size, &cfg->backend_pk);
  hexdumpn(cfg->backend_pk.bytes, 16, "   Server PK", 0, 0, 0, 0, 0);
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
  hexdumpn(cfg->dongle_sk.bytes, 16, "Si Dongle SK", 0, 0, 0, 0, 0);
  // slide through the extra space for a pubkey
  off += SK_MAX_SIZE - cfg->dongle_sk_size;

  read(sizeof(uint32_t), &cfg->en_tail);
  read(sizeof(uint32_t), &cfg->en_head);
  log_infof("head: %u\r\n", cfg->en_head);
  log_infof("tail: %u\r\n", cfg->en_tail);


  sto->map.otp = off;

  off += NUM_OTP * sizeof(dongle_otp_t); // add size of OTP

  sto->map.stat = off;

  sto->map.log = FLASH_OFFSET;
  sto->map.log_end = NVM_OFFSET - 1;
#undef read
  log_debugf("%s", "Config loaded.\r\n");
  log_infof("    Flash offset:    %u\r\n", FLASH_OFFSET);
  log_infof("    Total Size:    %u\r\n", sto->total_size);
  log_infof("    Config offset:    %u\r\n", sto->map.config);
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
  write(&cfg->t_cur, sizeof(dongle_timer_t));
  write(&cfg->backend_pk_size, sizeof(key_size_t));
  write(&cfg->backend_pk, PK_MAX_SIZE);
  write(&cfg->dongle_sk_size, sizeof(key_size_t));
  write(&cfg->dongle_sk, SK_MAX_SIZE);
  write(&cfg->en_tail, sizeof(enctr_entry_counter_t));
  write(&cfg->en_head, sizeof(enctr_entry_counter_t));

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

void inc_head(dongle_storage *sto) {
  sto->encounters.head = (sto->encounters.head + 1) % (dongle_storage_max_log_count(sto));
}

void inc_tail(dongle_storage *sto) {
  sto->encounters.tail = (sto->encounters.tail + 1) % (dongle_storage_max_log_count(sto));
}

void _log_increment_(dongle_storage *sto, dongle_config_t *cfg)
{
  inc_head(sto);
  // FORCED_DELETION
  // If the head catches, the tail, can either opt to block or delete.
  // We delete since newer records are preferred.
  if (sto->encounters.head == sto->encounters.tail) {
    log_errorf("Encounter storage full; idx=%lu\r\n", (uint32_t)sto->encounters.head);
    inc_tail(sto);
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
    result = dongle_storage_max_log_count(sto) -
    		(sto->encounters.tail - sto->encounters.head);
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
  dongle_encounter_entry_t en;
  enctr_entry_counter_t i = 0;
  enctr_entry_counter_t num = dongle_storage_num_encounters_current(sto);
#define age (cur_time - (en.dongle_time_start + en.dongle_time_int))
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
    inc_tail(sto);
    log_debugf("[%u] age: %u > %u, head: %u, tail: %u -> %u\r\n", i, age,
        DONGLE_MAX_LOG_AGE, sto->encounters.head, old_tail, sto->encounters.tail);

    i++;
  } while (sto->encounters.tail != sto->encounters.head);
#undef age
}

// Encounter offset is page number + offset in page
#define ENCOUNTER_LOG_OFFSET(j) \
    (sto->map.log + (j / ENCOUNTERS_PER_PAGE) * sto->page_size + \
    	(j % ENCOUNTERS_PER_PAGE)*ENCOUNTER_ENTRY_SIZE)

storage_addr_t get_encounter_offset(dongle_storage *sto, enctr_entry_counter_t i) {
  storage_addr_t offset = sto->map.log + (i / ENCOUNTERS_PER_PAGE) * sto->page_size +
        (i % ENCOUNTERS_PER_PAGE)*ENCOUNTER_ENTRY_SIZE;
  return offset;
}

storage_addr_t get_page_offset(dongle_storage *sto, enctr_entry_counter_t i) {
  storage_addr_t offset = sto->map.log + (i / ENCOUNTERS_PER_PAGE) * sto->page_size;
  return offset;
}

void dongle_storage_load_encounter(dongle_storage *sto,
    enctr_entry_counter_t i, dongle_encounter_cb cb)
{
  log_debugf("loading log entries starting at (virtual) index %lu\r\n", (uint32_t)i);
  enctr_entry_counter_t num = dongle_storage_num_encounters_current(sto);
  dongle_encounter_entry_t en;
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
    enctr_entry_counter_t i, dongle_encounter_entry_t *en)
{
  enctr_entry_counter_t num = dongle_storage_num_encounters_current(sto);
  if (i >= num) {
    log_errorf("Index for encounter log (%lu) is too large\r\n", (uint32_t)i);
    return;
  }
  storage_addr_t off = get_encounter_offset(sto, i);
  _flash_read_(sto, off, en, sizeof(dongle_encounter_entry_t));
}

void dongle_storage_load_encounters_from_time(dongle_storage *sto,
    dongle_timer_t min_time, dongle_encounter_cb cb)
{
  log_debugf("loading log entries starting at time %lu\r\n", (uint32_t)min_time);
  enctr_entry_counter_t num = dongle_storage_num_encounters_current(sto);
  dongle_encounter_entry_t en;
  enctr_entry_counter_t j = 0;
  for (enctr_entry_counter_t i = 0; i < num; i++) {
    log_debugf("i = %lu\r\n", (uint32_t) i);
    // Can be optimized to track timestamps and avoid extra loads
  //  dongle_storage_load_single_encounter(sto, i, &en);
    if (en.dongle_time_start >= min_time) {
      if (!cb(j, &en)) {
        break;
      }
      j++;
    }
  }
}

void dongle_storage_log_encounter(dongle_storage *sto, dongle_config_t *cfg,
		dongle_timer_t *dongle_time, dongle_encounter_entry_t *en)
{
  enctr_entry_counter_t num1, num2, num3;
  num1 = dongle_storage_num_encounters_current(sto);
  storage_addr_t start = get_encounter_offset(sto, sto->encounters.head);

  storage_addr_t off = start;

  // NOTE: once head has wrapped, erasing the next page before writing
  // will cause a page of old entries to be lost (204 entries)
  // currently, storing the old page in an in-memory buffer does not work
  // because allocating an 8K static or dynamic memory buffer is not
  // possible
  pre_erase(sto, off, ENCOUNTER_ENTRY_SIZE);


#define write(data, size) _flash_write_(sto, off, data, size), off += size

  write(en, ENCOUNTER_ENTRY_SIZE);

#undef write

  sto->total_encounters++;
  _log_increment_(sto, cfg);
  num2 = dongle_storage_num_encounters_current(sto);
  _delete_old_encounters_(sto, *dongle_time);
  num3 = dongle_storage_num_encounters_current(sto);
  log_debugf("time: %u, %u #entries: %lu -> %lu -> %lu, "
      "H: %lu, T: %lu, off: %u, size: %u %u\r\n",
      en->dongle_time_start, stat_start, num1, num2, num3,
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
      (uint32_t) dongle_storage_max_log_count(sto));
}

void dongle_storage_clean_log(dongle_storage *sto, dongle_timer_t cur_time)
{
  _delete_old_encounters_(sto, cur_time);
}

#undef next_multiple
#undef prev_multiple
