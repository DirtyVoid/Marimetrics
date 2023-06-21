#include "globe.h"

NRF_BLE_QWR_DEF(m_qwr);

int ble_tx_power() { return (board_revision() <= BOARD_CONFIG_R4_3) ? 4 : 8; }

struct interface_data ble_interface = {.cur_conn_handle = BLE_CONN_HANDLE_INVALID};

static void device_name_set(const char *name) {
    /* Note: If the device name is located in application flash memory, it
     * cannot be changed. The name is therefore copied into a buffer in RAM. */

#define DEVICE_NAME_MAX_LEN 48
    static char device_name[DEVICE_NAME_MAX_LEN];

    strncpy(device_name, name, DEVICE_NAME_MAX_LEN);
    size_t device_name_len = strnlen(device_name, DEVICE_NAME_MAX_LEN);

    ble_gap_conn_sec_mode_t sec_mode;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    uint32_t err_code = sd_ble_gap_device_name_set(
        &sec_mode, (const uint8_t *)device_name, device_name_len);
    assert(!err_code);
}

/**@brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile)
 * parameters of the device including the device name, appearance, and the preferred
 * connection parameters.
 */
static void gap_params_init() {
    ret_code_t err_code;

    ble_gap_conn_params_t gap_conn_params = {
        .min_conn_interval = MSEC_TO_UNITS(30, UNIT_1_25_MS),
        .max_conn_interval = MSEC_TO_UNITS(30, UNIT_1_25_MS),
        .slave_latency = 30,
        .conn_sup_timeout = MSEC_TO_UNITS(5000, UNIT_10_MS),
    };

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    assert(!err_code);
}

/**@brief Function for initializing the GATT module.
 */
static void gatt_init(void) {
    NRF_BLE_GATT_DEF(m_gatt);
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
    assert(!err_code);
}

/**@brief Function for handling Queued Write Module errors.
 *
 * @details A pointer to this function will be passed to each service which may need to
 * inform the application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void nrf_qwr_error_handler(uint32_t nrf_error) { APP_ERROR_HANDLER(nrf_error); }

#define BLE_CHAR_BANK_CAPACITY 24

static struct ble_char {
    ble_gatts_char_handles_t handles;
    enum ble_char_type {
        BLE_CHAR_UNALLOCATED = 0,
        BLE_CHAR_INT,
        BLE_CHAR_FLOAT,
        BLE_CHAR_STRING,
        BLE_CHAR_BOOL,
    } type;
    void *ble_char;
    union ble_callback {
        void (*i)(struct ble_char_int *, int64_t value);
        void (*f)(struct ble_char_float *, float value);
        void (*s)(struct ble_char_string *, const char *value);
        void (*b)(struct ble_char_bool *, bool value);
    } write_callback;
} ble_char_bank[BLE_CHAR_BANK_CAPACITY];

union char_value {
    uint64_t u;
    int64_t i;
    float f;
};

static union char_value parse_char_value(const uint8_t *data, size_t len) {
    uint64_t res = 0;
    assert(len <= 8);
    for (size_t i = 0; i < len; i++) {
        res += (uint64_t)(data[i]) << (8 * i);
    }
    union char_value v = {.u = res};
    return v;
}

struct ble_write_context {
    struct ble_char *c;
    size_t data_len;
    char data[0];
};

static void write_to_ble_char_task(void *ptr) {
    struct ble_write_context *context = ptr;
    const void *data = context->data;
    size_t len = context->data_len;
    struct ble_char *c = context->c;

    union ble_callback cb = c->write_callback;
    switch (c->type) {
    case BLE_CHAR_INT:
        if (cb.i)
            cb.i(c->ble_char, parse_char_value(data, len).i);
        break;
    case BLE_CHAR_FLOAT:
        if (cb.f)
            cb.f(c->ble_char, parse_char_value(data, len).f);
        break;
    case BLE_CHAR_STRING:
        if (cb.s)
            cb.s(c->ble_char, data);
        break;
    case BLE_CHAR_BOOL:
        if (cb.b)
            cb.b(c->ble_char, parse_char_value(data, len).i != 0);
        break;
    default:
        assert(0 && "invalid characteristic type");
    }

    free(context);
}

static void write_to_ble_char(struct ble_char *c, const void *data, size_t len) {
    /* If the type of the char is a string, allocate an extra byte so it can be
     * null-terminated. */
    size_t data_len = (c->type == BLE_CHAR_STRING) ? len + 1 : len;
    struct ble_write_context *context =
        malloc(sizeof(struct ble_write_context) + data_len);
    context->c = c;
    context->data_len = data_len;
    memcpy(context->data, data, len);
    if (c->type == BLE_CHAR_STRING) {
        context->data[len] = '\0';
    }

    schedule_task(write_to_ble_char_task, context);
}

