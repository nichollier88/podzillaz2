/*
 * Last updated: dec 5, 2008
 * jerl92
 *
 * Copyright (C) 2008 Keripo, jerl92
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "pz.h"

#define FRAMESKIP	(0)
#define LANG		(1)
#define SCALING		(2)
#define FOLDER		(3)

static PzModule * module;
static PzConfig * config;
static ttk_menu_item browser_extension;
static const char * binary, * igamegear_log;
static const char * frameskip_options[] = {"None", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", 0};
static const char * locale_options[] = {"Overseas", "Japan", 0};
static const char * scaling_options[] = {"Centre", "Scale", "Stretch", 0};

static TWindow *(*open_directory_ext_softdep)(const char *filename, const char *title, int ext(const char *file));

static int (*ti_widget_start_softdep)(TWidget * wid);
static TWidget *(*ti_new_standard_text_widget_softdep)(int x, int y, int w, int h, int absheight, char *dt, int (*callback)(TWidget *, char *));
static void (*ti_multiline_text_softdep)(ttk_surface srf, ttk_font fnt, int x, int y, int w, int h, ttk_color col, const char *t, int cursorpos, int scroll, int * lc, int * sl, int * cb);

static int browser_available()
{
	if (!open_directory_ext_softdep)
	  open_directory_ext_softdep = pz_module_softdep("browser", "open_directory_ext");
        return (!!open_directory_ext_softdep);
}

static int tiwidgets_available()
{
	if (!ti_widget_start_softdep)
	  ti_widget_start_softdep = pz_module_softdep("tiwidgets", "ti_widget_start");
	if (!ti_new_standard_text_widget_softdep)
	  ti_new_standard_text_widget_softdep = pz_module_softdep("tiwidgets", "ti_new_standard_text_widget");
        if (!ti_multiline_text_softdep)
	  ti_multiline_text_softdep = pz_module_softdep("tiwidgets", "ti_multiline_text");
	return (!!ti_widget_start_softdep & !!ti_new_standard_text_widget_softdep & !!ti_multiline_text_softdep);
}


int is_smsgg(const char *file)
{
	return (check_ext(file, ".sms") ||
			check_ext(file, ".gg")
			);
}

static PzWindow *load_gg_file(const char *file)
{
	char frameskip[13];
	snprintf(frameskip, 13, "--fskip %i",
		pz_get_int_setting(config, FRAMESKIP));
	char locale[9];
	if (pz_get_int_setting(config, LANG)==1) {
		snprintf(locale, 9, "--japan");
	} else {
		locale[0] = '\0';
	}
	char scaling[19];
	switch (pz_get_int_setting(config, SCALING))
	{
		case 0: snprintf(scaling, 19, "--display center"); break;
		case 1: snprintf(scaling, 19, "--display scale"); break;
		case 2: snprintf(scaling, 19, "--display stretch"); break;
	}
	char command[256];
	sprintf(command, "%s %s %s %s %s >%s", pz_module_get_datapath (module, "iGameGear"), file, frameskip, scaling, locale, igamegear_log);
	const char *const argv[] = {"sh", "-c", command, NULL};
	pz_execv("/bin/sh", (char *const *)argv);
	return TTK_MENU_DONOTHING;
}


static PzWindow *load_file_handler(ttk_menu_item *item)
{
	return load_gg_file(item->data);
}

static PzWindow *browse_roms()
{
        struct stat st;  
	char folder[30];
	sprintf(folder, "%s", pz_get_string_setting(config, FOLDER));

        if (!pz_get_setting(config, FOLDER)) {
          pz_warning(_("No folder have been selected yet"));
          pz_warning(_("Select a folder in /Setting/Select folder/iGameGear"));
          return TTK_MENU_DONOTHING;
        }

        if (stat (folder, &st) < 0 || !S_ISDIR (st.st_mode)) {
          pz_warning("Folder do not exsist. check again");
          return TTK_MENU_DONOTHING;
        }

        return open_directory_ext_softdep(folder, "iGameGear Roms", is_smsgg);        
}

static void select_folder_draw(TWidget * wid, ttk_surface srf)
{
	ttk_ap_fillrect(srf, ttk_ap_getx("window.bg"), wid->x, wid->y, wid->w, wid->h);
	if (ttk_screen->w < 160) {
		ti_multiline_text_softdep(srf, ttk_textfont, wid->x, wid->y, wid->w, wid->h, ttk_ap_getx("window.fg")->color,
			_("Type the ROM folder location."), -1, 0, 0, 0, 0);
	} else if (ttk_screen->w < 200) {
		ti_multiline_text_softdep(srf, ttk_textfont, wid->x, wid->y, wid->w, wid->h, ttk_ap_getx("window.fg")->color,
			_("Type the folder where the ROM are located."), -1, 0, 0, 0, 0);
	} else {
		ti_multiline_text_softdep(srf, ttk_textfont, wid->x, wid->y, wid->w, wid->h, ttk_ap_getx("window.fg")->color,
			_("Type the folder where the ROM are located. To be able tu use ROM selector in the Extras menu."), -1, 0, 0, 0, 0);
	}
}

static int save_config(TWidget * wid, char * fn)
{
        pz_set_string_setting(config, FOLDER, fn);
        pz_save_config(config);
        pz_close_window(wid->win);
        return 0;
}

static PzWindow * gg_nes_set_dir_window()
{
	TWindow * ret;
	TWidget * wid;
	TWidget * wid2;
	ret = pz_new_window(_("iGameGear"), PZ_WINDOW_NORMAL);
        wid = ti_new_standard_text_widget_softdep(10, 
                       10+ttk_text_height(ttk_textfont)*((ttk_screen->w < 160 || ttk_screen->w >= 320)?2:3), ret->w-20, 10+ttk_text_height(ttk_textfont), 0, "/mnt/", save_config);
	ttk_add_widget(ret, wid);
	wid2 = ttk_new_widget(10, 5);
	wid2->w = ret->w-20;
	wid2->h = ttk_text_height(ttk_menufont)*((ttk_screen->w < 160 || ttk_screen->w >= 320)?2:3);
	wid2->draw = select_folder_draw;
	ttk_add_widget(ret, wid2);
        ti_widget_start_softdep(wid);
        return pz_finish_window(ret);
}

static PzWindow *view_igg_log()
{
        TWindow * w;
	FILE * fp;
        char * buf;        
	fp = fopen(igamegear_log, "r");
	if (!fp) return 0;
        buf = strdup((char *)igamegear_log);
	w = pz_new_textview_window(buf);
	fclose(fp);
	return w;
}

static void cleanup()
{
	pz_save_config(config);
	pz_free_config(config);
	config = 0;
	pz_browser_remove_handler(is_smsgg);
}

static void init_launch() 
{
        struct stat st;

	module = pz_register_module("iGameGear", cleanup);
	binary = pz_module_get_datapath(module, "iGameGear");
	config = pz_load_config(pz_module_get_cfgpath(module, "config.conf"));
	igamegear_log = pz_module_get_cfgpath(module, "iGameGear.log");

	if (!pz_get_setting(config, FRAMESKIP)) pz_set_int_setting (config, FRAMESKIP, 5);
	if (!pz_get_setting(config, LANG)) pz_set_int_setting (config, LANG, 0);
	if (!pz_get_setting(config, SCALING)) pz_set_int_setting (config, SCALING, 0);

        if (!stat(binary, &st) == S_IXUSR || 00100)
            chmod(binary, S_IRWXU);

	if (browser_available()) 
          pz_menu_add_action_group("/Extras/Games/iGameGear/Roms", "FastLaunch", browse_roms);
        else pz_warning(_("Module browser is missing"));

	pz_menu_add_setting_group("/Extras/Games/iGameGear/Frameskip", "Settings", FRAMESKIP, config, frameskip_options);
	pz_menu_add_setting_group("/Extras/Games/iGameGear/Scaling", "Settings", SCALING, config, scaling_options);
	pz_menu_add_setting_group("/Extras/Games/iGameGear/Locale", "Settings", LANG, config, locale_options);
	pz_menu_add_action_group("/Extras/Games/iGameGear/View log", "settings", view_igg_log);

        if (tiwidgets_available())
	  pz_menu_add_action_group("/Setting/Select Folder/iGameGear", "Browse", gg_nes_set_dir_window);
        else pz_warning(_("Module tiwidgets is missing"));

	browser_extension.name = N_("Open with iGameGear");
	browser_extension.makesub = load_file_handler;
	pz_browser_add_action (is_smsgg, &browser_extension);
	pz_browser_set_handler(is_smsgg, load_gg_file);
}

PZ_MOD_INIT(init_launch)
