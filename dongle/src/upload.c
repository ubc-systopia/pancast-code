#include "./upload.h"

#include <string.h>

#ifdef DONGLE_PLATFORM__ZEPHYR
#include <stdio.h>
#include <bluetooth/conn.h>
#include <bluetooth/gatt.h>
#else
#include "sl_bluetooth.h"
#endif

#include "encounter.h"
#include "storage.h"

#define LOG_LEVEL__INFO
#include "common/src/util/log.h"
#include "common/src/util/util.h"

// Memory
#ifdef DONGLE_PLATFORM__ZEPHYR
struct k_mutex state_mu;
#endif
interact_state state;
uint8_t dongle_state;
enctr_entry_counter_t num_recs;
enctr_entry_counter_t next_rec = 0;
dongle_encounter_entry send_en;

#ifdef DONGLE_PLATFORM__ZEPHYR
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL,
                  DONGLE_SERVICE_UUID)};
#endif

// STATES
#define DONGLE_UPLOAD_STATE_LOCKED 0x01
#define DONGLE_UPLOAD_STATE_UNLOCKED 0x02
#define DONGLE_UPLOAD_STATE_SEND_DATA_0 0x03
#define DONGLE_UPLOAD_STATE_SEND_DATA_1 0x04
#define DONGLE_UPLOAD_STATE_SEND_DATA_2 0x05
#define DONGLE_UPLOAD_STATE_SEND_DATA_3 0x06
#define DONGLE_UPLOAD_STATE_SEND_DATA_4 0x07

#ifdef DONGLE_PLATFORM__ZEPHYR
struct bt_conn *terminal_conn = NULL;
static struct bt_uuid_128 SERVICE_UUID = BT_UUID_INIT_128(DONGLE_SERVICE_UUID);
static struct bt_uuid_128 CHARACTERISTIC_UUID = BT_UUID_INIT_128(DONGLE_CHARACTERISTIC_UUID);
static struct bt_uuid_128 uuid = BT_UUID_INIT_128(0);
static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_subscribe_params subscribe_params;

BT_GATT_SERVICE_DEFINE(dongle_service,
                       // 0. Primary Service Attribute
                       BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(DONGLE_SERVICE_UUID)),
                       // 1. Interaction Data
                       BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(DONGLE_CHARACTERISTIC_UUID), BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_WRITE, (BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), NULL, NULL, &state),
                       // 2. CCC
                       BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
                       //
);

static uint8_t _notify_(struct bt_conn *conn,
                        struct bt_gatt_subscribe_params *params,
                        const void *data, uint16_t length)
{
    if (!data)
    {
        log_info("[UNSUBSCRIBED]\r\n");
        params->value_handle = 0U;
        return BT_GATT_ITER_STOP;
    }

    log_debugf("[NOTIFICATION] data length %u\r\n", length);
    print_bytes(data, length, "Data");

    k_mutex_lock(&state_mu, K_FOREVER);
    memcpy(&state, data, sizeof(interact_state));
    k_mutex_unlock(&state_mu);

    interact_update();

    return BT_GATT_ITER_CONTINUE;
}

static uint8_t _discover_(struct bt_conn *conn,
                          const struct bt_gatt_attr *attr,
                          struct bt_gatt_discover_params *params)
{
    int err;

    if (!attr)
    {
        log_debug("Discover complete\r\n");
        (void)memset(params, 0, sizeof(*params));
        return BT_GATT_ITER_STOP;
    }

    log_debugf("[ATTRIBUTE] handle %u\r\n", attr->handle);

    if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_DECLARE_128(DONGLE_SERVICE_UUID)))
    {
        uuid = CHARACTERISTIC_UUID;
        discover_params.uuid = &uuid.uuid;
        discover_params.start_handle = attr->handle + 1;
        discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

        err = bt_gatt_discover(conn, &discover_params);
        if (err)
        {
            log_errorf("Characteristic Discover failed (err %d)\r\n", err);
        }
    }
    else if (!bt_uuid_cmp(discover_params.uuid,
                          BT_UUID_DECLARE_128(DONGLE_CHARACTERISTIC_UUID)))
    {
        memcpy(&uuid, BT_UUID_GATT_CCC, sizeof(uuid));
        discover_params.uuid = &uuid.uuid;
        discover_params.start_handle = attr->handle + 2;
        discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
        subscribe_params.value_handle = bt_gatt_attr_value_handle(attr);

        err = bt_gatt_discover(conn, &discover_params);
        if (err)
        {
            log_errorf("CCC Discover failed (err %d)\r\n", err);
        }
    }
    else
    {
        subscribe_params.notify = _notify_;
        subscribe_params.value = BT_GATT_CCC_NOTIFY;
        subscribe_params.ccc_handle = attr->handle;

        err = bt_gatt_subscribe(conn, &subscribe_params);
        if (err && err != -EALREADY)
        {
            log_errorf("Subscribe failed (err %d)\r\n", err);
        }
        else
        {
            log_info("[SUBSCRIBED]\r\n");
        }

        return BT_GATT_ITER_STOP;
    }

    return BT_GATT_ITER_STOP;
}
#endif

