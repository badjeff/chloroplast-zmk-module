/*
 * Copyright (c) 2026 badjeff
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_bq2518x_gpio

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bq2518x_gpio, CONFIG_CHARGER_LOG_LEVEL);

#define BQ2518X_VBAT_CTRL 0x03
#define BQ2518X_SYS_REG   0x0A

struct bq_gpio_config {
    const struct device *i2c_bus;
    uint16_t i2c_addr;
};

static int bq_gpio_pin_configure(const struct device *dev, gpio_pin_t pin, gpio_flags_t flags)
{
    const struct bq_gpio_config *cfg = dev->config;
    if (pin != 0) {
        return -EINVAL;
    }
    uint8_t current_reg_val;
    int ret = i2c_reg_read_byte(cfg->i2c_bus, cfg->i2c_addr, BQ2518X_VBAT_CTRL, &current_reg_val);
    if (ret < 0) {
        return ret;
    }
    /* Set PG_MODE BIT(7): 1b1 = PG_GPO as a general-purpose output pin (GPO) */
    current_reg_val |= BIT(7);
    return i2c_reg_write_byte(cfg->i2c_bus, cfg->i2c_addr, BQ2518X_VBAT_CTRL, current_reg_val);
}

static int bq_gpio_port_set_bits_raw(const struct device *dev, gpio_port_pins_t mask)
{
    const struct bq_gpio_config *cfg = dev->config;
    if (!(mask & BIT(0))) {
        return 0;
    }
    uint8_t current_reg_val;
    int ret = i2c_reg_read_byte(cfg->i2c_bus, cfg->i2c_addr, BQ2518X_SYS_REG, &current_reg_val);
    if (ret < 0) {
        return ret;
    }
    /* Set PG_GPO BIT(4): 1b1 = PG_GPO is low impedance */
    current_reg_val |= BIT(4); 
    return i2c_reg_write_byte(cfg->i2c_bus, cfg->i2c_addr, BQ2518X_SYS_REG, current_reg_val);
}

static int bq_gpio_port_clear_bits_raw(const struct device *dev, gpio_port_pins_t mask)
{
    const struct bq_gpio_config *cfg = dev->config;
    if (!(mask & BIT(0))) {
        return 0;
    }
    uint8_t current_reg_val;    
    int ret = i2c_reg_read_byte(cfg->i2c_bus, cfg->i2c_addr, BQ2518X_SYS_REG, &current_reg_val);
    if (ret < 0) {
        return ret;
    }
    /* Set PG_GPO BIT(4): 1b1 = PG_GPO is high impedance */
    current_reg_val &= ~BIT(4); 
    return i2c_reg_write_byte(cfg->i2c_bus, cfg->i2c_addr, BQ2518X_SYS_REG, current_reg_val);
}

static const struct gpio_driver_api bq_gpio_api = {
    .pin_configure = bq_gpio_pin_configure,
    .port_set_bits_raw = bq_gpio_port_set_bits_raw,
    .port_clear_bits_raw = bq_gpio_port_clear_bits_raw,
};

static int bq_gpio_init(const struct device *dev)
{
    const struct bq_gpio_config *cfg = dev->config;
    if (!device_is_ready(cfg->i2c_bus)) {
        return -ENODEV;
    }
    return 0;
}

#define CONFIG_ZMK_BQ2518X_GPIO_INIT_PRIORITY CONFIG_CHARGER_INIT_PRIORITY

#define BQ_GPIO_DEVICE_INIT(inst)                                                 \
    static const struct bq_gpio_config bq_gpio_cfg_##inst = {                     \
        .i2c_bus = DEVICE_DT_GET(DT_BUS(DT_PARENT(DT_DRV_INST(inst)))),           \
        .i2c_addr = DT_REG_ADDR(DT_PARENT(DT_DRV_INST(inst))),                    \
    };                                                                            \
    DEVICE_DT_INST_DEFINE(inst, bq_gpio_init, NULL, NULL,                         \
                          &bq_gpio_cfg_##inst, POST_KERNEL,                       \
                          CONFIG_ZMK_BQ2518X_GPIO_INIT_PRIORITY, &bq_gpio_api);

DT_INST_FOREACH_STATUS_OKAY(BQ_GPIO_DEVICE_INIT)
