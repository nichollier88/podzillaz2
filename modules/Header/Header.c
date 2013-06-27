/*
 * Last updated: --
 * 
 * Copyright (C) 2008 courtc, keripo, jerl92
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
 

#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "pz.h"

static PzModule *module;
TWidget *(*textw)(int x, int y, int w, int h, int absheight, char * dt, int (*callback)(TWidget *, char *));
int (*textwstart)(TWidget * wid);

static int pz_set_header(TWidget * wid, char * txt)
{
  int reboot = pz_dialog(_("Reboot"), 
      _("Header Name have been saved, Need to reboot to take effect"), 2, 0, _("ok"), _("Reboot"));
    pz_set_string_setting (pz_global_config,HEADER, txt);
    pz_save_config(pz_global_config);
     if (reboot == 1)
       pz_ipod_reboot();
     else     
       pz_close_window(wid->win);
    return 0;
}

int text_available()
{
	if (!textw)
		textw = pz_module_softdep("tiwidgets", "ti_new_standard_text_widget");
	if (!textwstart)
	  textwstart = pz_module_softdep("tiwidgets", "ti_widget_start");
    return (!!textw & !!textwstart);
}

static PzWindow *set_Header()
{
  if(text_available()){
        PzWindow * ret;
 	TWidget * wid;
 	ret = pz_new_window(_("Set Header Name"), PZ_WINDOW_NORMAL);
	wid = textw(10, 40, ret->w-20, 10+ttk_text_height(ttk_textfont), 0, "Podzillaz", pz_set_header);
	ttk_add_widget(ret, wid);
	ret = pz_finish_window(ret);
	textwstart(wid);
	return ret;
    } else { 
	pz_warning("Install module text input");
	return TTK_MENU_DONOTHING;
  }
}
 
static void Header_init()
{
	module = pz_register_module("Header", NULL);
  	pz_menu_add_action_group("/Setting/Appearance/Header Name", "setting", set_Header);
}

PZ_MOD_INIT(Header_init)
