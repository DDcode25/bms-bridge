/*
 * Консоль: status, cid2 и команды симулятора для стендовых тестов ТЗ 7.1
 * (T4 — sim pause, T6 — sim cell, T7 — sim temp, T9 — sim module).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_console.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "app.h"
#include "bmz_driver.h"
#include "pylon_rs485.h"

static int cmd_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    app_print_status();
    return 0;
}

static int cmd_cid2(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    pylon_rs485_print_cid2();
    return 0;
}

#ifdef CONFIG_BMZ_SOURCE_SIM

static int sim_usage(void)
{
    printf("sim cells <мВ>          — все ячейки\n"
           "sim cell <1..16> <мВ>   — одна ячейка\n"
           "sim temp <°C>           — все датчики, например 0 или -5.5\n"
           "sim current <А>         — ток батареи, заряд «+»\n"
           "sim soc <%%>\n"
           "sim pause | resume      — перестать / снова обновлять данные (T4)\n"
           "sim module <1..%d> on|off — модуль выпал / вернулся (T9)\n",
           CONFIG_BMZ_MODULES);
    return 1;
}

static int cmd_sim(int argc, char **argv)
{
    if (argc < 2) {
        return sim_usage();
    }
    const char *what = argv[1];
    if (!strcmp(what, "cells") && argc == 3) {
        bmz_sim_set_cells((uint16_t)atoi(argv[2]));
    } else if (!strcmp(what, "cell") && argc == 4) {
        int idx = atoi(argv[2]);
        if (idx < 1 || idx > 16) {
            return sim_usage();
        }
        bmz_sim_set_cell((uint8_t)(idx - 1), (uint16_t)atoi(argv[3]));
    } else if (!strcmp(what, "temp") && argc == 3) {
        bmz_sim_set_temp((int16_t)(strtof(argv[2], NULL) * 10.0f));
    } else if (!strcmp(what, "current") && argc == 3) {
        bmz_sim_set_current((int32_t)(strtof(argv[2], NULL) * 1000.0f));
    } else if (!strcmp(what, "soc") && argc == 3) {
        bmz_sim_set_soc((uint8_t)atoi(argv[2]));
    } else if (!strcmp(what, "pause") && argc == 2) {
        bmz_sim_set_paused(true);
    } else if (!strcmp(what, "resume") && argc == 2) {
        bmz_sim_set_paused(false);
    } else if (!strcmp(what, "module") && argc == 4) {
        int idx = atoi(argv[2]);
        if (idx < 1 || idx > CONFIG_BMZ_MODULES) {
            return sim_usage();
        }
        bmz_sim_set_module_online((uint8_t)(idx - 1), !strcmp(argv[3], "on"));
    } else {
        return sim_usage();
    }
    return 0;
}

#endif

void console_start(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "bridge>";

    esp_console_register_help_command();
    const esp_console_cmd_t cmds[] = {
        {.command = "status", .help = "Сводка: батарея, лимиты, опрос инвертора", .func = cmd_status},
        {.command = "cid2", .help = "Какие команды Pylontech запрашивал инвертор", .func = cmd_cid2},
#ifdef CONFIG_BMZ_SOURCE_SIM
        {.command = "sim", .help = "Управление симулятором, без аргументов — справка", .func = cmd_sim},
#endif
    };
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    }

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));
#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));
#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));
#else
#error Unsupported console type
#endif
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
