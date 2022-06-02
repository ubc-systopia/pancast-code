#include "storage.h"

#include <string.h>

#include "common/src/platform/gecko.h"
#include "common/src/util/log.h"
#include "common/src/util/util.h"
#include "stats.h"

#define next_multiple(k, n) ((n) + ((k) - ((n) % (k)))) // buggy

extern dongle_timer_t last_stat_time;
extern dongle_timer_t dongle_time;

static inline void dongle_storage_erase(dongle_storage *sto, storage_addr_t offset)
{
  int status = MSC_ErasePage((uint32_t *)offset);
  if (status != 0) {
    log_errorf("error erasing page: 0x%x", status);
  }
  sto->numErasures++;
}

// Erase before write
void pre_erase(dongle_storage *sto, storage_addr_t off, size_t write_size)
{
#define page_num(o) ((o) / sto->page_size)
  if ((off % sto->page_size) == 0) {
    dongle_storage_erase(sto, off);
  } else if (page_num(off + write_size) > page_num(off)) {
    dongle_storage_erase(sto, next_multiple(sto->page_size, off));
  }
#undef page_num
}

static inline int _flash_read_(storage_addr_t off, void *data, size_t size)
{
  memcpy(data, (uint32_t *) off, size);
  return 0;
}

static inline int _flash_write_(storage_addr_t off, void *data, size_t size)
{
  return MSC_WriteWord((uint32_t *) off, data, (uint32_t)size);
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
  dongle_storage_init_device(sto);
  sto->encounters.tail = 0;
  sto->encounters.head = 0;
  sto->total_encounters = 0;
  sto->numErasures = 0;
  sto->num_pages = FLASH_DEVICE_NUM_PAGES;
  sto->min_block_size = FLASH_DEVICE_BLOCK_SIZE;
  sto->page_size = FLASH_DEVICE_PAGE_SIZE;
  sto->total_size = sto->num_pages * sto->page_size;
  sto->map.config = sto->total_size - sto->page_size;
  if (FLASH_OFFSET % sto->page_size != 0) {
    log_errorf("Storage area start addr %u is not page (%u) aligned!\r\n",
        FLASH_OFFSET, sto->page_size);
  }
}

void dongle_storage_load_config(dongle_storage *sto, dongle_config_t *cfg)
{
  storage_addr_t off = sto->map.config;
#define read(size, dst) (_flash_read_(off, dst, size), off += size)
  read(sizeof(dongle_id_t), &cfg->id);
  read(sizeof(dongle_timer_t), &cfg->t_init);
  read(sizeof(dongle_timer_t), &cfg->t_cur);
  read(sizeof(key_size_t), &cfg->backend_pk_size);
  log_infof("bknd key off: %u, size: %u\r\n", off, cfg->backend_pk_size);
  if (cfg->backend_pk_size > PK_MAX_SIZE) {
    log_errorf("Key size read for backend pubkey (%u > %u)\r\n",
        cfg->backend_pk_size, PK_MAX_SIZE);
    cfg->backend_pk_size = PK_MAX_SIZE;
  }
  read(cfg->backend_pk_size, &cfg->backend_pk);
  hexdumpn(cfg->backend_pk.bytes, 16, "   Server PK");
  // slide through the extra space for a pubkey
  off += PK_MAX_SIZE - cfg->backend_pk_size;

  read(sizeof(key_size_t), &cfg->dongle_sk_size);
  log_infof("dongle key off: %u, size: %u\r\n", off, cfg->dongle_sk_size);
  if (cfg->dongle_sk_size > SK_MAX_SIZE) {
    log_errorf("Key size read for dongle privkey (%u > %u)\r\n",
        cfg->dongle_sk_size, SK_MAX_SIZE);
    cfg->dongle_sk_size = SK_MAX_SIZE;
  }
  read(cfg->dongle_sk_size, &cfg->dongle_sk);
  hexdumpn(cfg->dongle_sk.bytes, 16, "Si Dongle SK");
  // slide through the extra space for a pubkey
  off += SK_MAX_SIZE - cfg->dongle_sk_size;

  read(sizeof(uint32_t), &cfg->en_tail);
  read(sizeof(uint32_t), &cfg->en_head);
  sto->encounters.head = cfg->en_head;
  sto->encounters.tail = cfg->en_tail;

  sto->map.otp = off;

  off += NUM_OTP * sizeof(dongle_otp_t); // add size of OTP

  sto->map.stat = off;

  sto->map.log = FLASH_OFFSET;
  sto->map.log_end = NVM_OFFSET;
#undef read
}