/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context) {
    (void)p_context;
    const ble_gatts_evt_t *gatts_evt = &p_ble_evt->evt.gatts_evt;
    const ble_gattc_evt_t *gattc_evt = &p_ble_evt->evt.gattc_evt;
    const ble_gap_evt_t *gap_evt = &p_ble_evt->evt.gap_evt;
    ret_code_t err_code = NRF_SUCCESS;

    switch (p_ble_evt->header.evt_id) {
    case BLE_GAP_EVT_DISCONNECTED:
        nus_handle_disconnect();
        notifications_in_progress = 0;
        ble_interface.cur_conn_handle = BLE_CONN_HANDLE_INVALID;
        break;
    case BLE_GAP_EVT_CONNECTED:
        if (notifications_in_progress > 0)
            notifications_in_progress = -1;

        ble_interface.cur_conn_handle = gap_evt->conn_handle;

        err_code = sd_ble_gap_tx_power_set(
            BLE_GAP_TX_POWER_ROLE_CONN, ble_interface.cur_conn_handle, ble_tx_power());
        assert(!err_code);

        err_code =
            nrf_ble_qwr_conn_handle_assign(&m_qwr, ble_interface.cur_conn_handle);
        assert(!err_code);
        break;

    case BLE_GAP_EVT_PHY_UPDATE_REQUEST: {
        ble_gap_phys_t const phys = {
            .rx_phys = BLE_GAP_PHY_AUTO,
            .tx_phys = BLE_GAP_PHY_AUTO,
        };
        err_code = sd_ble_gap_phy_update(gap_evt->conn_handle, &phys);
        assert(!err_code);
    } break;

    case BLE_GATTC_EVT_TIMEOUT:
        // Disconnect on GATT Client timeout event.
        err_code = sd_ble_gap_disconnect(gattc_evt->conn_handle,
                                         BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        assert(!err_code);
        break;

    case BLE_GATTS_EVT_TIMEOUT:
        // Disconnect on GATT Server timeout event.
        err_code = sd_ble_gap_disconnect(gatts_evt->conn_handle,
                                         BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        assert(!err_code);
        break;
    case BLE_GATTS_EVT_WRITE: {
        const ble_gatts_evt_write_t *p_evt_write = &gatts_evt->params.write;
        uint16_t value_handle = p_evt_write->handle;
        for (int char_id = 0; char_id < BLE_CHAR_BANK_CAPACITY; char_id++) {
            struct ble_char *c = &ble_char_bank[char_id];
            if (c->type == BLE_CHAR_UNALLOCATED)
                continue;
            if (c->handles.value_handle == value_handle) {
                write_to_ble_char(c, p_evt_write->data, p_evt_write->len);
                break;
            }
        }
        break;
    }
    case BLE_GATTS_EVT_HVN_TX_COMPLETE: {
        const ble_gatts_evt_hvn_tx_complete_t *e = &gatts_evt->params.hvn_tx_complete;
        notifications_in_progress -= e->count;
        assert(notifications_in_progress >= 0);
        break;
    }
    default:
        // No implementation needed.
        break;
    }
}

/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void) {
    ret_code_t err_code;

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    assert(!err_code);
    extern int __data_start__;
    assert(ram_start <= (uintptr_t)&__data_start__);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    assert(!err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, BLE_NUS_BLE_OBSERVER_PRIO, ble_evt_handler,
                         NULL);
}

void ble_update_label(const char *label) {
    device_name_set(label);
    if (ble_interface.cur_conn_handle == BLE_CONN_HANDLE_INVALID)
        advertising_reinit();
}

static struct ble_char *find_unallocated_ble_char() {
    for (int i = 0; i < BLE_CHAR_BANK_CAPACITY; i++) {
        struct ble_char *c = &ble_char_bank[i];
        if (c->type == BLE_CHAR_UNALLOCATED)
            return c;
    }

    assert(0 && "No unallocated BLE characteristics");
}

static uint8_t ble_char_type_gatt_cpf_format(enum ble_char_type type) {
    switch (type) {
    case BLE_CHAR_INT:
        return BLE_GATT_CPF_FORMAT_SINT64;
    case BLE_CHAR_FLOAT:
        return BLE_GATT_CPF_FORMAT_FLOAT32;
    case BLE_CHAR_STRING:
        return BLE_GATT_CPF_FORMAT_UTF8S;
    case BLE_CHAR_BOOL:
        return BLE_GATT_CPF_FORMAT_BOOLEAN;
    default:
        assert(0 && "invalid BLE type");
    }
}

static uint8_t ble_char_max_len(enum ble_char_type type) {
    switch (type) {
    case BLE_CHAR_INT:
        return 8;
    case BLE_CHAR_FLOAT:
        return 4;
    case BLE_CHAR_STRING:
        return BLE_GATT_ATT_MTU_DEFAULT;
    case BLE_CHAR_BOOL:
        return 1;
    default:
        assert(0 && "invalid BLE type");
    }
}

static struct ble_char *init_ble_char(void *ble_char, union ble_callback callback,
                                      uint16_t uuid, const char *label,
                                      enum ble_char_type type, bool enable_write) {
    struct ble_char *c = find_unallocated_ble_char();
    c->type = type;
    c->write_callback = callback;
    c->ble_char = ble_char;

    ble_gatts_char_pf_t char_pf = {
        .format = ble_char_type_gatt_cpf_format(type),
        .exponent = 0,
        .unit = 0x2700, /* unitless */
    };

    ble_gatts_attr_md_t cccd_md = {
        .vloc = BLE_GATTS_VLOC_STACK,
    };
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);

    size_t label_len = strlen(label);
    ble_gatts_char_md_t char_md = {
        .char_props =
            {
                .read = 1,
                .write = enable_write ? 1 : 0,
                .notify = 1,
            },
        .char_user_desc_max_size = label_len,
        .char_user_desc_size = label_len,
        .p_char_pf = &char_pf,
        .p_char_user_desc = (const uint8_t *)label,
        .p_cccd_md = &cccd_md,
    };
    ble_gatts_attr_md_t attr_md = {
        .vloc = BLE_GATTS_VLOC_STACK,
        .rd_auth = 0,
        .wr_auth = 0,
        .vlen = 0,
    };
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

    ble_uuid_t ble_uuid = {
        .type = ble_interface.uuid_type,
        .uuid = uuid,
    };
    ble_gatts_attr_t attr = {
        .p_uuid = &ble_uuid,
        .p_attr_md = &attr_md,
        .init_len = 0,
        .init_offs = 0,
        .max_len = ble_char_max_len(type),
    };

    uint32_t err_code = sd_ble_gatts_characteristic_add(ble_interface.service_handle,
                                                        &char_md, &attr, &c->handles);
    assert(err_code == NRF_SUCCESS);

    return c;
}

