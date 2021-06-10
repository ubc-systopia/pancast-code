//
// Dongle Application for PanCast Beacon logging
// Early version based on the bluetooth-central example from Zephyr
// This acts as a passive scanner that makes no connections and
// instead interprets and logs payload data from PanCast beacons.
//

#define LOG_LEVEL__DEBUG
//#define MODE__TEST

#include <zephyr.h>
#include <sys/printk.h>
#include <sys/util.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <drivers/flash.h>

#include "./dongle.h"

#include "../../common/src/pancast.h"
#include "../../common/src/util.h"
#include "../../common/src/test.h"
#include "../../common/src/log.h"

#define DONGLE_REPORT_INTERVAL 30

// number of distinct broadcast ids to keep track of at one time
#define DONGLE_MAX_BC_TRACKED 16


void main(void)
{
	log_infof("Starting %s on %s\n", CONFIG_BT_DEVICE_NAME, CONFIG_BOARD);

	int err;

	err = bt_enable(NULL);
	if (err) {
		log_errorf("Bluetooth init failed (err %d)\n", err);
		return;
	}

	log_info("Bluetooth initialized\n");

	dongle_scan();
}

//
// GLOBAL MEMORY
//

// Mutual exclusion
// Access for both central updates and scan interupts
// is given on a FIFO basis
struct k_mutex dongle_mu;
#define LOCK k_mutex_lock(&dongle_mu, K_FOREVER);
#define UNLOCK k_mutex_unlock(&dongle_mu);

// System Config
size_t                      flash_min_block_size;
int                         flash_num_pages;
size_t                      flash_page_size;
#define NV_STATE (FLASH_OFFSET + flash_page_size)   // offset for flash state
#define NV_LOG (NV_STATE + flash_page_size)
#define ENCOUNTER_LOG_OFFSET(i) (NV_LOG + (i * ENCOUNTER_ENTRY_SIZE))

// Config
dongle_id_t                 dongle_id;
dongle_timer_t              t_init;
key_size_t                  backend_pk_size;        // size of backend public key
pubkey_t                    backend_pk;             // Backend public key
key_size_t                  dongle_sk_size;         // size of secret key
seckey_t                    dongle_sk;              // Secret Key

// Op
struct device              *flash;
dongle_epoch_counter_t      epoch;
struct k_timer              kernel_time;
dongle_timer_t              dongle_time;								// main dongle timer
dongle_timer_t              report_time;
beacon_eph_id_t             cur_id[DONGLE_MAX_BC_TRACKED];			    // currently observed ephemeral id
dongle_timer_t              obs_time[DONGLE_MAX_BC_TRACKED];			// time of last new id observation
size_t                      cur_id_idx;
enctr_entry_counter_t       enctr_entries;

// Reporting
enctr_entry_counter_t       enctr_entries_offset;

#ifdef MODE__TEST
int                         test_encounters;
#endif


static int decode_payload(uint8_t *data)
{
    data[0] = data[1];
    data[1] = data[MAX_BROADCAST_SIZE - 1];
    return 0;
}

// matches the schema defined by encode
static int decode_encounter(encounter_broadcast_t *dat, encounter_broadcast_raw_t *raw)
{
    uint8_t *src = (uint8_t*) raw;
    size_t pos = 0;
#define link(dst, type) (dst = (type*) (src + pos)); pos += sizeof(type)
    link(dat->t, beacon_timer_t);
	link(dat->b, beacon_id_t);
    link(dat->loc, beacon_location_id_t);
	link(dat->eph, beacon_eph_id_t);
#undef link
    return 0;
}

// compares two ephemeral ids
static int ephcmp(beacon_eph_id_t *a, beacon_eph_id_t *b)
{
#define A (a -> bytes)
#define B (b -> bytes)
	for (int i = 0; i < BEACON_EPH_ID_HASH_LEN; i++)
		if (A[i] != B[i])
				return 1;
#undef B
#undef A
	return 0;
}

static void _dongle_encounter_(encounter_broadcast_t *enc, size_t i)
{
#define en (*enc)
// when a valid encounter is detected
// log the encounter
    log_debugf("Beacon Encounter (id=%u, t_b=%u, t_d=%u)\n", *en.b, *en.t, dongle_time);
// Write to storage
// Starting point is determined statically
    off_t off = 0;
#define base ENCOUNTER_LOG_OFFSET(enctr_entries)
#define align off += (flash_min_block_size - (off % flash_min_block_size))
#define write(data, size) (flash_write(flash, base + off, data, size) ? \
                                log_error("Error writing flash\n") : NULL), \
                                off += size, align
    write(en.b, sizeof(beacon_id_t));
    write(en.loc, sizeof(beacon_location_id_t));
    write(en.t, sizeof(beacon_timer_t));
    write(&dongle_time, sizeof(dongle_timer_t));
    write(en.eph, BEACON_EPH_ID_SIZE);
