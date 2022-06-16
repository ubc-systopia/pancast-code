#include "storage.h"
#include "nvm3_lib.h"

#include <string.h>

#include "common/src/platform/gecko.h"
#include "common/src/util/log.h"
#include "common/src/util/util.h"
#include "stats.h"

#define next_multiple(k, n) ((n) + ((k) - ((n) % (k)))) // buggy

#define OTP(i) (DONGLE_OTPSTORE_OFFSET + (i * sizeof(dongle_otp_t)))

extern dongle_stats_t stats;

static inline void dongle_storage_erase(storage_addr_t offset)
{
  int status = MSC_ErasePage((uint32_t *)offset);
  if (status != 0) {
    log_errorf("error erasing page: 0x%x", status);
  }
  stats.numErasures++;
}

// Erase before write
void pre_erase(storage_addr_t off, size_t write_size)
{
#define page_num(o) ((o) / FLASH_DEVICE_PAGE_SIZE)
  if ((off % FLASH_DEVICE_PAGE_SIZE) == 0) {
    dongle_storage_erase(off);
  } else if (page_num(off + write_size) > page_num(off)) {
    dongle_storage_erase(next_multiple(FLASH_DEVICE_PAGE_SIZE, off));
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

static inline void dongle_storage_init_device(void)
{
  MSC_ExecConfig_TypeDef execConfig = MSC_EXECCONFIG_DEFAULT;
  MSC_ExecConfigSet(&execConfig);
  MSC_Init();
}

void dongle_storage_init(void)
{
  dongle_storage_init_device();
  stats.total_encounters = 0;
  stats.numErasures = 0;
  if (FLASH_OFFSET % FLASH_DEVICE_PAGE_SIZE != 0) {
    log_errorf("Storage area start addr %u is not page (%u) aligned!\r\n",
        FLASH_OFFSET, FLASH_DEVICE_PAGE_SIZE);
  }
}

void dongle_storage_load_config(dongle_config_t *cfg)
{
  storage_addr_t off = DONGLE_CONFIG_OFFSET;
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
  read(cfg->backend_pk_size, cfg->backend_pk);
  hexdumpn(cfg->backend_pk->bytes, 16, "   Server PK");
  // slide through the extra space for a pubkey
  off += PK_MAX_SIZE - cfg->backend_pk_size;

  read(sizeof(key_size_t), &cfg->dongle_sk_size);
  log_infof("dongle key off: %u, size: %u\r\n", off, cfg->dongle_sk_size);
  if (cfg->dongle_sk_size > SK_MAX_SIZE) {
    log_errorf("Key size read for dongle privkey (%u > %u)\r\n",
        cfg->dongle_sk_size, SK_MAX_SIZE);
    cfg->dongle_sk_size = SK_MAX_SIZE;
  }
  read(cfg->dongle_sk_size, cfg->dongle_sk);
  hexdumpn(cfg->dongle_sk->bytes, 16, "Si Dongle SK");
  // slide through the extra space for a pubkey
  off += SK_MAX_SIZE - cfg->dongle_sk_size;

  read(sizeof(uint32_t), &cfg->en_tail);
  read(sizeof(uint32_t), &cfg->en_head);

#undef read
}

void dongle_storage_save_config(dongle_config_t *cfg)
{
  storage_addr_t off = DONGLE_CONFIG_OFFSET;
  int total_size = DONGLE_CONFIG_SIZE +
    (NUM_OTP*sizeof(dongle_otp_t)) + sizeof(dongle_stats_t);

  dongle_otp_t otps[NUM_OTP];
  _flash_read_(OTP(0), otps, NUM_OTP*sizeof(dongle_otp_t));

  char statbuf[sizeof(dongle_stats_t)];
  _flash_read_(DONGLE_STATSTORE_OFFSET, statbuf, sizeof(dongle_stats_t));

  pre_erase(off, total_size);

#define write(data, size) \
  (_flash_write_(off, data, size), off += size)

  write(&cfg->id, sizeof(dongle_id_t));
  write(&cfg->t_init, sizeof(dongle_timer_t));
  write(&cfg->t_cur, sizeof(dongle_timer_t));
  write(&cfg->backend_pk_size, sizeof(key_size_t));
  write(cfg->backend_pk, PK_MAX_SIZE);
  write(&cfg->dongle_sk_size, sizeof(key_size_t));
  write(cfg->dongle_sk, SK_MAX_SIZE);
  write(&cfg->en_tail, sizeof(uint32_t));
  write(&cfg->en_head, sizeof(uint32_t));

  // write OTPs
  off = OTP(0);
  write(otps, sizeof(dongle_otp_t)*NUM_OTP);

  // write stats
  off = DONGLE_STATSTORE_OFFSET;
  write(statbuf, sizeof(dongle_stats_t));
#undef write
}

static inline void dongle_storage_save_cursor_clock(dongle_config_t *cfg)
{
//  dongle_storage_save_config(cfg);
  nvm3_save_clock_cursor(cfg);
}

#if 0
void dongle_storage_load_otp(int i, dongle_otp_t *otp)
{
  _flash_read_(OTP(i), otp, sizeof(dongle_otp_t));
}

void dongle_storage_save_otp(otp_set otps)
{
  storage_addr_t off = OTP(0);
  pre_erase(off, (NUM_OTP * sizeof(dongle_otp_t)));

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

int dongle_storage_match_otp(uint64_t val)
{
  dongle_otp_t otp;
  for (int i = 0; i < NUM_OTP; i++) {
    dongle_storage_load_otp(i, &otp);
    if (otp.val == val && !otp_is_used(&otp)) {
      otp.flags &= 0xfffffffffffffffe;
      _flash_write_(OTP(i), &otp, sizeof(dongle_otp_t));
      return i;
    }
  }
  return -1;
}
#endif

static inline int inc_idx(int idx)
{
  idx = ((idx + 1) % MAX_LOG_COUNT);
  return idx;
}

enctr_entry_counter_t num_encounters_current(enctr_entry_counter_t head,
    enctr_entry_counter_t tail)
{
  enctr_entry_counter_t result;
  if (head >= tail) {
    result = head - tail;
  } else {
    result = MAX_LOG_COUNT - (tail - head);
  }
  return result;
}

/*
 * delete oldest [num] encounter entries older than the specified time
 */
void _delete_old_encounters_(dongle_config_t *cfg,
    dongle_timer_t age_threshold, enctr_entry_counter_t num)
{
  dongle_encounter_entry_t en;
  enctr_entry_counter_t i = 0;
#define age ((int) (age_threshold - (en.dongle_time_start + en.dongle_time_int)))
  while ((cfg->en_tail != cfg->en_head) && i < num) {
    // tail is updated during loop, so reference first index every time
    dongle_storage_load_single_encounter(cfg->en_tail, &en);

    // oldest entry is newer than the specified time, no need to iterate further
    if (age <= DONGLE_MAX_LOG_AGE)
      break;

    // delete old logs
    cfg->en_tail = inc_idx(cfg->en_tail);

    i++;
  }
#undef age
}

void dongle_storage_load_encounter(enctr_entry_counter_t i,
    enctr_entry_counter_t num, dongle_encounter_cb cb)
{
  enctr_entry_counter_t prev_idx;
  dongle_encounter_entry_t en;
  do {
    if (i >= num)
      break;

    dongle_storage_load_single_encounter(i, &en);

    prev_idx = i;
    i = inc_idx(i);
  } while (cb(prev_idx, &en));
}

/*
 * caller must check that i < num encounters currently stored
 */
void dongle_storage_load_single_encounter(enctr_entry_counter_t i,
    dongle_encounter_entry_t *en)
{
  storage_addr_t off = ENCOUNTER_LOG_OFFSET(i);
  _flash_read_(off, en, sizeof(dongle_encounter_entry_t));
}

void dongle_storage_log_encounter(dongle_config_t *cfg,
		dongle_timer_t *dongle_time, dongle_encounter_entry_t *en)
{
  enctr_entry_counter_t num = num_encounters_current(cfg->en_head, cfg->en_tail);
  _delete_old_encounters_(cfg, *dongle_time, num);

  storage_addr_t off = ENCOUNTER_LOG_OFFSET(cfg->en_head);

  /*
   * XXX: once head has wrapped, erasing the next page before writing will
   * cause a page of old entries to be lost (204 entries) currently.
   * storing the old page in an in-memory buffer does not work because
   * allocating an 8K static or dynamic memory buffer is not possible.
   */
  pre_erase(off, ENCOUNTER_ENTRY_SIZE);


#define write(data, size) _flash_write_(off, data, size), off += size

  write(en, ENCOUNTER_ENTRY_SIZE);

#undef write

  cfg->en_head = inc_idx(cfg->en_head);

  /*
   * Forced deletion...
   * if the log is full, we can either stop logging further or delete old logs.
   * we delete old logs since newer encounters are preferred.
   */
  if (cfg->en_head == cfg->en_tail) {
    log_errorf("Encounter storage full; idx=%lu\r\n", cfg->en_head);
    cfg->en_tail = inc_idx(cfg->en_tail);
  }

#if 0
  log_debugf("curr time: %u, en end time: %u, %u #entries: %lu, "
      "H: %lu, T: %lu, off: %u, size: %u %u\r\n",
      *dongle_time, en->dongle_time_start+en->dongle_time_int,
      stats.last_report_time, num, cfg->en_head, cfg->en_tail,
      start, off - start, ENCOUNTER_ENTRY_SIZE);
#endif
  stats.total_encounters++;

  dongle_storage_save_cursor_clock(cfg);
}

void dongle_storage_save_stat(dongle_config_t *cfg, void * stat, size_t len)
{
  storage_addr_t off = DONGLE_CONFIG_OFFSET;
  int total_size = DONGLE_CONFIG_SIZE +
    (NUM_OTP*sizeof(dongle_otp_t)) + sizeof(dongle_stats_t);

  dongle_otp_t otps[NUM_OTP];
  _flash_read_(OTP(0), otps, NUM_OTP*sizeof(dongle_otp_t));

  pre_erase(DONGLE_CONFIG_OFFSET, total_size);

#define write(data, size) \
  (_flash_write_(off, data, size), off += size)

  write(&cfg->id, sizeof(dongle_id_t));
  write(&cfg->t_init, sizeof(dongle_timer_t));
  write(&cfg->t_cur, sizeof(dongle_timer_t));
  write(&cfg->backend_pk_size, sizeof(key_size_t));
  write(cfg->backend_pk, PK_MAX_SIZE);
  write(&cfg->dongle_sk_size, sizeof(key_size_t));
  write(cfg->dongle_sk, SK_MAX_SIZE);
  write(&cfg->en_tail, sizeof(uint32_t));
  write(&cfg->en_head, sizeof(uint32_t));

  _flash_write_(DONGLE_OTPSTORE_OFFSET, otps, NUM_OTP*sizeof(dongle_otp_t));
  _flash_write_(DONGLE_STATSTORE_OFFSET, stat, len);
}

void dongle_storage_read_stat(void * stat, size_t len)
{
  _flash_read_(DONGLE_STATSTORE_OFFSET, stat, len);
}

#undef next_multiple
