/*
 * Last updated: Jul 16, 2013
 * ~Jerl92
 *
 * Copyright (C) 2008-2013 Keripo Jerl92
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

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "pz.h"

static PzConfig *config;
static PzModule *module;
static ttk_menu_item browser_extension;
static const char *binary;

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

static int is_gba(const char *file)
{
        return check_ext(file, ".gba");
}

static PzWindow *exec_file(const char *file)
{ 
        char * command = (char *) calloc(120, sizeof(char));
	strcpy(command, "igpSP ");
	strcat(command, file);
	pz_exec(pz_module_get_datapath (module, command));
	return TTK_MENU_DONOTHING;
}

static PzWindow *load_handler(ttk_menu_item *item)
{
	return exec_file(item->data);
}

static PzWindow *browse_roms()
{
        struct stat st;
        char * folder = (char *) pz_get_string_setting(config, 1);

        if (stat (folder, &st) < 0 || !S_ISDIR (st.st_mode)) {
          pz_warning("Folder do not exsist or have been not selected");
          pz_warning("Select a valid Folder in Setting -> Select folder -> Game Boy Advance");  
          return TTK_MENU_DONOTHING;
        }
        
        if (browser_available()) {
          return open_directory_ext_softdep(folder, "Game Boy Advance", is_gba);
        } 

        pz_warning("Browser module is missing.");
        return TTK_MENU_DONOTHING;
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

static int save_folder(TWidget * wid, char * fn)
{	
        pz_set_string_setting(config, 1, fn);
        pz_save_config(config);
        pz_ipod_fix_settings(config);
        pz_close_window(wid->win);
	return 0;
}

static PzWindow * select_folder()
{
        if (tiwidgets_available()) {
	PzWindow * ret;
	TWidget * wid;
	TWidget * wid2;
	ret = pz_new_window(_("Game Boy Advance"), PZ_WINDOW_NORMAL);
	wid = ti_new_standard_text_widget_softdep(10, 
                       10+ttk_text_height(ttk_textfont)*((ttk_screen->w < 160 || ttk_screen->w >= 320)?2:3), ret->w-20, 10+ttk_text_height(ttk_textfont), 0, "/mnt/", save_folder);
	ttk_add_widget(ret, wid);
	wid2 = ttk_new_widget(10, 5);
	wid2->w = ret->w-20;
	wid2->h = ttk_text_height(ttk_menufont)*((ttk_screen->w < 160 || ttk_screen->w >= 320)?2:3);
	wid2->draw = select_folder_draw;
	ttk_add_widget(ret, wid2);
	ret = pz_finish_window(ret);
	ti_widget_start_softdep(wid);
	return ret;
        }
        pz_warning("Tiwidgets module is missing");
        return TTK_MENU_DONOTHING;
}

static void cleanup()
{
	pz_browser_remove_handler(is_gba);
}

static void init() 
{
        struct stat st;

	module = pz_register_module("igpSP", cleanup);
	binary = pz_module_get_datapath(module, "igpSP");
	config = pz_load_config(pz_module_get_cfgpath(module,"igpsp_dir.conf"));
             
        if (!stat(binary, &st) == S_IXUSR || 00100)
            chmod(binary, S_IRWXU);
	
	browser_extension.name = N_("Open with Game Boy Advance");
	browser_extension.makesub = load_handler;
	pz_browser_add_action (is_gba, &browser_extension);
	pz_browser_set_handler(is_gba, exec_file);

        pz_menu_add_action_group("/Extras/Games/Game Boy Advance", "Browse", browse_roms);
        pz_menu_add_action_group("/Setting/Select folder/Game Boy Advance", "Browse", select_folder);
}

PZ_MOD_INIT(init)