#undef write
#undef align
#undef base
    enctr_entries++;
#ifdef MODE__TEST
    if (*en.b == TEST_BEACON_ID) {
        test_encounters++;
    }
#endif
// reset the observation time
    obs_time[i] = dongle_time;
#undef en
}

static void _dongle_track_(encounter_broadcast_t *enc)
{
#define en (*enc)
// determine which tracked id, if any, is a match
	size_t i = DONGLE_MAX_BC_TRACKED;
	for (size_t j = 0; j < DONGLE_MAX_BC_TRACKED; j++) {
		if (!ephcmp(en.eph, &cur_id[j])) {
			i = j;
		}
	}
	if (i == DONGLE_MAX_BC_TRACKED) {
// if no match was found, start tracking the new id, replacing the oldest one
// currently tracked
		i = cur_id_idx;
		log_debugf("new ephemeral id observed (beacon=%u), tracking at index %d\n", *en.b, i);
		print_bytes(en.eph -> bytes, BEACON_EPH_ID_HASH_LEN, "eph_id");
		cur_id_idx = (cur_id_idx + 1) % DONGLE_MAX_BC_TRACKED;
		memcpy(&cur_id[i], en.eph -> bytes, BEACON_EPH_ID_HASH_LEN);
		obs_time[i] = dongle_time;
	} else {
// when a matching ephemeral id is observed
// check conditions for a valid encounter
		dongle_timer_t dur = dongle_time - obs_time[i];
		if (dur >= DONGLE_ENCOUNTER_MIN_TIME) {
            _dongle_encounter_(&en, i);
		}
	}
#undef en
}

