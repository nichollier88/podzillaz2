/*
 * Simple Clock - A simple module that displays the time
 */

#include <stdio.h>
#include <time.h>
#include "pz.h"

static void draw_clock(PzWidget *wid, ttk_surface srf) {
    time_t t;
    struct tm *tm;
    char buf[16];
    int w, h, x, y;

    t = time(NULL);
    tm = localtime(&t);

    /* Format the time as HH:MM:SS */
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);

    /* Clear the background */
    /* We use the window dimensions to ensure we cover the whole area */
    ttk_fillrect(srf, 0, 0, wid->win->w, wid->win->h, ttk_ap_getx("window.bg")->color);

    /* Calculate text position to center it */
    w = ttk_text_width(ttk_menufont, buf);
    h = ttk_text_height(ttk_menufont);
    x = (wid->win->w - w) / 2;
    y = (wid->win->h - h) / 2;

    /* Draw the time string */
    ttk_text(srf, ttk_menufont, x, y, ttk_ap_getx("window.fg")->color, buf);
}

static int event_clock(PzEvent *ev) {
    switch (ev->type) {
    case PZ_EVENT_BUTTON_UP:
        if (ev->arg == PZ_BUTTON_MENU) {
            pz_close_window(ev->wid->win);
            return 0;
        }
        break;
    case PZ_EVENT_TIMER:
        /* Mark widget as dirty to trigger redraw */
        ev->wid->dirty = 1;
        break;
    }
    return 0;
}

static PzWindow *create_clock_window(void) {
    /* Create a window with a timer that fires every 1000ms (1 second) */
    return pz_do_window("Simple Clock", PZ_WINDOW_NORMAL, draw_clock, event_clock, 1000);
}

static void init_simpleclock(void) {
    pz_register_module("simpleclock", NULL);
    pz_menu_add_action("/Extras/Simple Clock", create_clock_window);
}

PZ_MOD_INIT(init_simpleclock)