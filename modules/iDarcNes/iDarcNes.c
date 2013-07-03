/*
 * jerl92 2008
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

static PzConfig *Config;
static PzModule *Module;
static ttk_menu_item browser_extension;

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

int IF_NES(const char *file)
{
  return check_ext(file, ".nes");
}

static PzWindow *idarcnes_exec(const char *file)
{
        char * command = (char *) calloc(120, sizeof(char));
	strcpy(command, "iDarcNes ");
	strcat(command, file);
	pz_exec(pz_module_get_datapath (Module, command));
	return TTK_MENU_DONOTHING;
}

static PzWindow *nes_file_handler(ttk_menu_item *item)
{
	return idarcnes_exec(item->data);
}

static PzWindow *idarcnes_browser()
{
        static const char *Directory;
        Directory = pz_get_string_setting(Config, 1);
        if (!Config){
           pz_error(_("No directory have been select."));
           return TTK_MENU_UPALL;
           pz_error(_("Go to /Setting/Select directory/iDarcNes to set it"));
           return 0;
        }
         else { 
           return open_directory_ext(Directory, "iDarcNes", IF_NES);
        } 
}

static int save_config(TWidget * wid, char * txt)
{
        pz_set_string_setting(Config, 1, txt);
        pz_save_config(Config);
        pz_close_window(wid->win);
        return 0;
}

PzWindow * set_directory()
{
     if (!text_available()) {
             pz_error(_("Tiwidgets(Text input) is missing."));
              mkdir("/mnt/rom/iDarcNes/", S_IFDIR|S_IRUSR|S_IWUSR|S_IXUSR|S_IXGRP|S_IWGRP|S_IRGRP);
              save_config(NULL, "/mnt/rom/iDarcNes/");
              pz_error(_("Until you install tiwidgets your Rom folder will be /mnt/rom/iDarcNes/"));
             return 0;
      }
       else {
	    PzWindow * ret;
	     TWidget * wid;
	      ret = pz_new_window(_("iDarcNes Rom Directory"), PZ_WINDOW_NORMAL);
	      wid = ltinstw(10, 40, ret->w-20, 10+ttk_text_height(ttk_textfont), 0, "/mnt/", save_config);
	     ttk_add_widget(ret, wid);
	    ret = pz_finish_window(ret);
	   ltws(wid);
          return ret;
      }
}

static void cleanup()
{
	pz_browser_remove_handler(IF_NES);
}

static void init_launch() 
{
        const char * Binary;
        struct stat buf;

	Module = pz_register_module("iDarcNes", cleanup);
	Config = pz_load_config(pz_module_get_cfgpath(Module,"Config.conf"));
	Binary = pz_module_get_datapath(Module, "iDarcNes");

        if (!stat(Binary, &buf) == S_IXUSR || 00100)
            chmod(Binary, S_IRWXU);
				
	browser_extension.name = N_("Open with Nintedo NES");
	browser_extension.makesub = nes_file_handler;
	pz_browser_add_action (IF_NES, &browser_extension);
	pz_browser_set_handler(IF_NES, idarcnes_exec);
	
        pz_menu_add_action_group("/Setting/Select directory/iDarcNes", "Browse", set_directory);
        pz_menu_add_action_group("/Extras/Games/iDarcNes", "Emulator", idarcnes_browser);
}

PZ_MOD_INIT(init_launch)
