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
static TWidget *devices_menu_widget = NULL;
static void bt_populate_devices_menu(TWidget *menu);

#define BT_SETTING_ENABLED 1

int is_bt_enabled()
{
    FILE *fp;
    char line[256];
    int enabled = 0;

    fp = popen("bluetoothctl show", "r");
    if (fp == NULL)
    {
        pz_error("Failed to run bluetoothctl");
        return 0;
    }

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        if (strstr(line, "Powered: yes") != NULL)
        {
            enabled = 1;
            break;
        }
    }

    pclose(fp);

    if (!enabled)
        pz_error("Bluetooth is disabled.\nPlease enable it first.");
    return enabled;
}

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

typedef struct
{
    pid_t scan_pid;
    int spinner_state;
} bt_scan_data;

static void scan_draw(PzWidget *wid, ttk_surface srf)
{
    bt_scan_data *data = (bt_scan_data *)wid->data;
    char text[64];
    const char spinner[] = {'|', '/', '-', '\\'};

    ttk_fillrect(srf, 0, 0, wid->w, wid->h, ttk_ap_getx("window.bg")->color);

    snprintf(text, sizeof(text), "%c", spinner[data->spinner_state % 4]);
    ttk_text(srf, ttk_textfont, (wid->w - ttk_text_width(ttk_textfont, text)) / 2,
             (wid->h - ttk_text_height(ttk_textfont)) / 2, ttk_ap_getx("window.fg")->color, text);
}

static int scan_loop(TWidget *this)
{
    bt_scan_data *data = (bt_scan_data *)this->data;
    int status;

    data->spinner_state++;
    this->dirty = 1;

    if (waitpid(data->scan_pid, &status, WNOHANG) > 0)
    {
        pz_message("Scan complete.");
        pz_close_window(this->win);
        if (devices_menu_widget)
        {
            bt_populate_devices_menu(devices_menu_widget);
        }
    }

    return 0;
}