void interact_update()
{
#ifdef DONGLE_PLATFORM__ZEPHYR
    k_mutex_lock(&state_mu, K_FOREVER);
#else
    DONGLE_NO_OP;
#endif
    switch (dongle_state)
    {
    case DONGLE_UPLOAD_STATE_LOCKED:
        log_debug("Dongle is locked\r\n");
        if (state.flags == DONGLE_UPLOAD_DATA_TYPE_ACK_NUM_RECS)
        {
            log_debug("Ack for num recs received\r\n");
            break;
        }
        if (state.flags != DONGLE_UPLOAD_DATA_TYPE_OTP)
        {
            break;
        }
        log_debug("Checking code\r\n");
        dongle_lock();
        dongle_storage *storage = get_dongle_storage();
        int otp_idx = dongle_storage_match_otp(storage,
                                               *((dongle_otp_val *)state.otp.val));
        dongle_unlock();
        if (otp_idx > 0)
        {
            log_debug("Code checks out\r\n");
            dongle_lock();
            num_recs = dongle_storage_num_encounters_current(storage);
            if (num_recs == 0)
            {
                log_debug("No records, re-locking\r\n");
            }
            else
            {
                dongle_state = DONGLE_UPLOAD_STATE_UNLOCKED;
            }
            state.flags = DONGLE_UPLOAD_DATA_TYPE_NUM_RECS;
            memcpy(state.num_recs.val, &num_recs, sizeof(enctr_entry_counter_t));
            dongle_unlock();
            peer_update();
        }
        else
        {
            log_debug("bad code\r\n");
        }
        break;
    case DONGLE_UPLOAD_STATE_UNLOCKED:
    case DONGLE_UPLOAD_STATE_SEND_DATA_4:
        if (state.flags != DONGLE_UPLOAD_DATA_TYPE_ACK_NUM_RECS && state.flags != DONGLE_UPLOAD_DATA_TYPE_ACK_DATA_4)
        {
            break;
        }
        else if (state.flags == DONGLE_UPLOAD_DATA_TYPE_ACK_NUM_RECS)
        {
            log_debug("Ack for num recs received\r\n");
            log_info("Sending records...\r\n");
        }
        if (state.flags == DONGLE_UPLOAD_DATA_TYPE_ACK_DATA_4)
        {
            log_debug("Ack for d4 received\r\n");
            next_rec++;
        }

        if (next_rec >= num_recs)
        {
            log_info("All records sent\r\n");
            dongle_state = DONGLE_UPLOAD_STATE_LOCKED;
            next_rec = 0;
            break;
        }

        dongle_state = DONGLE_UPLOAD_STATE_SEND_DATA_0;

        // load next encounter
        dongle_storage_load_single_encounter(get_dongle_storage(), next_rec, &send_en);
        //_display_encounter_(&send_en);

        state.flags = DONGLE_UPLOAD_DATA_TYPE_DATA_0;
        memcpy(state.data.bytes, &send_en.beacon_id, sizeof(beacon_id_t));
        peer_update();
        break;
    case DONGLE_UPLOAD_STATE_SEND_DATA_0:
        if (state.flags != DONGLE_UPLOAD_DATA_TYPE_ACK_DATA_0)
        {
            break;
        }
        log_debug("Ack for d0 received\r\n");
        dongle_state = DONGLE_UPLOAD_STATE_SEND_DATA_1;

        state.flags = DONGLE_UPLOAD_DATA_TYPE_DATA_1;
        memcpy(state.data.bytes, &send_en.beacon_time, sizeof(beacon_timer_t));
        peer_update();
        break;
    case DONGLE_UPLOAD_STATE_SEND_DATA_1:
        if (state.flags != DONGLE_UPLOAD_DATA_TYPE_ACK_DATA_1)
        {
            break;
        }
        log_debug("Ack for d1 received\r\n");
        dongle_state = DONGLE_UPLOAD_STATE_SEND_DATA_2;

        state.flags = DONGLE_UPLOAD_DATA_TYPE_DATA_2;
        memcpy(state.data.bytes, &send_en.dongle_time, sizeof(dongle_timer_t));
        peer_update();
        break;
    case DONGLE_UPLOAD_STATE_SEND_DATA_2:
        if (state.flags != DONGLE_UPLOAD_DATA_TYPE_ACK_DATA_2)
        {
            break;
        }
        log_debug("Ack for d2 received\r\n");
        dongle_state = DONGLE_UPLOAD_STATE_SEND_DATA_3;

        state.flags = DONGLE_UPLOAD_DATA_TYPE_DATA_3;
        memcpy(state.data.bytes, send_en.eph_id.bytes, BEACON_EPH_ID_SIZE);
        peer_update();
        break;
    case DONGLE_UPLOAD_STATE_SEND_DATA_3:
        if (state.flags != DONGLE_UPLOAD_DATA_TYPE_ACK_DATA_3)
        {
            break;
        }
        log_debug("Ack for d3 received\r\n");
        dongle_state = DONGLE_UPLOAD_STATE_SEND_DATA_4;

        state.flags = DONGLE_UPLOAD_DATA_TYPE_DATA_4;
        memcpy(state.data.bytes, &send_en.location_id, sizeof(beacon_location_id_t));
        peer_update();
        break;
    default:
        log_errorf("No match for state! (%d)\r\n", dongle_state);
    }
#ifdef DONGLE_PLATFORM__ZEPHYR
    k_mutex_unlock(&state_mu);
#else
    DONGLE_NO_OP;
#endif
}