static void dongle_log(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	char addr_str[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
// Filter mis-sized packets
    if (ad -> len != ENCOUNTER_BROADCAST_SIZE + 1) {
        return;
    }
    decode_payload(ad -> data);
    LOCK
    encounter_broadcast_t en;
    decode_encounter(&en, (encounter_broadcast_raw_t*) ad -> data);
    _dongle_track_(&en);
    UNLOCK
}

static void _dongle_report_()
{
    // do report
    if (dongle_time - report_time >= DONGLE_REPORT_INTERVAL) {
        report_time = dongle_time;
        log_infof("*** Begin Report for %s ***\n", CONFIG_BT_DEVICE_NAME);
        log_infof("ID: %u\n", dongle_id);
        log_infof("Initial clock: %u\n", t_init);
        log_infof("Backend public key (%u bytes)\n", backend_pk_size);
        log_infof("Secret key (%u bytes)\n", dongle_sk_size);
        log_infof("dongle timer: %u\n", dongle_time);
// Report logged encounters
        log_info("Encounters logged:\n");
            log_info("----------------------------------------------\n");
            log_info("< Time (dongle), Beacon ID, Time (beacon) >   \n");
            log_info("----------------------------------------------\n");
        dongle_timer_t enctr_time;
        beacon_id_t    enctr_beacon;
        beacon_timer_t enctr_beacon_time;
        for (int i = enctr_entries_offset; i < enctr_entries; i++) {
            off_t off = 0;
#define align off += (flash_min_block_size - (off % flash_min_block_size))
#define seek(size) off += size, align
#define base ENCOUNTER_LOG_OFFSET(i)
#define read(size, dst) flash_read(flash, base + off, dst, size), off += size, align
            read(sizeof(beacon_id_t), &enctr_beacon);
            seek(sizeof(beacon_location_id_t));
            read(sizeof(beacon_timer_t), &enctr_beacon_time);
            read(sizeof(dongle_timer_t), &enctr_time);
            seek(BEACON_EPH_ID_SIZE);
#undef read
#undef base
#undef seek
#undef align
            log_infof("< %u, %u, %u >\n",
                enctr_time, enctr_beacon, enctr_beacon_time);
        }
        enctr_entries_offset = enctr_entries;
#ifdef MODE__TEST
        int err = 0;
        if (test_encounters < 1) {
            log_infof("FAILED: Encounter test. encounters logged in window: %d\n", 
                        test_encounters);
            err++;
        }
        log_infof("Tests completed: status = %d\n", err);
        test_encounters = 0;
#endif
        log_info(   "*** End Report ***\n");
    }
}

static void _dongle_load_()
{
#ifdef MODE__TEST
    dongle_id = TEST_DONGLE_ID;
    t_init = TEST_DONGLE_INIT_TIME;
#else
    off_t off = 0;
#define read(size, dst) (flash_read(flash, FLASH_OFFSET + off, dst, size), off += size)
    read(sizeof(dongle_id_t), &dongle_id);
    read(sizeof(dongle_timer_t), &t_init);
    read(sizeof(key_size_t), &backend_pk_size);
    read(backend_pk_size, &backend_pk);
    read(sizeof(key_size_t), &dongle_sk_size);
    read(dongle_sk_size, &dongle_sk);
#undef read
#endif
}

static bool _flash_page_info_(const struct flash_pages_info *info, void*data)
{
    if (!flash_num_pages) {
        flash_page_size = info->size;
    } else if (info->size != flash_page_size) {
        log_errorf("differing page sizes! (%u and %u)\n", info->size, flash_page_size);
    }
    flash_num_pages++;
    return true;
}

static void _dongle_init_flash_()
{
    flash = device_get_binding(DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL);
    log_info("Getting flash information.\n");
    flash_min_block_size = flash_get_write_block_size(flash);
    flash_num_pages = 0;
    flash_page_foreach(flash, _flash_page_info_, NULL);
    log_infof("Pages: %d, size=%u\n", flash_num_pages, flash_page_size);
// Check if the state area of memory has been written (i.e. whether this
// is a non-first boot)
#define flash_check_t uint64_t
#define FLASH_CHECK 0x0011002200330044
    flash_check_t check;
    flash_read(flash, NV_STATE, &check, sizeof(flash_check_t));
    log_debugf("check: %llx\n", check);
    if (check != FLASH_CHECK) {
        log_info("State flash check failed, erasing since this is first use\n");
        for (off_t off = NV_STATE; 
                off < flash_num_pages * flash_page_size;
                off += flash_page_size) {
            flash_erase(flash, off, flash_page_size);
        }
        check = FLASH_CHECK;
        flash_write(flash, NV_STATE, &check, sizeof(flash_check_t));
    }
#undef FLASH_CHECK
#undef flash_check_t
}

static void _dongle_init_()
{
    k_mutex_init(&dongle_mu);

    _dongle_init_flash_();
    _dongle_load_();

    dongle_time = t_init;
	report_time = dongle_time;
    cur_id_idx = 0;
	epoch = 0;
    enctr_entries = 0;
    enctr_entries_offset = 0;

#ifdef MODE__TEST
    test_encounters = 0;
#endif

	k_timer_init(&kernel_time, NULL, NULL);

// Timer zero point
#define DUR K_MSEC(DONGLE_TIMER_RESOLUTION)
	k_timer_start(&kernel_time, DUR, DUR);
#undef DUR									// Initial time
}

static void dongle_scan(void)
{

    _dongle_init_();

// Scan Start
	int err = err = bt_le_scan_start(BT_LE_SCAN_PARAM(
		BT_LE_SCAN_TYPE_PASSIVE,    // passive scan
		BT_LE_SCAN_OPT_NONE,        // no options; in particular, allow duplicates
		BT_GAP_SCAN_FAST_INTERVAL,  // interval
		BT_GAP_SCAN_FAST_WINDOW     // window
	), dongle_log);
	if (err) {
		log_errorf("Scanning failed to start (err %d)\n", err);
		return;
	} else {
		log_debug("Scanning successfully started\n");
	}

// Dongle Loop
// timing and control logic is largely the same as the beacon
// application. The main difference is that scanning does not
// require restart for a new epoch.

	uint32_t timer_status = 0;

	while (!err) {
// get most updated time
		timer_status = k_timer_status_sync(&kernel_time);
		timer_status += k_timer_status_get(&kernel_time);
        LOCK
        dongle_time += timer_status;
// update epoch
		static dongle_epoch_counter_t old_epoch;
		old_epoch = epoch;
		epoch = epoch_i(dongle_time, t_init);
		if (epoch != old_epoch) {
			log_infof("EPOCH STARTED: %u\n", epoch);
			// TODO: log time to flash
		}
        _dongle_report_();
        UNLOCK
	}
}