#define OTP(i) (sto->map.otp + (i * sizeof(dongle_otp_t)))

void dongle_storage_save_config(dongle_storage *sto, dongle_config_t *cfg)
{
  storage_addr_t off = sto->map.config;
  int total_size = sizeof(dongle_config_t) +
    (NUM_OTP*sizeof(dongle_otp_t)) + sizeof(dongle_stats_t);

  dongle_otp_t otps[NUM_OTP];
  _flash_read_(OTP(0), otps, NUM_OTP*sizeof(dongle_otp_t));

  char statbuf[sizeof(dongle_stats_t)];
  _flash_read_(sto->map.stat, statbuf, sizeof(dongle_stats_t));

  pre_erase(sto, off, total_size);

#define write(data, size) \
  (_flash_write_(off, data, size), off += size)

  // write config
  write(cfg, sizeof(dongle_config_t));

  // write OTPs
  off = OTP(0);
  write(otps, sizeof(dongle_otp_t)*NUM_OTP);

  // write stats
  off = sto->map.stat;
  write(statbuf, sizeof(dongle_stats_t));
#undef write
}

static inline void dongle_storage_save_cursor_clock(dongle_storage *sto,
    dongle_config_t *cfg)
{
  dongle_storage_save_config(sto, cfg);
  nvm3_save_clock_cursor(sto, cfg);
}

#if 0
void dongle_storage_load_otp(dongle_storage *sto, int i, dongle_otp_t *otp)
{
  _flash_read_(OTP(i), otp, sizeof(dongle_otp_t));
}

void dongle_storage_save_otp(dongle_storage *sto, otp_set otps)
{
  storage_addr_t off = OTP(0);
  pre_erase(sto, off, (NUM_OTP * sizeof(dongle_otp_t)));

#define write(data, size) \
  (_flash_write_(off, data, size), off += size)

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
      _flash_write_(OTP(i), &otp, sizeof(dongle_otp_t));
      return i;
    }
  }
  return -1;
}
#endif

static inline void inc_head(dongle_storage *sto)
{
  sto->encounters.head = (sto->encounters.head + 1) % MAX_LOG_COUNT;
}

static inline void inc_tail(dongle_storage *sto)
{
  sto->encounters.tail = (sto->encounters.tail + 1) % MAX_LOG_COUNT;
}

static inline int inc_idx(int idx)
{
  idx = ((idx + 1) % MAX_LOG_COUNT);
  return idx;
}

static inline void _log_increment_(dongle_storage *sto)
{
  inc_head(sto);
  // FORCED_DELETION
  // If the head catches, the tail, can either opt to block or delete.
  // We delete since newer records are preferred.
  if (sto->encounters.head == sto->encounters.tail) {
    log_errorf("Encounter storage full; idx=%lu\r\n", sto->encounters.head);
    inc_tail(sto);
  }
}

enctr_entry_counter_t dongle_storage_num_encounters_current(dongle_storage *sto)
{
  enctr_entry_counter_t result;
  if (sto->encounters.head >= sto->encounters.tail) {
    result = sto->encounters.head - sto->encounters.tail;
  } else {
    result = MAX_LOG_COUNT - (sto->encounters.tail - sto->encounters.head);
  }
  return result;
}

// Adjust encounter indexing to ensure that the oldest log entry satisfies the
// age criteria
void _delete_old_encounters_(dongle_storage *sto, dongle_timer_t cur_time)
{
  dongle_encounter_entry_t en;
  enctr_entry_counter_t i = 0;
  enctr_entry_counter_t num = dongle_storage_num_encounters_current(sto);
#define age ((int) (cur_time - (en.dongle_time_start + en.dongle_time_int)))
  do {
    if (i >= num) {
        break;
    }

//    uint32_t old_tail = sto->encounters.tail;
    // tail is updated during loop, so reference first index every time
    dongle_storage_load_single_encounter(sto, 0, &en);

    if (age <= DONGLE_MAX_LOG_AGE)
      break;

    // delete old logs
    inc_tail(sto);
//    log_debugf("[%u] age: %u > %u, head: %u, tail: %u -> %u\r\n", i, age,
//        DONGLE_MAX_LOG_AGE, sto->encounters.head, old_tail, sto->encounters.tail);

    i++;
  } while (sto->encounters.tail != sto->encounters.head);
#undef age
}

