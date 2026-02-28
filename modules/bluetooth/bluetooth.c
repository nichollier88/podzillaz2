/*
 * Bluetooth Manager Module for Podzilla 2
 */

#include "pz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static PzModule *bt_module;
static PzConfig *bt_config;

#define BT_SETTING_ENABLED 1

static void bt_cleanup(void)
{
}

void bt_power(u_int8_t enable, u_int8_t show_message)
{
    if (enable)
    {
        system("btmgmt power on");
        if (show_message)
            pz_message("Bluetooth Enabled");
    }
    else
    {
        system("btmgmt power off");
        if (show_message)
            pz_message("Bluetooth Disabled");
    }
}

static void bt_power_changed(ttk_menu_item *item, int sid)
{
    int enabled = item->choice;
    pz_set_int_setting(bt_config, BT_SETTING_ENABLED, enabled);
    pz_save_config(bt_config);

    bt_power(enabled, 1);
}

typedef struct {
    pid_t scan_pid;
    int spinner_state;
} bt_scan_data;

static void scan_draw(PzWidget *wid, ttk_surface srf)
{
    bt_scan_data *data = (bt_scan_data *)wid->data;
    char text[64];
    const char spinner[] = {'|', '/', '-', '\\'};
    
    ttk_fillrect(srf, 0, 0, wid->w, wid->h, ttk_ap_getx("window.bg")->color);
    
    snprintf(text, sizeof(text), "Scanning for devices %c", spinner[data->spinner_state % 4]);
    ttk_text(srf, ttk_textfont, (wid->w - ttk_text_width(ttk_textfont, text)) / 2,
             (wid->h - ttk_text_height(ttk_textfont)) / 2, ttk_ap_getx("window.fg")->color, text);
}

static int scan_loop(TWidget *this)
{
    bt_scan_data *data = (bt_scan_data *)this->data;
    int status;
    
    data->spinner_state++;
    this->dirty = 1;
    
    if (waitpid(data->scan_pid, &status, WNOHANG) > 0) {
        pz_message("Scan complete.");
        pz_close_window(this->win);
    }
    
    return 0;
}

static int scan_handle_event(PzEvent *e)
{
    if (e->type == PZ_EVENT_BUTTON_DOWN && e->arg == PZ_BUTTON_MENU) {
        bt_scan_data *data = (bt_scan_data *)e->wid->data;
        kill(data->scan_pid, SIGTERM);
        pz_close_window(e->wid->win);
        return TTK_EV_DONE;
    }
    return TTK_EV_UNUSED;
}

static void scan_destroy(TWidget *this)
{
    free(this->data);
}

static PzWindow *bt_scan(void)
{
    if (!pz_get_int_setting(bt_config, BT_SETTING_ENABLED))
    {
        pz_error("Bluetooth is disabled.\nPlease enable it first.");
        return (PzWindow *)PZ_MENU_DONOTHING;
    }

    pid_t pid = vfork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", "btmgmt find > /tmp/bt_devices.txt", NULL);
        exit(1);
    } else if (pid > 0) {
        PzWindow *win = pz_new_window(_("Scanning..."), PZ_WINDOW_NORMAL);
        PzWidget *wid = pz_add_widget(win, scan_draw, scan_handle_event);
        
        bt_scan_data *data = malloc(sizeof(bt_scan_data));
        data->scan_pid = pid;
        data->spinner_state = 0;
        wid->data = data;
        
        ttk_widget_set_timer(wid, 250);
        wid->timer = scan_loop;
        wid->destroy = scan_destroy;
        
        return pz_finish_window(win);
    } else {
        pz_error("Failed to start scan process.");
        return (PzWindow *)PZ_MENU_DONOTHING;
    }
}

static PzWindow *bt_connect_helper(struct ttk_menu_item *item)
{
    char cmd[512];
    char *mac = (char *)item->data;

    if (!mac)
        return (PzWindow *)PZ_MENU_DONOTHING;

    snprintf(cmd, sizeof(cmd), "btmgmt pair %s", mac);
    if (system(cmd) == 0)
    {
        pz_message("Paired");
    }
    else
    {
        pz_message("Pairing Failed");
    }

    snprintf(cmd, sizeof(cmd), "bluetoothctl trust %s", mac);
    if (system(cmd) == 0)
    {
        pz_message("Trusted");
    }
    else
    {
        pz_message("Trusting Failed");
    }

    snprintf(cmd, sizeof(cmd), "bluetoothctl connect %s", mac);
    if (system(cmd) == 0)
    {
        pz_message("Connected");
    }
    else
    {
        pz_message("Connection Failed");
    }

    return (PzWindow *)PZ_MENU_DONOTHING;
}

