# Copyright (c) 2026 badjeff
# SPDX-License-Identifier: Apache-2.0

zephyr_library_amend()
zephyr_library_property(ALLOW_EMPTY TRUE)

zephyr_syscall_header(${ZEPHYR_BASE}/include/zephyr/drivers/charger.h)

# zephyr_library_sources_ifdef(CONFIG_USERSPACE charger_handlers.c)

# BQ2518x driver from Zephyr 4.4
# https://github.com/zephyrproject-rtos/zephyr/blob/main/drivers/charger/charger_bq2518x.c
# New changes are all scoped in CONFIG_CHARGER_BQ2518X_MODE_INT_SIG
zephyr_library_sources_ifdef(CONFIG_CHARGER_BQ2518X charger_bq2518x.c)

# Bridging BQ25186 PG/GPO Pin to virtual gpio
zephyr_library_sources_ifdef(CONFIG_CHARGER_BQ2518X_GPIO charger_bq2518x_gpio.c)

# Bridging BQ25186 Interupts for BT_BAS_BLS
zephyr_library_sources_ifdef(CONFIG_CHARGER_BQ25186_BAS charger_bq25186_bas.c)

# add "zephyr/drivers/adc/adc_context.h" to include path
# zephyr_include_directories(${ZEPHYR_BASE}/drivers/adc)

zephyr_include_directories(${ZEPHYR_CURRENT_MODULE_DIR}/include)
zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)