void init_ble_char_int(struct ble_char_int *c, uint16_t uuid, const char *label,
                       void (*write_callback)(struct ble_char_int *, int64_t value)) {
    c->c = init_ble_char(c, (union ble_callback){.i = write_callback}, uuid, label,
                         BLE_CHAR_INT, write_callback != NULL);
}

void init_ble_char_float(struct ble_char_float *c, uint16_t uuid, const char *label,
                         void (*write_callback)(struct ble_char_float *, float value)) {
    c->c = init_ble_char(c, (union ble_callback){.f = write_callback}, uuid, label,
                         BLE_CHAR_FLOAT, write_callback != NULL);
}

void init_ble_char_string(struct ble_char_string *c, uint16_t uuid, const char *label,
                          void (*write_callback)(struct ble_char_string *,
                                                 const char *value)) {
    c->c = init_ble_char(c, (union ble_callback){.s = write_callback}, uuid, label,
                         BLE_CHAR_STRING, write_callback != NULL);
}

void init_ble_char_bool(struct ble_char_bool *c, uint16_t uuid, const char *label,
                        void (*write_callback)(struct ble_char_bool *, bool value)) {
    c->c = init_ble_char(c, (union ble_callback){.b = write_callback}, uuid, label,
                         BLE_CHAR_BOOL, write_callback != NULL);
}