static int scan_handle_event(PzEvent *e)
{
    if (e->type == PZ_EVENT_BUTTON_DOWN && e->arg == PZ_BUTTON_MENU)
    {
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
    if (!is_bt_enabled())
        return (PzWindow *)PZ_MENU_DONOTHING;

    pid_t pid = vfork();
    if (pid == 0)
    {
        execl("/bin/sh", "sh", "-c", "btmgmt find > /tmp/bt_devices.txt", NULL);
        exit(1);
    }
    else if (pid > 0)
    {
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
    }
    else
    {
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
    if (system(cmd) != 0)
    {
        pz_error("Pairing Failed");
        goto EXIT_BT_CONNECT;
    }

    snprintf(cmd, sizeof(cmd), "bluetoothctl trust %s", mac);
    if (system(cmd) != 0)
    {
        pz_error("Trusting Failed");
        goto EXIT_BT_CONNECT;
    }

    snprintf(cmd, sizeof(cmd), "bluetoothctl connect %s", mac);
    if (system(cmd) != 0)
    {
        pz_error("Connection Failed");
        goto EXIT_BT_CONNECT;
    }

    pz_message("Connected");

EXIT_BT_CONNECT:
    return (PzWindow *)PZ_MENU_DONOTHING;
}

struct mac_list
{
    char *mac;
    struct mac_list *next;
};

static void bt_add_device(TWidget *menu, struct mac_list **seen_macs, char *mac, char *name, PzWindow *(*action)(struct ttk_menu_item *))
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

    item->makesub = action;
    item->data = strdup(mac);
    item->free_name = 1;
    item->free_data = 1;

    ttk_menu_append(menu, item);
}

static PzWindow *bt_disconnect_helper(struct ttk_menu_item *item)
{
    char cmd[512];
    char *mac = (char *)item->data;

    if (!mac)
        return (PzWindow *)PZ_MENU_DONOTHING;

    snprintf(cmd, sizeof(cmd), "btmgmt unpair %s", mac);
    pz_message((system(cmd) == 0) ? "Unpaired" : "Unpairing Failed");

    return (PzWindow *)PZ_MENU_DONOTHING;
}

static int bt_check_connected(const char *mac)
{
    char cmd[512];
    char line[256];
    FILE *fp;
    int connected = 0;

    snprintf(cmd, sizeof(cmd), "bluetoothctl info %s", mac);
    fp = popen(cmd, "r");
    if (fp)
    {
        while (fgets(line, sizeof(line), fp))
        {
            if (strstr(line, "Connected: yes"))
            {
                connected = 1;
                break;
            }
        }
        pclose(fp);
    }
    return connected;
}

static void bt_get_active_audio_mac(char *out_mac, size_t max_len)
{
    FILE *fp = popen("wpctl status", "r");
    char line[256];
    
    if (out_mac && max_len > 0)
        out_mac[0] = '\0';
        
    if (!fp)
        return;

    int in_default_section = 0;

    while (fgets(line, sizeof(line), fp))
    {
        if (strstr(line, "Default Configured Devices:"))
        {
            in_default_section = 1;
        }
        else if (in_default_section && strstr(line, "Audio/Sink"))
        {
            char *p = strstr(line, "bluez_output.");
            if (p)
            {
                p += 13; /* skip "bluez_output." */
                int i = 0;
                while (*p && *p != '.' && i < max_len - 1 && i < 17)
                {
                    out_mac[i] = (*p == '_') ? ':' : *p;
                    p++;
                    i++;
                }
                out_mac[i] = '\0';
            }
            break;
        }
        else if (in_default_section && line[0] != ' ' && line[0] != '\t' && line[0] != '\n')
        {
            /* Exited section */
            break;
        }
    }
    pclose(fp);
}

static PzWindow *bt_list_paired_devices(void)
{
    FILE *fp;
    char line[256];
    TWidget *menu;
    PzWindow *win;
    struct mac_list *seen_macs = NULL;

    if (!is_bt_enabled())
        return (PzWindow *)PZ_MENU_DONOTHING;

    menu = ttk_new_menu_widget(NULL, ttk_menufont, ttk_screen->w - ttk_screen->wx, ttk_screen->h - ttk_screen->wy);
    if (!menu)
    {
        printf("Unable to create menu widget\n");
        return (PzWindow *)PZ_MENU_DONOTHING;
    }

    fp = popen("bluetoothctl devices Paired", "r");
    if (fp)
    {
        int found = 0;
        while (fgets(line, sizeof(line), fp))
        {
            char current_mac[64] = {0};
            char current_name[256] = {0};

            /* Expected format: Device MAC_ADDRESS Name */
            if (strncmp(line, "Device ", 7) == 0)
            {
                char *mac_start = line + 7;
                char *mac_end = strchr(mac_start, ' ');
                if (mac_end)
                {
                    *mac_end = '\0';
                    strncpy(current_mac, mac_start, sizeof(current_mac) - 1);

                    char *name_start = mac_end + 1;
                    char *name_end = strchr(name_start, '\n');
                    if (name_end)
                        *name_end = '\0';
                    strncpy(current_name, name_start, sizeof(current_name) - 1);

                    char active_audio_mac[64] = {0};
                    char display_name[512];
                    int connected = 0;
                    int is_audio = 0;

                    connected = bt_check_connected(current_mac);
                    bt_get_active_audio_mac(active_audio_mac, sizeof(active_audio_mac));

                    if (strcasecmp(current_mac, active_audio_mac) == 0)
                        is_audio = 1;

                    snprintf(display_name, sizeof(display_name), "%s%s%s", 
                             is_audio ? "[A] " : "", 
                             connected ? "[C] " : "", 
                             current_name[0] ? current_name : current_mac);

                    bt_add_device(menu, &seen_macs, current_mac, display_name, 
                                  connected ? bt_disconnect_helper : bt_connect_helper);
                    found = 1;
                }
            }
        }
        pclose(fp);

        if (!found)
            pz_message("No paired devices.");
    }
    else
    {
        pz_error("Failed to list paired devices.");
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
    ttk_window_set_title(win, strdup("Paired"));
    win->data = 0x12345678;
    return win;
}

static PzWindow *bt_refresh_helper(struct ttk_menu_item *item)
{
    (void)item;
    return bt_scan();
}

static void bt_populate_devices_menu(TWidget *menu)
{
    FILE *fp;
    char line[256];
    struct mac_list *seen_macs = NULL;
    char current_mac[64] = {0};
    char current_name[256] = {0};

    ttk_menu_clear(menu);

    ttk_menu_item *refresh_item = calloc(1, sizeof(ttk_menu_item));
    if (refresh_item)
    {
        refresh_item->name = strdup("Refresh...");
        refresh_item->makesub = bt_refresh_helper;
        refresh_item->free_name = 1;
        ttk_menu_append(menu, refresh_item);
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
                    bt_add_device(menu, &seen_macs, current_mac, current_name, bt_connect_helper);
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
            bt_add_device(menu, &seen_macs, current_mac, current_name, bt_connect_helper);
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
}

static void (*old_menu_destroy)(TWidget *);
static void devices_menu_destroy(TWidget *this)
{
    devices_menu_widget = NULL;
    if (old_menu_destroy)
        old_menu_destroy(this);
}

static PzWindow *bt_list_devices(void)
{
    TWidget *menu;
    PzWindow *win;

    if (!is_bt_enabled())
        return (PzWindow *)PZ_MENU_DONOTHING;

    menu = ttk_new_menu_widget(NULL, ttk_menufont, ttk_screen->w - ttk_screen->wx, ttk_screen->h - ttk_screen->wy);
    if (!menu)
    {
        printf("Unable to create menu widget\n");
        return (PzWindow *)PZ_MENU_DONOTHING;
    }

    devices_menu_widget = menu;
    old_menu_destroy = menu->destroy;
    menu->destroy = devices_menu_destroy;

    bt_populate_devices_menu(menu);

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
    ttk_menu_item *item =
        pz_menu_add_setting("/Settings/Bluetooth/Toggle Power", BT_SETTING_ENABLED, bt_config, 0);
    item->choicechanged = bt_power_changed;
    pz_menu_add_action("/Settings/Bluetooth/Devices", bt_list_devices);
    pz_menu_add_action("/Settings/Bluetooth/Paired", bt_list_paired_devices);
}

PZ_MOD_INIT(init_bluetooth)