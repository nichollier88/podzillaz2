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
static PzConfig *nesConfig;
static PzModule *nesModule;
static const char *nesBinary;
static ttk_menu_item nesExtension;

extern TWindow *open_directory_ext(const char *filename, const char *header, int ext(const char *file));
static TWidget *(*ltinstw)(int x, int y, int w, int h, int absheight,
		char *dt, int (*callback)(TWidget *, char *));
static int (*ltws)(TWidget * wid);

static int text_available()
{
	if (!ltws)
		ltws = pz_module_softdep("tiwidgets", "ti_widget_start");
	if (!ltinstw)
		ltinstw = pz_module_softdep("tiwidgets",
				"ti_new_standard_text_widget");
	return (!!ltinstw & !!ltws);
}

static int ifNes(const char *file)
{
  return check_ext(file, ".nes");
}

static PzWindow *idarcnes_exec(const char *file)
{
        char * nes_command = (char *) calloc(sizeof(nes_command), sizeof(char));
	strcpy(nes_command, "iDarcNes ");
	strcat(nes_command, file);
	pz_exec(pz_module_get_datapath (nesModule, nes_command));
	return TTK_MENU_DONOTHING;
}

static PzWindow *nesHandler(ttk_menu_item *item)
{
	return idarcnes_exec(item->data);
}

static PzWindow *nesBrowser()
{
        const char *nesDirectory;
        nesDirectory = pz_get_string_setting(nesConfig, 1);
        if (!nesConfig){
           pz_error(_("No directory have been select."));
           return TTK_MENU_UPALL;
           pz_error(_("Go to /Setting/Select directory/iDarcNes to set it"));
           return 0;
        }
         else { 
           return open_directory_ext(nesDirectory, "iDarcNes", ifNes);
        } 
}

static int nesSaveConfig(TWidget * wid, char * txt)
{
        pz_set_string_setting(nesConfig, 1, txt);
        pz_save_config(nesConfig);
        pz_close_window(wid->win);
        return 0;
}

static PzWindow * nesDirectory()
{
        if (!text_available()) {
           pz_error(_("Tiwidgets(Text input) is missing."));
            mkdir("/mnt/rom/iDarcNes/", S_IFDIR|S_IRUSR|S_IWUSR|S_IXUSR|S_IXGRP|S_IWGRP|S_IRGRP);
             nesSaveConfig(NULL, "/mnt/rom/iDarcNes/");
            pz_error(_("Until you install tiwidgets your Rom folder will be /mnt/rom/iDarcNes/"));
          return TTK_MENU_UPONE;
         } else {
	  PzWindow * ret;
	   TWidget * wid;
	    ret = pz_new_window(_("iDarcNes Rom Directory"), PZ_WINDOW_NORMAL);
             wid = ltinstw(10, 40, ret->w-20, 10+ttk_text_height(ttk_textfont), 0, "/mnt/", nesSaveConfig);
	    ttk_add_widget(ret, wid);
 	   ltws(wid);
          return pz_finish_window(ret);
        }
}

static void nesCleanup()
{
	pz_browser_remove_handler(ifNes);
}

static void init_idarcnes() 
{
        struct stat nesBuf;

	nesModule = pz_register_module("iDarcNes", nesCleanup);
        nesConfig = pz_load_config(pz_module_get_cfgpath(nesModule,"Config.conf"));
	nesBinary = pz_module_get_datapath(nesModule, "iDarcNes");

        if (!stat(nesBinary, &nesBuf) == S_IXUSR || 00100)
            chmod(nesBinary, S_IRWXU);
				
	nesExtension.name = N_("Open with Nintedo NES");
	nesExtension.makesub = nesHandler;
	pz_browser_add_action(ifNes, &nesExtension);
	pz_browser_set_handler(ifNes, idarcnes_exec);
	
        pz_menu_add_action_group("/Setting/Select directory/iDarcNes", "Browse", nesDirectory);
        pz_menu_add_action_group("/Extras/Games/iDarcNes", "Emulator", nesBrowser);
}

PZ_MOD_INIT(init_idarcnes)
