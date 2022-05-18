#define LOG_LEVEL__INFO

#include "storage.h"

#include <string.h>

#include "common/src/platform/gecko.h"
#include "common/src/util/log.h"
#include "common/src/util/util.h"

#define prev_multiple(k, n) ((n) - ((n) % (k)))
#define next_multiple(k, n) ((n) + ((k) - ((n) % (k)))) // buggy

void beacon_storage_erase(beacon_storage *sto, storage_addr_t offset)
{
  log_debugf("erasing page at 0x%x\r\n", (offset));
  MSC_ErasePage((uint32_t *)offset);
  log_debugf("%s", "erased.\r\n");
  sto->numErasures++;
}

void pre_erase(beacon_storage *sto, storage_addr_t off, size_t write_size)
{
  // Erase before write
#define page_num(o) ((o) / sto->page_size)
  if ((off % sto->page_size) == 0) {
    beacon_storage_erase(sto, off);
  } else if (page_num(off + write_size) > page_num(off)) {
    beacon_storage_erase(sto, next_multiple(sto->page_size, off));
  }
#undef page_num
}

int _flash_read_(beacon_storage *sto, storage_addr_t off, void *data, size_t size)
{
  log_debugf("size: %d bytes, addr: 0x%x, flash off: 0x%0x\r\n",
      size, off, sto->map.config);
  memcpy(data, (uint32_t *)off, size);
  return 0;
}

int _flash_write_(beacon_storage *sto, storage_addr_t off, void *data, size_t size)
{
  log_debugf("size: %d bytes, addr: 0x%x, flash off: 0x%0x\r\n",
      size, off, sto->map.config);
  return MSC_WriteWord((uint32_t *)off, data, (uint32_t)size);
}

void beacon_storage_get_info(beacon_storage *sto)
{
  sto->num_pages = FLASH_DEVICE_NUM_PAGES;
  sto->min_block_size = FLASH_DEVICE_BLOCK_SIZE;
  sto->page_size = FLASH_DEVICE_PAGE_SIZE;
  sto->total_size = sto->num_pages * sto->page_size;
}

void beacon_storage_init_device(beacon_storage *sto)
{
  MSC_ExecConfig_TypeDef execConfig = MSC_EXECCONFIG_DEFAULT;
  sto->mscExecConfig = execConfig;
  MSC_ExecConfigSet(&sto->mscExecConfig);
  MSC_Init();
}

void beacon_storage_init(beacon_storage *sto)
{
  log_debugf("%s", "Initializing storage...\r\n");
  sto->numErasures = 0;
  beacon_storage_init_device(sto);
  beacon_storage_get_info(sto);
  sto->map.config = FLASH_OFFSET;
  if (FLASH_OFFSET % sto->page_size != 0) {
    log_errorf("storage start addr %u is not page (%u) aligned!\r\n",
        FLASH_OFFSET, sto->page_size);
  }
//  sto->map.stat = sto->total_size - (3*sto->page_size);
}

