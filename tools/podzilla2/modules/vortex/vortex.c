/*
 * Vortex
 *  
 * Copyright (C) 2005 Scott Lawrence
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "pz.h"
#include "console.h"
#include "vglobals.h"
#include "levels.h"

static vortex_globals vglob;

int Vortex_Rand( int max )
{
        return (int)((float)max * rand() / (RAND_MAX + 1.0));
}

void Vortex_ChangeToState( int st )
{
	Vortex_Console_WipeAllText();
	Vortex_Console_HiddenStatic( 0 );
	vglob.timer = 0;
	vglob.state = st;
}

void draw_vortex (PzWidget *widget, ttk_surface srf) 
{
	char buf[16];
	char * word;

	switch( vglob.state ) {
	case VORTEX_STATE_STARTUP:
		Vortex_Console_Render( srf, ttk_makecol( BLACK ));
		if( Vortex_Console_GetZoomCount() == 0 )
			Vortex_ChangeToState( VORTEX_STATE_LEVELSEL );
		break;

	case VORTEX_STATE_LEVELSEL:
		/* let the user select what level to start on */
		switch((vglob.timer>>3) & 0x03 ) {
		case 0: word = "SELECT"; break;
		case 1: word = "START";  break;
		case 2: word = "LEVEL";  break;
		case 3: word = "";       break;
		}
		pz_vector_string_center( srf, word,
			    (ttk_screen->w - ttk_screen->wx)>>1, 30,
			    10, 18, 1, ttk_makecol( BLACK ));

		snprintf( buf, 15, "%d", vglob.startLevel );
		pz_vector_string_center( srf, buf,
			    (ttk_screen->w - ttk_screen->wx)>>1, 60,
			    10, 18, 1, ttk_makecol( BLACK ));
		break;

	case VORTEX_STATE_GAME:
		/* do game stuff in here */
		Vortex_Console_Render( srf, ttk_makecol( BLACK ));
		pz_vector_string_center( srf, "[menu] to exit.",
                            (ttk_screen->w - ttk_screen->wx)>>1, 10,
                            5, 9, 1, ttk_makecol( BLACK ));

		pz_vector_string_center( srf, "unimplemented",
                            (ttk_screen->w - ttk_screen->wx)>>1, 30,
                            5, 9, 1, ttk_makecol( BLACK ));
		break;

	case VORTEX_STATE_DEATH:
		break;

	case VORTEX_STATE_DEAD:
		break;

	}
/*
	//ttk_line (srf, vglob.widget->x + 5, 10, vglob.widget->x + vglob.widget->w - 5, 10, ttk_makecol (DKGREY));
	printf( "%d %d\n", Vortex_Console_GetZoomCount(),
				Vortex_Console_GetStaticCount() );
*/
}

void cleanup_vortex() 
{
}


int event_vortex (PzEvent *ev) 
{
	switch (ev->type) {
	case PZ_EVENT_SCROLL:

		if( vglob.state == VORTEX_STATE_LEVELSEL ) {
			TTK_SCROLLMOD( ev->arg, 4 );
	   		/* adjust */ 
			vglob.startLevel += ev->arg;

			/* and clip */
			if( vglob.startLevel < 0 ) 
				vglob.startLevel = 0;
			if( vglob.startLevel > NLEVELS ) 
				vglob.startLevel = NLEVELS;
		}

		if( vglob.state == VORTEX_STATE_GAME ) {
			TTK_SCROLLMOD( ev->arg, 3 );
			if( ev->arg > 0 ) {
				Vortex_Console_AddItem( "BURRITO", 
					    Vortex_Rand(4)-2, Vortex_Rand(4)-2, 
					    VORTEX_STYLE_NORMAL );
			} else {
				Vortex_Console_AddItem( "MONTANA", 
					    Vortex_Rand(4)-2, Vortex_Rand(4)-2, 
					    VORTEX_STYLE_NORMAL );
			}
		}
		break;

	case PZ_EVENT_BUTTON_UP:
		switch( ev->arg ) {
		case( PZ_BUTTON_MENU ):
			pz_close_window (ev->wid->win);
			break;

		case( PZ_BUTTON_ACTION ):
			if( vglob.state == VORTEX_STATE_LEVELSEL )
				Vortex_ChangeToState( VORTEX_STATE_GAME );
			break;

		default:
			break;
		}
		break;

	case PZ_EVENT_DESTROY:
		cleanup_vortex();
		break;

	case PZ_EVENT_TIMER:
		Vortex_Console_Tick();
		vglob.timer++;
		ev->wid->dirty = 1;
		break;
	}
	return 0;
}

static void Vortex_Initialize( void )
{
	/* globals init */
	vglob.widget = NULL;
	vglob.state = VORTEX_STATE_STARTUP;
	vglob.level = 0;
	vglob.startLevel = 0;
	vglob.timer = 0;
}

PzWindow *new_vortex_window()
{
	//fp = fopen (pz_module_get_datapath (vglob.module, "message.txt"), "r");

	srand( time( NULL ));
	Vortex_Console_Init();

	vglob.window = pz_new_window( "Vortex", PZ_WINDOW_NORMAL );
	vglob.widget = pz_add_widget( vglob.window, draw_vortex, event_vortex );
	pz_widget_set_timer( vglob.widget, 40 );

	Vortex_Initialize( );
	Vortex_Console_HiddenStatic( 1 );
	Vortex_Console_AddItem( "VORTEX", 0, 0, VORTEX_STYLE_NORMAL );

	return pz_finish_window( vglob.window );
}

void init_vortex() 
{
	/* internal module name */
	vglob.module = pz_register_module ("vortex", cleanup_vortex);

	/* menu item display name */
	pz_menu_add_action ("/Extras/Games/Vortex WIP", new_vortex_window);

	Vortex_Initialize();
}

PZ_MOD_INIT (init_vortex)