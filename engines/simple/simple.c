/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *                                                                              
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *                                                                              
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>

#include <gdk-pixbuf/gdk-pixdata.h>
#include <gmodule.h>

#include <xfce4-session/xfsm-splash-engine.h>

#include "fallback.h"


#define BORDER          2

#define DEFAULT_FONT    "Sans Bold 10"
#define DEFAULT_FGCOLOR "White"


typedef struct _Simple Simple;
struct _Simple
{
  gboolean     dialog_active;
  GdkWindow   *window;
  GdkPixmap   *pixmap;
  GdkGC       *gc;
  PangoLayout *layout;
  GdkRectangle area;
  GdkRectangle textbox;
};


static GdkFilterReturn
simple_filter (GdkXEvent *xevent, GdkEvent *event, gpointer user_data)
{
  Simple *simple = (Simple *) user_data;
  XVisibilityEvent *xvisev = (XVisibilityEvent *) xevent;

  switch (xvisev->type)
    {
    case VisibilityNotify:
      if (!simple->dialog_active)
        {
          gdk_window_raise (simple->window);
          return GDK_FILTER_REMOVE;
        }
      break;
    }

  return GDK_FILTER_CONTINUE;
}


static void
simple_setup (XfsmSplashEngine *engine, XfsmSplashRc *rc)
{
  PangoFontDescription *description;
  PangoFontMetrics     *metrics;
  PangoContext         *context;
  GdkWindowAttr         attr;
  GdkRectangle          geo;
  GdkWindow            *root;
  GdkPixbuf            *logo;
  GdkCursor            *cursor;
  GdkColor              color;
  Simple               *simple;
  gint                  logo_width;
  gint                  logo_height;
  gint                  text_height;

  simple = (Simple *) engine->user_data;

  root = gdk_screen_get_root_window (engine->primary_screen);
  gdk_screen_get_monitor_geometry (engine->primary_screen,
                                   engine->primary_monitor,
                                   &geo);

  logo = gdk_pixbuf_from_pixdata (&fallback, FALSE, NULL);
  logo_width = gdk_pixbuf_get_width (logo);
  logo_height = gdk_pixbuf_get_height (logo);

  cursor = gdk_cursor_new (GDK_WATCH);

  /* create pango layout */
  description = pango_font_description_from_string (DEFAULT_FONT);
  context = gdk_pango_context_get_for_screen (engine->primary_screen);
  pango_context_set_font_description (context, description);
  metrics = pango_context_get_metrics (context, description, NULL);
  text_height = (pango_font_metrics_get_ascent (metrics)
                 + pango_font_metrics_get_descent (metrics)) / PANGO_SCALE
                + 3;

  simple->area.width = logo_width + 2 * BORDER;
  simple->area.height = logo_height + text_height + 3 * BORDER;
  simple->area.x = (geo.width - simple->area.width) / 2;
  simple->area.y = (geo.height - simple->area.height) / 2;

  simple->layout = pango_layout_new (context);
  simple->textbox.x = BORDER;
  simple->textbox.y = simple->area.height - (text_height + BORDER);
  simple->textbox.width = simple->area.width - 2 * BORDER;
  simple->textbox.height = text_height;

  /* create splash window */
  attr.x                  = simple->area.x;
  attr.y                  = simple->area.y;
  attr.event_mask         = GDK_VISIBILITY_NOTIFY_MASK;
  attr.width              = simple->area.width;
  attr.height             = simple->area.height;
  attr.wclass             = GDK_INPUT_OUTPUT;
  attr.window_type        = GDK_WINDOW_TEMP;
  attr.cursor             = cursor;
  attr.override_redirect  = TRUE;

  simple->window = gdk_window_new (root, &attr, GDK_WA_X | GDK_WA_Y
                                  | GDK_WA_NOREDIR | GDK_WA_CURSOR);

  simple->pixmap = gdk_pixmap_new (simple->window,
                                   simple->area.width,
                                   simple->area.height,
                                   -1);

  gdk_window_set_back_pixmap (simple->window, simple->pixmap, FALSE);

  simple->gc = gdk_gc_new (simple->pixmap);
  
  gdk_gc_set_function (simple->gc, GDK_CLEAR);
  gdk_draw_rectangle (simple->pixmap,
                      simple->gc, TRUE,
                      0, 0,
                      simple->area.width,
                      simple->area.height);

  gdk_gc_set_function (simple->gc, GDK_COPY);
  gdk_draw_pixbuf (simple->pixmap,
                   simple->gc,
                   logo,
                   0, 0,
                   BORDER, BORDER,
                   logo_width,
                   logo_height,
                   GDK_RGB_DITHER_NONE, 0, 0);

  gdk_color_parse (DEFAULT_FGCOLOR, &color);
  gdk_gc_set_rgb_fg_color (simple->gc, &color);

  gdk_window_add_filter (simple->window, simple_filter, simple);
  gdk_window_show (simple->window);

  /* cleanup */
  pango_font_description_free (description);
  pango_font_metrics_unref (metrics);
  gdk_cursor_unref (cursor);
  g_object_unref (context);
  g_object_unref (logo);
}