// Read data from flashed storage
// Format matches the fixed structure which is also used as a protocol when appending non-app
// data to the device image.
void beacon_storage_load_config(beacon_storage *sto, beacon_config_t *cfg)
{
  log_debugf("%s", "Loading config...\r\n");
  storage_addr_t off = sto->map.config;
#define read(size, dst) (_flash_read_(sto, off, dst, size), off += size)
  read(sizeof(beacon_id_t), &cfg->beacon_id);
  read(sizeof(beacon_location_id_t), &cfg->beacon_location_id);
  read(sizeof(beacon_timer_t), &cfg->t_init);
  read(sizeof(beacon_timer_t), &cfg->t_cur);
  read(sizeof(key_size_t), &cfg->backend_pk_size);
  log_debugf("bknd key off: %u, size: %u\r\n", off, cfg->backend_pk_size);
  if (cfg->backend_pk_size > PK_MAX_SIZE) {
    log_errorf("Key size read for backend pubkey (%u > %u)\r\n",
        cfg->backend_pk_size, PK_MAX_SIZE);
    cfg->backend_pk_size = PK_MAX_SIZE;
  }
  read(cfg->backend_pk_size, &cfg->backend_pk);
  hexdumpn(cfg->backend_pk.bytes, 16, "   Server PK");
  // slide through the extra space for a pubkey
  off += PK_MAX_SIZE - cfg->backend_pk_size;

  read(sizeof(key_size_t), &cfg->beacon_sk_size);
  log_debugf("beacon key off: %u, size: %u\r\n", off, cfg->beacon_sk_size);
  if (cfg->beacon_sk_size > SK_MAX_SIZE) {
    log_errorf("Key size read for beacon privkey (%u > %u)\r\n",
        cfg->beacon_sk_size, SK_MAX_SIZE);
    cfg->beacon_sk_size = SK_MAX_SIZE;
  }
  read(cfg->beacon_sk_size, &cfg->beacon_sk);
  hexdumpn(cfg->beacon_sk.bytes, 16, "Si Beacon SK");
  // slide through the extra space for a pubkey
  off += SK_MAX_SIZE - cfg->beacon_sk_size;

  read(sizeof(test_filter_size_t), &sto->test_filter_size);
  if (sto->test_filter_size != TEST_FILTER_LEN) {
      log_errorf("Test filter length mismatch (%u != %u)\r\n",
          sto->test_filter_size, TEST_FILTER_LEN);
      sto->test_filter_size = TEST_FILTER_LEN;
  }
  sto->map.test_filter = off;

  /*
   * place stats on a separate page that can be repeatedly erased
   * and written to in order to ensure persistence of stats correctly.
   */
  sto->map.stat = next_multiple(sto->page_size,
      sto->map.test_filter + sto->test_filter_size);
#undef read
}

void beacon_storage_save_config(beacon_storage *sto, beacon_config_t *cfg)
{
  storage_addr_t off = sto->map.config;
  int total_size = sizeof(beacon_config_t) +
    sizeof(test_filter_size_t);

  // erase from the beginning of config page
  pre_erase(sto, off, total_size);

#define write(data, size) \
  (_flash_write_(sto, off, data, size), off += size)

  /*
   * write config to flash
   *
   * XXX: for some reason, using write(cfg, sizeof(beacon_config_t))
   * does not store the config correctly to flash.
   */
  write(&cfg->beacon_id, sizeof(beacon_id_t));
  write(&cfg->beacon_location_id, sizeof(beacon_location_id_t));
  write(&cfg->t_init, sizeof(beacon_timer_t));
  write(&cfg->t_cur, sizeof(beacon_timer_t));
  write(&cfg->backend_pk_size, sizeof(key_size_t));
  write(&cfg->backend_pk, cfg->backend_pk_size);
  off += PK_MAX_SIZE - cfg->backend_pk_size;
  write(&cfg->beacon_sk_size, sizeof(key_size_t));
  write(&cfg->beacon_sk, cfg->beacon_sk_size);
  off += SK_MAX_SIZE - cfg->beacon_sk_size;

  // write test filter len
  write(&sto->test_filter_size, sizeof(test_filter_size_t));

}


void beacon_storage_save_stat(beacon_storage *sto, void *stat, size_t len)
{
  beacon_storage_erase(sto, sto->map.stat);
  storage_addr_t off = sto->map.stat;
  _flash_write_(sto, off, stat, len);
}

void beacon_storage_read_stat(beacon_storage *sto, void *stat, size_t len)
{
  _flash_read_(sto, sto->map.stat, stat, len);
}

void beacon_storage_read_test_filter(beacon_storage *sto, uint8_t *buf)
{
  _flash_read_(sto, sto->map.test_filter, buf, sto->test_filter_size);
}

#undef next_multiple
#undef prev_multiple