struct mac_list
{
    char *mac;
    struct mac_list *next;
};

static void bt_add_device(TWidget *menu, struct mac_list **seen_macs, char *mac, char *name)
{
    struct mac_list *node;
    ttk_menu_item *item;

    /* Check for duplicates */
    for (node = *seen_macs; node; node = node->next)
    {
        if (strcmp(node->mac, mac) == 0)
        {
            return;
        }
    }

    node = malloc(sizeof(struct mac_list));
    if (node)
    {
        node->mac = strdup(mac);
        node->next = *seen_macs;
        *seen_macs = node;
    }

    item = calloc(1, sizeof(ttk_menu_item));
    if (!item)
        return;

    if (name && name[0])
        item->name = strdup(name);
    else
        item->name = strdup(mac);

    item->makesub = bt_connect_helper;
    item->data = strdup(mac);
    item->free_name = 1;
    item->free_data = 1;

    ttk_menu_append(menu, item);
}

static PzWindow *bt_list_devices(void)
{
    FILE *fp;
    char line[256];
    TWidget *menu;
    PzWindow *win;
    struct mac_list *seen_macs = NULL;
    char current_mac[64] = {0};
    char current_name[256] = {0};

    if (!pz_get_int_setting(bt_config, BT_SETTING_ENABLED))
    {
        pz_error("Bluetooth is disabled.");
        return (PzWindow *)PZ_MENU_DONOTHING;
    }

    menu = ttk_new_menu_widget(NULL, ttk_menufont, ttk_screen->w -
                ttk_screen->wx, ttk_screen->h - ttk_screen->wy);
    if (!menu)
    {
        printf("Unable to create menu widget\n");
        return (PzWindow *)PZ_MENU_DONOTHING;
    }

    fp = fopen("/tmp/bt_devices.txt", "r");
    if (fp)
    {
        while (fgets(line, sizeof(line), fp))
        {
            char *mac_start, *name_start;

            if ((mac_start = strstr(line, "dev_found:")))
            {
                /* Add previous device if exists */
                if (current_mac[0])
                {
                    bt_add_device(menu, &seen_macs, current_mac, current_name);
                }

                /* Reset for new device */
                memset(current_mac, 0, sizeof(current_mac));
                memset(current_name, 0, sizeof(current_name));

                mac_start += 10;
                while (*mac_start == ' ')
                    mac_start++;
                char *mac_end = strchr(mac_start, ' ');
                if (mac_end)
                    *mac_end = 0;
                strncpy(current_mac, mac_start, sizeof(current_mac) - 1);
            }
            else if ((name_start = strstr(line, "name ")) && current_mac[0])
            {
                name_start += 5;
                char *name_end = strchr(name_start, '\n');
                if (name_end)
                    *name_end = 0;
                strncpy(current_name, name_start, sizeof(current_name) - 1);
            }
        }
        /* Add last device */
        if (current_mac[0])
        {
            bt_add_device(menu, &seen_macs, current_mac, current_name);
        }
        fclose(fp);
    }
    else
    {
        pz_message("No devices found.\nPlease scan first.");
    }

    /* Free seen list */
    while (seen_macs)
    {
        struct mac_list *next = seen_macs->next;
        free(seen_macs->mac);
        free(seen_macs);
        seen_macs = next;
    }

    win = pz_new_menu_window(menu);
    ttk_window_set_title(win, strdup("Devices"));
    win->data = 0x12345678;
    return win;
}

static void init_bluetooth(void)
{
    bt_module = pz_register_module("bluetooth", bt_cleanup);
    bt_config = pz_load_config(pz_module_get_cfgpath(bt_module, "bluetooth.conf"));

    /* Initialize default settings */
    if (!pz_get_setting(bt_config, BT_SETTING_ENABLED))
        pz_set_int_setting(bt_config, BT_SETTING_ENABLED, 0);

    bt_power(pz_get_int_setting(bt_config, BT_SETTING_ENABLED), 0);

    /* Add menu items */
    ttk_menu_item *item = pz_menu_add_setting(
        "/Settings/Bluetooth/Toggle Power", BT_SETTING_ENABLED, bt_config, 0);
    item->choicechanged = bt_power_changed;
    pz_menu_add_action("/Settings/Bluetooth/Scan Devices", bt_scan);
    pz_menu_add_action("/Settings/Bluetooth/Devices", bt_list_devices);
}

PZ_MOD_INIT(init_bluetooth)