static void set_ble_char(struct ble_char *c, const void *data, size_t len) {

    if (!c)
        return;

    ble_gatts_value_t gatts_value = {
        .len = len,
        .offset = 0,
        .p_value = (void *)data,
    };

    uint16_t value_handle = c->handles.value_handle;

    uint16_t conn;
    {
        CRITICAL_SCOPE();
        conn = ble_interface.cur_conn_handle;
    }

    /* Update the value */
    uint32_t err_code = sd_ble_gatts_value_set(conn, value_handle, &gatts_value);
    assert(!err_code);

    /* Send notification if connected */
    if (conn != BLE_CONN_HANDLE_INVALID) {
        ble_gatts_hvx_params_t hvx_params = {
            .handle = value_handle,
            .type = BLE_GATT_HVX_NOTIFICATION,
            .offset = gatts_value.offset,
            .p_len = &gatts_value.len,
            .p_data = gatts_value.p_value,
        };

        uint8_t cccd_value[2];

        ble_gatts_value_t cccd_gatts_value = {
            .len = 2, .offset = 0, .p_value = cccd_value};

        err_code =
            sd_ble_gatts_value_get(conn, c->handles.cccd_handle, &cccd_gatts_value);
        bool notification_enabled;
        if (err_code == BLE_ERROR_GATTS_SYS_ATTR_MISSING) {
            /*
             * This error may be present immediately after connection prior to
             * sd_ble_gatts_sys_attr_set being called by the bonding manager.
             */
            err_code = NRF_SUCCESS;
            notification_enabled = false;
        } else {
            assert(!err_code);
            notification_enabled = ble_srv_is_notification_enabled(cccd_value);
        }
        if (notification_enabled) {
            do {
                /* If the notification queue is full this will fail with
                 * NRF_ERROR_RESOURCES so we poll until success. */
                err_code = sd_ble_gatts_hvx(conn, &hvx_params);
            } while (err_code == NRF_ERROR_RESOURCES);

            if (err_code == NRF_SUCCESS) {
                CRITICAL_SCOPE();
                assert(notifications_in_progress >= 0);
                notifications_in_progress++;
            }

            if (err_code == NRF_ERROR_INVALID_STATE ||
                err_code == BLE_ERROR_INVALID_CONN_HANDLE) {
                /*
                 * This error may occur during disconnect.
                 */
                err_code = NRF_SUCCESS;
            }
            assert(!err_code);
        }
    }
}

void set_ble_char_int(struct ble_char_int *c, int64_t value) {
    set_ble_char(c->c, &value, sizeof(value));
}

void set_ble_char_float(struct ble_char_float *c, float value) {
    set_ble_char(c->c, &value, sizeof(value));
}

