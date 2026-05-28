/*
 * Copyright (c) 2026 badjeff
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_bq25186_bas

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/charger.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/bluetooth/services/bas.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bq25186_bas, CONFIG_CHARGER_LOG_LEVEL);

#define LED_NODE DT_NODELABEL(charger_status_led)
static const struct led_dt_spec status_led = LED_DT_SPEC_GET(LED_NODE);

struct bq25186_bas_config {
    const struct device *chgr_dev;
	const struct gpio_dt_spec irq_gpio;
};

struct bq25186_bas_data {
    const struct device *dev;
    struct gpio_callback irq_gpio_cb;
    struct k_work read_work;
};

static void bq25186_bas_set_interrupt(const struct device *dev, const bool en) {
    const struct bq25186_bas_config *config = dev->config;
    int ret = gpio_pin_interrupt_configure_dt(&config->irq_gpio, en 
                                               ? GPIO_INT_EDGE_FALLING
                                               : GPIO_INT_DISABLE);
    if (ret < 0) {
        LOG_ERR("Cannot set interrupt: %s", en ? "ENABLE" : "DISABLE");
    }
}

static void bq25186_bas_set_status_led(const struct device *dev, const bool en) {
    if (led_is_ready_dt(&status_led)) {
        if (en) {
            led_on_dt(&status_led);
        } else {
            led_off_dt(&status_led);
        }
    }
}

static void bq25186_bas_work_handler(struct k_work *work) {
    struct bq25186_bas_data *data = CONTAINER_OF(work, struct bq25186_bas_data, read_work);
    const struct device *dev = data->dev;
    const struct bq25186_bas_config *config = dev->config;
    int ret = 0;
    union charger_propval val;

    bq25186_bas_set_interrupt(dev, false);

    ret = charger_get_prop(config->chgr_dev, CHARGER_PROP_ONLINE, &val);
    if (ret < 0) {
        LOG_ERR("Connot get prop CHARGER_PROP_ONLINE from charger: %d", ret);
        return;
    }
    if (!val.online) {
        LOG_DBG("External supply not present. BQ2518X_STAT0_VIN_PGOOD_STAT != 0");
        LOG_DBG("set BAS charging state: DISCHARGING");
        bt_bas_bls_set_battery_charge_state(BT_BAS_BLS_CHARGE_STATE_DISCHARGING_ACTIVE);
        bq25186_bas_set_interrupt(dev, true);
        return;
    }

    ret = charger_get_prop(config->chgr_dev, CHARGER_PROP_STATUS, &val);
    if (ret < 0) {
        LOG_ERR("Connot get prop CHARGER_PROP_STATUS from charger: %d", ret);
        bq25186_bas_set_interrupt(dev, true);
        return;
    }
    LOG_DBG("status: %d", val.status);

    bool led_ready = led_is_ready_dt(&status_led);
    if (led_ready) {
        switch (val.status) {
            case CHARGER_STATUS_CHARGING:
                LOG_DBG("set charging status led: ON");
                bq25186_bas_set_status_led(dev, true);
                break;
            case CHARGER_STATUS_NOT_CHARGING:
            case CHARGER_STATUS_FULL:
            default:
                LOG_DBG("set charging status led: OFF");
                bq25186_bas_set_status_led(dev, false);
                break;
        }
    }

#if IS_ENABLED(CONFIG_BT_BAS_BLS)
    switch (val.status) {
        case CHARGER_STATUS_CHARGING:
            LOG_DBG("set BAS charging state: CHARGING");
            bt_bas_bls_set_battery_charge_state(BT_BAS_BLS_CHARGE_STATE_CHARGING);
            break;
        case CHARGER_STATUS_NOT_CHARGING:
            LOG_DBG("set BAS charging state: DISCHARGING");
            bt_bas_bls_set_battery_charge_state(BT_BAS_BLS_CHARGE_STATE_DISCHARGING_ACTIVE);
            break;
        case CHARGER_STATUS_FULL:
            LOG_DBG("set BAS charging state: FULL");
            bt_bas_bls_set_battery_charge_state(BT_BAS_BLS_CHARGE_STATE_DISCHARGING_ACTIVE);
            break;
        default:
            LOG_DBG("set BAS charging state: UNKNOWN");
            bt_bas_bls_set_battery_charge_state(BT_BAS_BLS_CHARGE_STATE_UNKNOWN);
            break;
    }
#else
    LOG_DBG("got prop CHARGER_PROP_STATUS: %d", val.status);
#endif /* IS_ENABLED(CONFIG_BT_BAS_BLS) */

    bq25186_bas_set_interrupt(dev, true);
}

static void bq25186_bas_irq_gpio_cb(const struct device *gpiob, struct gpio_callback *cb,
                                  uint32_t pins) {
    struct bq25186_bas_data *data = CONTAINER_OF(cb, struct bq25186_bas_data, irq_gpio_cb);
    // const struct device *dev = data->dev;
    k_work_submit(&data->read_work);
}

static int bq25186_bas_init_irq(const struct device *dev) {
    int err;
    struct bq25186_bas_data *data = dev->data;
    const struct bq25186_bas_config *config = dev->config;

    // check readiness of irq gpio pin
    if (!device_is_ready(config->irq_gpio.port)) {
        LOG_ERR("IRQ GPIO device not ready");
        return -ENODEV;
    }

    // init the irq pin
    err = gpio_pin_configure_dt(&config->irq_gpio, GPIO_INPUT);
    if (err) {
        LOG_ERR("Cannot configure IRQ GPIO");
        return err;
    }

    // setup and add the irq callback associated
    gpio_init_callback(&data->irq_gpio_cb, bq25186_bas_irq_gpio_cb, BIT(config->irq_gpio.pin));

    err = gpio_add_callback(config->irq_gpio.port, &data->irq_gpio_cb);
    if (err) {
        LOG_ERR("Cannot add IRQ GPIO callback");
    }

    return err;
}

static int bq25186_bas_init(const struct device *dev) {
    // const struct bq25186_bas_config *config = dev->config;
    struct bq25186_bas_data *data = dev->data;
    int ret;

    data->dev = dev;

    ret = bq25186_bas_init_irq(dev);
    if (ret < 0) {
        LOG_ERR("Failed to init irq gpio: %d", ret);
        return -EIO;
    }
    k_work_init(&data->read_work, bq25186_bas_work_handler);
    bq25186_bas_set_interrupt(dev, true);

    // charger_charge_enable(config->chgr_dev, true);

    LOG_INF("bq25186_bas driver initialized successfully");
    return 0;
}

#define CONFIG_ZMK_BQ25186_INIT_PRIORITY CONFIG_CHARGER_INIT_PRIORITY

#define ZMK_BQ25186_INST(n)                                                        \
    static struct bq25186_bas_data bq25186_bas_data_##n;                           \
    static const struct bq25186_bas_config bq25186_bas_config_##n = {              \
        .chgr_dev = DEVICE_DT_GET(DT_INST_PHANDLE(n, charger_device)),             \
        .irq_gpio = GPIO_DT_SPEC_INST_GET(n, irq_gpios),                           \
    };                                                                             \
    DEVICE_DT_INST_DEFINE(n, bq25186_bas_init, NULL,                               \
                          &bq25186_bas_data_##n, &bq25186_bas_config_##n,          \
                          POST_KERNEL, CONFIG_ZMK_BQ25186_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(ZMK_BQ25186_INST)