static void
simple_next (XfsmSplashEngine *engine, const gchar *text)
{
  Simple *simple = (Simple *) engine->user_data;
  gint tw, th, tx, ty;
  GdkColor color;
  GdkGC *gc;

  g_print ("NEXT: \"%s\"\n", text);

  pango_layout_set_text (simple->layout, text, -1);
  pango_layout_get_pixel_size (simple->layout, &tw, &th);
  tx = simple->textbox.x + (simple->textbox.width - tw) / 2;
  ty = simple->textbox.y + (simple->textbox.height - th) / 2;

  gdk_gc_set_function (simple->gc, GDK_CLEAR);
  gdk_draw_rectangle (simple->pixmap,
                      simple->gc, TRUE,
                      simple->textbox.x,
                      simple->textbox.y,
                      simple->textbox.width,
                      simple->textbox.height);

  gdk_color_parse ("#333333", &color);
  gc = gdk_gc_new (simple->pixmap);
  gdk_gc_set_function (gc, GDK_COPY);
  gdk_gc_set_rgb_fg_color (gc, &color);
  gdk_draw_layout (simple->pixmap, gc, tx + 1, ty + 1, simple->layout);
  g_object_unref (gc);

  gdk_gc_set_function (simple->gc, GDK_COPY);
  gdk_draw_layout (simple->pixmap,
                   simple->gc, 
                   tx, ty,
                   simple->layout);

  gdk_window_clear_area (simple->window,
                         simple->textbox.x,
                         simple->textbox.y,
                         simple->textbox.width,
                         simple->textbox.height);
}


static int
simple_run (XfsmSplashEngine *engine,
            GtkWidget        *dialog)
{
  Simple *simple = (Simple *) engine->user_data;
  GtkRequisition requisition;
  int result;
  int x;
  int y;

  simple->dialog_active = TRUE;

  gtk_widget_size_request (dialog, &requisition);
  x = simple->area.x + (simple->area.width - requisition.width) / 2;
  y = simple->area.y + (simple->area.height - requisition.height) / 2;
  gtk_window_move (GTK_WINDOW (dialog), x, y);
  result = gtk_dialog_run (GTK_DIALOG (dialog));

  simple->dialog_active = FALSE;

  return result;
}


static void
simple_destroy (XfsmSplashEngine *engine)
{
  Simple *simple = (Simple *) engine->user_data;

  gdk_window_remove_filter (simple->window, simple_filter, simple);
  gdk_window_destroy (simple->window);
  g_object_unref (simple->layout);
  g_object_unref (simple->pixmap);
  g_object_unref (simple->gc);
  g_free (engine->user_data);
}


G_MODULE_EXPORT void
engine_init (XfsmSplashEngine *engine)
{
  Simple *simple;

  simple = g_new0 (Simple, 1);

  engine->user_data = simple;
  engine->setup = simple_setup;
  engine->next = simple_next;
  engine->run = simple_run;
  engine->destroy = simple_destroy;
}


G_MODULE_EXPORT void
config_init (XfsmSplashConfig *config)
{
  config->name        = g_strdup ("Simple");
  config->description = g_strdup ("Simple Splash Engine");
  config->version     = g_strdup (VERSION);
  config->author      = g_strdup ("Benedikt Meurer");
  config->homepage    = g_strdup ("http://www.xfce.org/");
}