void peer_update()
{
#ifdef DONGLE_PLATFORM__ZEPHYR
    k_mutex_lock(&state_mu, K_FOREVER);
    bt_gatt_notify(NULL, &dongle_service.attrs[1], &state, sizeof(interact_state));
    k_mutex_unlock(&state_mu);
#else
    DONGLE_NO_OP;
#endif
}

#ifdef DONGLE_PLATFORM__ZEPHYR
static void _peer_connected_(struct bt_conn *conn, uint8_t err)
{
    if (err)
    {
        log_infof("Peer connection failed (err 0x%02x)\r\n", err);
    }
    else
    {
        log_info("Peer connected\r\n");
        if (terminal_conn == NULL)
        {
            terminal_conn = conn;
            uuid = SERVICE_UUID;
            discover_params.uuid = &uuid.uuid;
            discover_params.func = &_discover_;
            discover_params.start_handle = BT_ATT_FIRST_ATTTRIBUTE_HANDLE;
            discover_params.end_handle = BT_ATT_LAST_ATTTRIBUTE_HANDLE;
            discover_params.type = BT_GATT_DISCOVER_PRIMARY;

            err = bt_gatt_discover(terminal_conn, &discover_params);
            if (err)
            {
                log_errorf("Discover failed(err %d)\r\n", err);
                return;
            }
        }
    }
}

static void _peer_disconnected_(struct bt_conn *conn, uint8_t reason)
{
    log_infof("Peer disconnected (reason 0x%02x)\r\n", reason);
    terminal_conn = NULL;
    k_mutex_lock(&state_mu, K_FOREVER);
    dongle_state = DONGLE_UPLOAD_STATE_LOCKED;
    k_mutex_unlock(&state_mu);
}

static struct bt_conn_cb conn_callbacks = {
    .connected = _peer_connected_,
    .disconnected = _peer_disconnected_,
};

static void _peer_auth_cancel_(struct bt_conn *conn)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    log_infof("Peer pairing cancelled: %s\r\n", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
    .cancel = _peer_auth_cancel_,
};
#endif

int access_advertise()
{

    dongle_state = DONGLE_UPLOAD_STATE_LOCKED;

#ifdef DONGLE_PLATFORM__ZEPHYR
    print_bytes(ad[0].data, ad[0].data_len, "ad data");
    int err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err)
    {
        log_infof("Advertising failed to start (err %d)\r\n", err);
        return err;
    }

    bt_conn_cb_register(&conn_callbacks);
    bt_conn_auth_cb_register(&auth_cb_display);

    // obtain and report adverisement address
    char addr_s[BT_ADDR_LE_STR_LEN];
    bt_addr_le_t addr = {0};
    size_t count = 1;

    bt_id_get(&addr, &count);
    bt_addr_le_to_str(&addr, addr_s, sizeof(addr_s));

    log_infof("ACCESS: Bluetooth advertising started with address %s\r\n", addr_s);
#else
    sl_status_t sc;
    uint8_t ad_handle = 0x00;
#define INTERVAL 160 // 100 ms per the example
    sc = sl_bt_advertiser_create_set(&ad_handle);
    if (sc) {
        log_errorf("error creating advertising set: 0x%x\r\n", sc);
        return sc;
    }
    sc = sl_bt_advertiser_set_timing(ad_handle,
        INTERVAL,
        INTERVAL, 0, 0);
    if (sc) {
        log_errorf("error setting advertising timing: 0x%x\r\n", sc);
        return sc;
    }
#undef INTERVAL
    sc = sl_bt_advertiser_start(ad_handle,
        advertiser_general_discoverable,
        advertiser_connectable_scannable);
    if (sc) {
        log_errorf("error starting advertising: 0x%x\r\n", sc);
        return sc;
    }
    log_info("Advertising started successfully\r\n");
#endif

    return 0;
}
