/*
 * Bluetooth Manager Module for Podzilla 2
 */

#include "pz.h"
#include "btlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static PzModule *bt_module;
static PzConfig *bt_config;

#define BT_SETTING_ENABLED 1

static void bt_cleanup(void)
{
    close_all();
}

static PzWindow *bt_toggle_power(void)
{
    int enabled = pz_get_int_setting(bt_config, BT_SETTING_ENABLED);
    enabled = !enabled;
    pz_set_int_setting(bt_config, BT_SETTING_ENABLED, enabled);
    pz_save_config(bt_config);

    if (enabled) {
        char *cfg = pz_module_get_cfgpath(bt_module, "devices.txt");
        if (init_blue(cfg)) {
            pz_message("Bluetooth Enabled");
        } else {
            pz_message("Bluetooth Init Failed");
            pz_set_int_setting(bt_config, BT_SETTING_ENABLED, 0);
        }
    } else {
        close_all();
        pz_message("Bluetooth Disabled");
    }

    return (PzWindow *)PZ_MENU_DONOTHING;
}

static PzWindow *bt_scan(void)
{
    if (!pz_get_int_setting(bt_config, BT_SETTING_ENABLED)) {
        pz_error("Bluetooth is disabled.\nPlease enable it first.");
        return (PzWindow *)PZ_MENU_DONOTHING;
    }

    pz_message("Scanning for devices...");
    classic_scan();
    le_scan();
    pz_message("Scan complete.");
    return (PzWindow *)PZ_MENU_DONOTHING;
}

static PzWindow *bt_list_devices(void)
{
    char buf[4096];
    int count;

    if (!pz_get_int_setting(bt_config, BT_SETTING_ENABLED)) {
        pz_error("Bluetooth is disabled.");
        return (PzWindow *)PZ_MENU_DONOTHING;
    }

    count = device_info_ex(BTYPE_CL | BTYPE_LE | BTYPE_ME | BTYPE_SHORT, buf, sizeof(buf));

    if (count <= 0) {
        pz_message("No devices found.");
    } else {
        pz_message(buf);
    }
    return (PzWindow *)PZ_MENU_DONOTHING;
}

static void init_bluetooth(void)
{
    bt_module = pz_register_module("bluetooth", bt_cleanup);
    bt_config = pz_load_config(pz_module_get_cfgpath(bt_module, "bluetooth.conf"));

    /* Initialize default settings */
    if (!pz_get_setting(bt_config, BT_SETTING_ENABLED))
        pz_set_int_setting(bt_config, BT_SETTING_ENABLED, 0);

    if (pz_get_int_setting(bt_config, BT_SETTING_ENABLED)) {
        char *cfg = pz_module_get_cfgpath(bt_module, "devices.txt");
        init_blue(cfg);
    }

    /* Add menu items */
    pz_menu_add_action("/Settings/Bluetooth/Toggle Power", bt_toggle_power);
    pz_menu_add_action("/Settings/Bluetooth/Scan Devices", bt_scan);
    pz_menu_add_action("/Settings/Bluetooth/Devices", bt_list_devices);
}

PZ_MOD_INIT(init_bluetooth)