void dongle_storage_load_encounter(dongle_storage *sto,
    enctr_entry_counter_t i, dongle_encounter_cb cb)
{
  enctr_entry_counter_t prev_idx;
  enctr_entry_counter_t num = dongle_storage_num_encounters_current(sto);
  log_infof("loading log entries starting at idx %lu cnt: %lu\r\n", i, num);
  dongle_encounter_entry_t en;
  do {
    if (i >= num)
      break;

    dongle_storage_load_single_encounter(sto, i, &en);

    prev_idx = i;
    i = inc_idx(i);
  } while (cb(prev_idx, &en));
}

void dongle_storage_load_all_encounter(dongle_storage *sto, dongle_encounter_cb cb)
{
  dongle_storage_load_encounter(sto, 0, cb);
}

/*
 * caller must check that i < num encounters currently stored
 */
void dongle_storage_load_single_encounter(dongle_storage *sto,
    enctr_entry_counter_t i, dongle_encounter_entry_t *en)
{
  storage_addr_t off = ENCOUNTER_LOG_OFFSET(sto, i);
  _flash_read_(off, en, sizeof(dongle_encounter_entry_t));
}

#if 0
void dongle_storage_load_encounters_from_time(dongle_storage *sto,
    dongle_timer_t min_time, dongle_encounter_cb cb)
{
  log_debugf("loading log entries starting at time %lu\r\n", min_time);
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
#endif

void dongle_storage_log_encounter(dongle_storage *sto, dongle_config_t *cfg,
		dongle_timer_t *dongle_time, dongle_encounter_entry_t *en)
{
//  enctr_entry_counter_t num1, num2, num3;
//  num1 = dongle_storage_num_encounters_current(sto);
  storage_addr_t off = ENCOUNTER_LOG_OFFSET(sto, sto->encounters.head);

  // NOTE: once head has wrapped, erasing the next page before writing
  // will cause a page of old entries to be lost (204 entries)
  // currently, storing the old page in an in-memory buffer does not work
  // because allocating an 8K static or dynamic memory buffer is not
  // possible
  pre_erase(sto, off, ENCOUNTER_ENTRY_SIZE);


#define write(data, size) _flash_write_(off, data, size), off += size

  write(en, ENCOUNTER_ENTRY_SIZE);

#undef write

  sto->total_encounters++;
  _log_increment_(sto);
//  num2 = dongle_storage_num_encounters_current(sto);
  _delete_old_encounters_(sto, *dongle_time);
//  num3 = dongle_storage_num_encounters_current(sto);
#if 0
  log_debugf("curr time: %u, en end time: %u, %u #entries: %lu -> %lu -> %lu, "
      "H: %lu, T: %lu, off: %u, size: %u %u\r\n",
      *dongle_time, en->dongle_time_start+en->dongle_time_int,
      last_stat_time, num1, num2, num3,
      sto->encounters.head, sto->encounters.tail,
      start, off - start, ENCOUNTER_ENTRY_SIZE);
#endif
  // save head and tail to flash
  cfg->en_head = sto->encounters.head;
  cfg->en_tail = sto->encounters.tail;
  dongle_storage_save_cursor_clock(sto, cfg);
}

void dongle_storage_save_stat(dongle_storage *sto, dongle_config_t *cfg,
    void * stat, size_t len)
{
  int total_size = sizeof(dongle_config_t) + (NUM_OTP*sizeof(dongle_otp_t))
    + sizeof(dongle_stats_t);

  dongle_otp_t otps[NUM_OTP];
  _flash_read_(OTP(0), otps, NUM_OTP*sizeof(dongle_otp_t));

  pre_erase(sto, sto->map.config, total_size);

  _flash_write_(sto->map.config, cfg, sizeof(dongle_config_t));
  _flash_write_(sto->map.otp, otps, NUM_OTP*sizeof(dongle_otp_t));
  _flash_write_(sto->map.stat, stat, len);
}

void dongle_storage_read_stat(dongle_storage *sto, void * stat, size_t len)
{
  _flash_read_(sto->map.stat, stat, len);
}

void dongle_storage_clean_log(dongle_storage *sto, dongle_timer_t cur_time)
{
  _delete_old_encounters_(sto, cur_time);
}

#undef next_multiple
