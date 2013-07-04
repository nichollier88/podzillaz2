/*
 * Last updated: May 18, 2008
 * ~Keripo
 *
 * Copyright (C) 2008 Keripo
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

static PzConfig *igpsp_conf;
static PzModule *igpsp_module;
static ttk_menu_item igpsp_browser_extension;
static const char *igpsp_binary, *igpsp_dir;

extern TWindow *open_directory_ext(const char *filename, const char *header, int ext(const char *file));
static TWidget *(*ltinstw)(int x, int y, int w, int h, int absheight,
		char *dt, int (*callback)(TWidget *, char *));
static int (*ltws)(TWidget * wid);

int text_available()
{
	if (!ltws)
		ltws = pz_module_softdep("tiwidgets", "ti_widget_start");
	if (!ltinstw)
		ltinstw = pz_module_softdep("tiwidgets",
				"ti_new_standard_text_widget");
	return (!!ltinstw & !!ltws);
}

int is_gba(const char *file)
{
        return check_ext(file, ".gba");
}

static PzWindow *igpsp_load_file(const char *file)
{
        char * command_igpsp = (char *) calloc(sizeof(command_igpsp), sizeof(char));
	strcpy(command_igpsp, "igpSP ");
	strcat(command_igpsp, file);
	pz_exec(pz_module_get_datapath (igpsp_module, command_igpsp));
	return TTK_MENU_DONOTHING;
}

static PzWindow *load_igpsp_handler(ttk_menu_item *item)
{
	return igpsp_load_file(item->data);
}

static PzWindow *browse_igpsp_roms()
{
	igpsp_dir = pz_get_string_setting(igpsp_conf, 1);
        return open_directory_ext(igpsp_dir, "Game Boy Advance", is_gba);
}

static int set_igpsp_directory(TWidget * wid, char * txt)
{
        pz_set_string_setting(igpsp_conf, 1, txt);
        pz_save_config(igpsp_conf);
        pz_close_window(wid->win);
        return 0;
}

static PzWindow * igpsp_directory()
{
        if (!text_available()) {
           pz_error(_("Tiwidgets(Text input) is missing."));
            mkdir("/mnt/rom//", S_IFDIR|S_IRUSR|S_IWUSR|S_IXUSR|S_IXGRP|S_IWGRP|S_IRGRP);
             set_igpsp_directory(NULL, "/mnt/rom/igpSP/");
            pz_error(_("Until you install tiwidgets your Rom folder will be /mnt/rom/igpSP/"));
          return TTK_MENU_UPONE;
         } else {
	  PzWindow * ret;
	   TWidget * wid;
	    ret = pz_new_window(_("igpSP Rom Directory"), PZ_WINDOW_NORMAL);
             wid = ltinstw(10, 40, ret->w-20, 10+ttk_text_height(ttk_textfont), 0, "/mnt/", set_igpsp_directory);
	    ttk_add_widget(ret, wid);
 	   ltws(wid);
          return pz_finish_window(ret);
        }
}

static void igpsp_cleanup()
{
	pz_browser_remove_handler(is_gba);
}

static void init_igpsp() 
{
        struct stat gbaBuf;

	igpsp_module = pz_register_module("igpSP", igpsp_cleanup);
	igpsp_binary = pz_module_get_datapath(igpsp_module, "igpSP");
	igpsp_conf = pz_load_config(pz_module_get_cfgpath(igpsp_module,"igpsp_dir.conf"));

        if (!stat(igpsp_binary, &gbaBuf) == S_IXUSR || 00100)
            chmod(igpsp_binary, S_IRWXU);
	
	igpsp_browser_extension.name = N_("Open with GBA");
	igpsp_browser_extension.makesub = load_igpsp_handler;
	pz_browser_add_action (is_gba, &igpsp_browser_extension);
	pz_browser_set_handler(is_gba, igpsp_load_file);

        pz_menu_add_action_group("/Extras/Games/Game Boy Advance", "Browse", browse_igpsp_roms);
        pz_menu_add_action_group("/Setting/Set directory/Set Game Boy Advance", "Browse", igpsp_directory);
}

PZ_MOD_INIT(init_igpsp)