void set_ble_char_string(struct ble_char_string *c, const char *value) {
    char buf[BLE_GATT_ATT_MTU_DEFAULT];
    strncpy(buf, value, array_size(buf));
    buf[array_size(buf) - 1] = '\0';
    set_ble_char(c->c, buf, array_size(buf));
}

void set_ble_char_bool(struct ble_char_bool *c, bool value) {
    uint8_t v = value ? 1 : 0;
    set_ble_char(c->c, &v, sizeof(v));
}

void ble_init() {
    ble_stack_init();

    if (board_revision() <= BOARD_CONFIG_R4_3) {
        static const struct sky66112 sky66112 = {
            .cps_gpio_idx = 6,
            .crx_gpio_idx = 19,
            .ctx_gpio_idx = 17,
        };
        sky66112_init(&sky66112);
    }

    gap_params_init();
    gatt_init();

    ret_code_t err_code;

    // Initialize Queued Write Module.
    nrf_ble_qwr_init_t qwr_init = {.error_handler = nrf_qwr_error_handler};
    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    assert(!err_code);

    struct interface_data *p_uwq = &ble_interface;

    /* Register base UUID */
    static const ble_uuid128_t base_uuid = {
        .uuid128 = {0xdf, 0x58, 0x3c, 0x7d, 0x74, 0x24, 0x4a, 0x89, 0xbf, 0x47, 0xbd,
                    0x00, 0x2a, 0xb7, 0x67, 0x80}};
    err_code = sd_ble_uuid_vs_add(&base_uuid, &p_uwq->uuid_type);
    assert(!err_code);

    /* Register custom service */
    ble_uuid_t ble_uuid = {.type = p_uwq->uuid_type, .uuid = SERVICE_UUID};
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid,
                                        &p_uwq->service_handle);
    assert(!err_code);
}

#define RX_BUFFER_LENGTH 256

static struct {
    bool connected;
    void *context;
    void (*on_uart_connect)(void *context);
    void (*on_uart_disconnect)(void *context);
    void (*on_uart_rx)(void *context, const char *string);

    struct {
        char data[RX_BUFFER_LENGTH];
        int length;
    } rx_buffer;
} handler_data;

void nus_handle_connect() {
    if (!handler_data.connected && handler_data.on_uart_connect) {
        handler_data.connected = true;
        schedule_task(handler_data.on_uart_connect, handler_data.context);
    }
}

void nus_handle_disconnect() {
    if (handler_data.connected && handler_data.on_uart_disconnect) {
        handler_data.connected = false;
        schedule_task(handler_data.on_uart_disconnect, handler_data.context);
    }
}

void on_uart_rx_handler(void *context) {
    (void)context;
    handler_data.on_uart_rx(handler_data.context, handler_data.rx_buffer.data);
    memory_barrier();
    handler_data.rx_buffer.length = 0;
}

void nus_handle_receive_data(const char *data, int length) {
    if (handler_data.connected && handler_data.on_uart_rx) {
        assert(handler_data.rx_buffer.length == 0);
        assert(length < RX_BUFFER_LENGTH);
        memcpy(handler_data.rx_buffer.data, data, length);
        handler_data.rx_buffer.data[length] = '\0';
        handler_data.rx_buffer.length = length;
        schedule_task(on_uart_rx_handler, NULL);
    }
}

void start_ble(void *context, const char *label, void (*on_uart_connect)(void *context),
               void (*on_uart_disconnect)(void *context),
               void (*on_uart_rx)(void *context, const char *string)) {
    handler_data.context = context;
    handler_data.on_uart_connect = on_uart_connect;
    handler_data.on_uart_disconnect = on_uart_disconnect;
    handler_data.on_uart_rx = on_uart_rx;

    device_name_set(label);

    /* Initialize Nordic UART Service */
    nus_init();

    advertising_init();
    conn_params_init();
    peer_manager_init();

    advertising_start(false);
}
