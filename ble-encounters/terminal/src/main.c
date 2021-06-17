#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr.h>
#include <sys/printk.h>
#include <string.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <sys/byteorder.h>

#define LOG_LEVEL__DEBUG
#include "../../common/src/log.h"
#include "../../dongle/src/dongle.h"
#include "../../common/src/util.h"

static void start_scan(void);

static struct bt_conn *default_conn;
static struct bt_uuid_128 SERVICE_UUID = BT_UUID_INIT_128(DONGLE_SERVICE_UUID);
static struct bt_uuid_128 CHARACTERISTIC_UUID = BT_UUID_INIT_128(DONGLE_CHARACTERISTIC_UUID);
static struct bt_uuid_128 uuid = BT_UUID_INIT_128(0);
static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_subscribe_params subscribe_params;

struct k_mutex state_mu;
interact_state state;

BT_GATT_SERVICE_DEFINE(service,
                       // 0. Primary Service Attribute
                       BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(DONGLE_SERVICE_UUID)),
                       // 1. Interaction Data
                       BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(DONGLE_CHARACTERISTIC_UUID), BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_WRITE, (BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), NULL, NULL, &state),
                       // 2. CCC
                       BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
                       //
);

static uint8_t notify_func(struct bt_conn *conn,
                           struct bt_gatt_subscribe_params *params,
                           const void *data, uint16_t length)
{
    if (!data)
    {
        printk("[UNSUBSCRIBED]\n");
        params->value_handle = 0U;
        return BT_GATT_ITER_STOP;
    }

    printk("[NOTIFICATION] data length %u\n", length);
    info_bytes(data, length, "Data");

    k_mutex_lock(&state_mu, K_FOREVER);

    memcpy(&state, data, sizeof(interact_state));

    int locked = state.flags & 0b00000001 >> 0;

    printk("Dongle is %s\n", locked ? "locked" : "unlocked");

    if (locked)
    {
        state.flags = 0b00000000;
    }
    else
    {
        state.flags = 0b00000001;
    }

    bt_gatt_notify(NULL, &service.attrs[1], &state, sizeof(interact_state));

    k_mutex_unlock(&state_mu);

    return BT_GATT_ITER_CONTINUE;
}

static uint8_t discover_func(struct bt_conn *conn,
                             const struct bt_gatt_attr *attr,
                             struct bt_gatt_discover_params *params)
{
    int err;

    if (!attr)
    {
        printk("Discover complete\n");
        (void)memset(params, 0, sizeof(*params));
        return BT_GATT_ITER_STOP;
    }

    printk("[ATTRIBUTE] handle %u\n", attr->handle);

    if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_DECLARE_128(DONGLE_SERVICE_UUID)))
    {
        uuid = CHARACTERISTIC_UUID;
        discover_params.uuid = &uuid.uuid;
        discover_params.start_handle = attr->handle + 1;
        discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

        err = bt_gatt_discover(conn, &discover_params);
        if (err)
        {
            printk("Characteristic Discover failed (err %d)\n", err);
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
            printk("CCC Discover failed (err %d)\n", err);
        }
    }
    else
    {
        subscribe_params.notify = notify_func;
        subscribe_params.value = BT_GATT_CCC_NOTIFY;
        subscribe_params.ccc_handle = attr->handle;

        err = bt_gatt_subscribe(conn, &subscribe_params);
        if (err && err != -EALREADY)
        {
            printk("Subscribe failed (err %d)\n", err);
        }
        else
        {
            printk("[SUBSCRIBED]\n");
        }

        return BT_GATT_ITER_STOP;
    }

    return BT_GATT_ITER_STOP;
}

