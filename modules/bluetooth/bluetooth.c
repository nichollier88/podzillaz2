/*
 * Bluetooth Manager Module for Podzilla 2
 */

#include "pz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static PzModule *bt_module;
static PzConfig *bt_config;

#define BT_SETTING_ENABLED 1

static void bt_cleanup(void)
{
}

static PzWindow *bt_toggle_power(void)
{
    int enabled = pz_get_int_setting(bt_config, BT_SETTING_ENABLED);
    enabled = !enabled;
    pz_set_int_setting(bt_config, BT_SETTING_ENABLED, enabled);
    pz_save_config(bt_config);

    if (enabled) {
        system("bluetoothctl power on");
        pz_message("Bluetooth Enabled");
    } else {
        system("bluetoothctl power off");
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

    pz_message("Scanning for devices (5s)...");
    system("timeout 5 bluetoothctl scan on");
    pz_message("Scan complete.");
    return (PzWindow *)PZ_MENU_DONOTHING;
}

static PzWindow *bt_connect_helper(struct ttk_menu_item *item)
{
    char cmd[512];
    char *mac = (char *)item->data;

    if (!mac) return (PzWindow *)PZ_MENU_DONOTHING;

    snprintf(cmd, sizeof(cmd), "timeout 15 bluetoothctl pair %s", mac);
    system(cmd);

    snprintf(cmd, sizeof(cmd), "timeout 5 bluetoothctl trust %s", mac);
    system(cmd);

    snprintf(cmd, sizeof(cmd), "timeout 15 bluetoothctl connect %s", mac);
    if (system(cmd) == 0) {
        pz_message("Connected");
    } else {
        pz_message("Connection Failed");
    }

    return (PzWindow *)PZ_MENU_DONOTHING;
}

static PzWindow *bt_list_devices(void)
{
    FILE *fp;
    char line[256];
    TWidget *menu;
    PzWindow *win;

    if (!pz_get_int_setting(bt_config, BT_SETTING_ENABLED)) {
        pz_error("Bluetooth is disabled.");
        return (PzWindow *)PZ_MENU_DONOTHING;
    }

    menu = ttk_new_menu_widget(NULL, ttk_menufont, 0, 0);
    if (!menu) return (PzWindow *)PZ_MENU_DONOTHING;

    system("bluetoothctl devices > /tmp/bt_devices.txt");
    fp = fopen("/tmp/bt_devices.txt", "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            char *mac, *name;
            ttk_menu_item item;
            
            if (strncmp(line, "Device ", 7) != 0) continue;
            
            mac = strtok(line + 7, " \n");
            name = strtok(NULL, "\n");
            if (!mac || !name) continue;

            memset(&item, 0, sizeof(item));
            item.name = strdup(name);
            item.makesub = bt_connect_helper;
            item.data = strdup(mac);
            item.free_name = 1;
            item.free_data = 1;
            
            ttk_menu_append(menu, &item);
        }
        fclose(fp);
    }

    win = pz_new_menu_window(menu);
    ttk_window_set_title(win, strdup("Devices"));
    return win;
}

static void init_bluetooth(void)
{
    bt_module = pz_register_module("bluetooth", bt_cleanup);
    bt_config = pz_load_config(pz_module_get_cfgpath(bt_module, "bluetooth.conf"));

    /* Initialize default settings */
    if (!pz_get_setting(bt_config, BT_SETTING_ENABLED))
        pz_set_int_setting(bt_config, BT_SETTING_ENABLED, 0);

    if (pz_get_int_setting(bt_config, BT_SETTING_ENABLED)) {
        system("bluetoothctl power on");
    } else {
        system("bluetoothctl power off");
    }

    /* Add menu items */
    pz_menu_add_action("/Settings/Bluetooth/Toggle Power", bt_toggle_power);
    pz_menu_add_action("/Settings/Bluetooth/Scan Devices", bt_scan);
    pz_menu_add_action("/Settings/Bluetooth/Devices", bt_list_devices);
}

PZ_MOD_INIT(init_bluetooth)