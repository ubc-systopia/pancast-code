#include <string.h>

#include <settings.h>
#include <storage.h>
#include <log.h>
#include <util.h>

#define next_multiple(k, n) ((n) + ((k) - ((n) % (k)))) // buggy

static bool _flash_page_info_(const struct flash_pages_info *info, void *data)
{
#define sto ((beacon_storage *) data)
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

void beacon_storage_erase(beacon_storage *sto, storage_addr_t offset)
{
  log_debugf("erasing page at 0x%x\r\n", (offset));
  flash_erase(sto->dev, (offset), sto->page_size);
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
  return flash_read(sto->dev, off, data, size);
}

/*
 * writes must be done in multiples of
 * flash_get_write_block_size(sto-dev),
 * which is 4 for nordic beacons
 */
int _flash_write_(beacon_storage *sto, storage_addr_t off, void *data, size_t size)
{
  int ret = flash_write(sto->dev, off, data, size);
  log_debugf("size: %d bytes, addr: 0x%x, flash off: 0x%0x, ret: %d\r\n",
      size, off, sto->map.config, ret);
  return ret;
}

void beacon_storage_get_info(beacon_storage *sto)
{
  sto->num_pages = 0;
  sto->min_block_size = flash_get_write_block_size(sto->dev);
  flash_page_foreach(sto->dev, _flash_page_info_, sto);
  sto->total_size = sto->num_pages * sto->page_size;
}

void beacon_storage_init_device(beacon_storage *sto)
{
  sto->dev = device_get_binding(DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL);
}

void beacon_storage_init(beacon_storage *sto)
{
  log_debugf("%s", "Initializing storage...\r\n");
  sto->numErasures = 0;
  beacon_storage_init_device(sto);
  beacon_storage_get_info(sto);
  log_infof("#pages: %d, page size: %u, write blk size: %u\r\n",
      sto->num_pages, sto->page_size,
      flash_get_write_block_size(sto->dev));
  sto->map.config = FLASH_OFFSET;
  if (FLASH_OFFSET % sto->page_size != 0) {
    log_errorf("Storage area start addr %u is not page (%u) aligned!\r\n",
        FLASH_OFFSET, sto->page_size);
  }
//  sto->map.stat = sto->total_size - (3*sto->page_size);
}

/*
 * Read data from flashed storage
 * Format matches the fixed structure which is also used as a
 * protocol when appending non-app data to the device image.
 */
void beacon_storage_load_config(beacon_storage *sto, beacon_config_t *cfg)
{
  storage_addr_t off = sto->map.config;
  log_debugf("loading config off: %u\r\n", off);

#define read(size, dst) \
  (_flash_read_(sto, off, dst, size), off += size)

  read(sizeof(beacon_id_t), &cfg->beacon_id);
  read(sizeof(beacon_location_id_t), &cfg->beacon_location_id);
  read(sizeof(beacon_timer_t), &cfg->t_init);
//  read(sizeof(beacon_timer_t), &cfg->t_cur);
  read(sizeof(key_size_t), &cfg->backend_pk_size);
  log_expf("bknd key off: 0x%0x, size: %u\r\n",
      off, cfg->backend_pk_size);
  if (cfg->backend_pk_size > PK_MAX_SIZE) {
    log_errorf("Key size read for backend pubkey (%u > %u)\r\n",
        cfg->backend_pk_size, PK_MAX_SIZE);
    cfg->backend_pk_size = PK_MAX_SIZE;
  }
  read(PK_MAX_SIZE, cfg->backend_pk->bytes);
  hexdumpn(cfg->backend_pk->bytes, 16, "    Server PK");
  // slide through the extra space for a pubkey
//  off += PK_MAX_SIZE - cfg->backend_pk_size;

  read(sizeof(key_size_t), &cfg->beacon_sk_size);
  log_expf("beacon key off: 0x%0x, size: %u\r\n",
      off, cfg->beacon_sk_size);
  if (cfg->beacon_sk_size > SK_MAX_SIZE) {
    log_errorf("Key size read for beacon privkey (%u > %u)\r\n",
        cfg->beacon_sk_size, SK_MAX_SIZE);
    cfg->beacon_sk_size = SK_MAX_SIZE;
  }
  read(SK_MAX_SIZE, cfg->beacon_sk->bytes);
  hexdumpn(cfg->beacon_sk->bytes, 16, "nRF Beacon SK");
  // slide through the extra space for a pubkey
//  off += SK_MAX_SIZE - cfg->beacon_sk_size;

  read(sizeof(sto->test_filter_size), &sto->test_filter_size);

#if MODE__NRF_BEACON_TEST_CONFIG
  if (sto->test_filter_size != TEST_FILTER_LEN) {
      log_errorf("Test filter length mismatch (%u != %u)\r\n",
          sto->test_filter_size, TEST_FILTER_LEN);
      sto->test_filter_size = TEST_FILTER_LEN;
  }
#endif

  sto->map.test_filter = off;

  /*
   * place stats on a separate page that can be repeatedly erased
   * and written to in order to ensure persistence of stats correctly.
   */
  sto->map.stat = next_multiple(sto->page_size,
      sto->map.test_filter + sto->test_filter_size);
  off = sto->map.stat;
  read(sizeof(beacon_timer_t), &cfg->t_cur);
#undef read
  log_debugf("%s", "Config loaded.\r\n");
}

void beacon_storage_save_config(beacon_storage *sto, beacon_config_t *cfg)
{
  storage_addr_t off = sto->map.config;
  int total_size = sizeof(beacon_config_t) +
    sizeof(pubkey_t) + sizeof(beacon_sk_t) +
    sizeof(sto->test_filter_size);

#if 0
  // read out current value from storage
  int test_filter_len = sto->test_filter_size;

  char test_filter[16];
  off += (sizeof(beacon_config_t) + sizeof(sto->test_filter_size));
  log_infof("test filter len: %u\r\n", sto->test_filter_size);
  _flash_read_(sto, off, test_filter, 16);
  hexdumpn(test_filter, 16, "TEST filter pfx");

  off = sto->map.config;
#endif

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
//  write(&cfg->t_cur, sizeof(beacon_timer_t));
  write(&cfg->backend_pk_size, sizeof(key_size_t));
  write(cfg->backend_pk, PK_MAX_SIZE);
//  off += PK_MAX_SIZE - cfg->backend_pk_size;
  write(&cfg->beacon_sk_size, sizeof(key_size_t));
  write(cfg->beacon_sk, SK_MAX_SIZE);
//  off += SK_MAX_SIZE - cfg->beacon_sk_size;

  // write test filter len
  write(&sto->test_filter_size, sizeof(sto->test_filter_size));

#if 0
  // write test filter
  write(test_filter, sto->test_filter_size);
#endif
}

void beacon_storage_save_stat(beacon_storage *sto, beacon_config_t *cfg,
    void *stat, size_t len)
{
  beacon_storage_erase(sto, sto->map.stat);
  storage_addr_t off = sto->map.stat;
  _flash_write_(sto, off, &cfg->t_cur, sizeof(beacon_timer_t));
  off += sizeof(beacon_timer_t);
  _flash_write_(sto, off, stat, len);
}

void beacon_storage_read_stat(beacon_storage *sto, void *stat, size_t len)
{
  _flash_read_(sto, sto->map.stat, stat, len);
}

#undef next_multiple