// Should be using the parse data helper with callback but couldn't get it to work
void parse_data(uint8_t type, uint8_t *data, uint16_t len, bt_addr_le_t *addr)
{
    switch (type)
    {
    case BT_DATA_UUID128_ALL:
        if ((len * 8) != 128U)
        {
            printk("AD malformed\n");
            return;
        }

        print_bytes(data, len, "DATA");

        struct bt_le_conn_param *param;
        int err;

        uint64_t uuid_0, uuid_1, uuid_2, uuid_3, uuid_4;

        memcpy(&uuid_4, data, 6);
        memcpy(&uuid_3, data + 6, 2);
        memcpy(&uuid_2, data + 6 + 2, 2);
        memcpy(&uuid_1, data + 6 + 2 + 2, 2);
        memcpy(&uuid_0, data + 6 + 2 + 2 + 2, 4);

        struct bt_uuid *uuid = BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(uuid_0, uuid_1, uuid_2, uuid_3, uuid_4));

        if (bt_uuid_cmp(uuid, BT_UUID_DECLARE_128(DONGLE_SERVICE_UUID)))
        {
            return;
        }

        printk("Service ID match\n");

        err = bt_le_scan_stop();
        if (err)
        {
            printk("Stop LE scan failed (err %d)\n", err);
            return;
        }

        param = BT_LE_CONN_PARAM_DEFAULT;
        err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
                                param, &default_conn);
        if (err)
        {
            printk("Create conn failed (err %d)\n", err);
            start_scan();
        }

        return;
    default:
        printk("Data type is not 128 UUID\n");
        return;
    }
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                         struct net_buf_simple *ad)
{
    char dev[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(addr, dev, sizeof(dev));
    printk("[DEVICE]: %s, AD evt type %u, AD data len %u, RSSI %i\n",
           dev, type, ad->len, rssi);

    if (strcmp(dev, "D5:AA:E4:57:78:50 (random)"))
    {
        return;
    }

    /* We're only interested in connectable events */
    if (type == BT_GAP_ADV_TYPE_ADV_IND ||
        type == BT_GAP_ADV_TYPE_ADV_DIRECT_IND)
    {
        info_bytes(ad->data, ad->len, "ad data");
        parse_data(ad->data[1], ad->data + 2, ad->data[0] - 1, addr);
        //bt_data_parse(ad, eir_found, (void *)addr);
    }
}

static void start_scan(void)
{
    int err;

    /* Use active scanning and disable duplicate filtering to handle any
	 * devices that might update their advertising data at runtime. */
    struct bt_le_scan_param scan_param = {
        .type = BT_LE_SCAN_TYPE_ACTIVE,
        .options = BT_LE_SCAN_OPT_NONE,
        .interval = BT_GAP_SCAN_FAST_INTERVAL,
        .window = BT_GAP_SCAN_FAST_WINDOW,
    };

    err = bt_le_scan_start(&scan_param, device_found);
    if (err)
    {
        printk("Scanning failed to start (err %d)\n", err);
        return;
    }

    printk("Scanning successfully started\n");
}

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
    char addr[BT_ADDR_LE_STR_LEN];
    int err;

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (conn_err)
    {
        printk("Failed to connect to %s (%u)\n", addr, conn_err);

        bt_conn_unref(default_conn);
        default_conn = NULL;

        start_scan();
        return;
    }

    printk("Connected: %s\n", addr);

    if (conn == default_conn)
    {
        uuid = SERVICE_UUID;
        discover_params.uuid = &uuid.uuid;
        discover_params.func = &discover_func;
        discover_params.start_handle = BT_ATT_FIRST_ATTTRIBUTE_HANDLE;
        discover_params.end_handle = BT_ATT_LAST_ATTTRIBUTE_HANDLE;
        discover_params.type = BT_GATT_DISCOVER_PRIMARY;

        err = bt_gatt_discover(default_conn, &discover_params);
        if (err)
        {
            printk("Discover failed(err %d)\n", err);
            return;
        }
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    printk("Disconnected: %s (reason 0x%02x)\n", addr, reason);

    if (default_conn != conn)
    {
        return;
    }

    bt_conn_unref(default_conn);
    default_conn = NULL;

    start_scan();
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

void main(void)
{
    int err;
    err = bt_enable(NULL);

    if (err)
    {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }

    printk("Bluetooth initialized\n");

    bt_conn_cb_register(&conn_callbacks);

    start_scan();
}