/*
 *
 * Copyright (C) 2008 jerl92
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

static PzModule *iboy_module;
static ttk_menu_item iboy_browser_extension;

#define iboy_binary "/usr/lib/iBoy/iBoy"

static int if_gb(const char *file)
{
  return (check_ext(file, ".gb") || check_ext(file, ".gbc"));
}

static PzWindow *iboy_load_file(const char *file)
{
        char * iboy_command = (char *) calloc(sizeof(iboy_command), sizeof(char));
	strcpy(iboy_command, "iBoy ");
	strcat(iboy_command, file);
	pz_exec(pz_module_get_datapath (iboy_module, iboy_command));
	return TTK_MENU_DONOTHING;
}

static PzWindow *iboy_file_handler(ttk_menu_item *item)
{
	return iboy_load_file(item->data);
}

static PzWindow *iboy_lunch()
{
	pz_exec(iboy_binary);
        return TTK_MENU_DONOTHING;
}

static void iboy_cleanup()
{
	pz_browser_remove_handler(if_gb);
}

static void init_iBoy() 
{
        struct stat nesBuf;

        iboy_module = pz_register_module("iBoy", iboy_cleanup);

        if (!stat(iboy_binary, &nesBuf) == S_IXUSR || 00100)
            chmod(iboy_binary, S_IRWXU);

	iboy_browser_extension.name = N_("Open with iBoy");
	iboy_browser_extension.makesub = iboy_file_handler;
	pz_browser_add_action(if_gb, &iboy_browser_extension);
	pz_browser_set_handler(if_gb, iboy_load_file);

 	pz_menu_add_action_group("/Extras/Games/Game Boy color", "Emulator", iboy_lunch);
}

PZ_MOD_INIT(init_iBoy)
