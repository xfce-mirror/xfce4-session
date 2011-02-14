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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>

#include <gdk-pixbuf/gdk-pixdata.h>
#include <gmodule.h>

#include <libxfsm/xfsm-splash-engine.h>

#include <engines/mice/preview.h>
#include <engines/mice/slide.h>


#define BORDER  2
#define COLOR   "#DAE7FE"
#define STEPS   8


#define MICE_WINDOW(obj)  ((MiceWindow *)(obj))
#define MICE(obj)         ((Mice *)(obj))


typedef struct _MiceWindow MiceWindow;
typedef struct _Mice       Mice;


struct _MiceWindow
{
  GdkWindow *window;
  GdkPixmap *pixmap;
  GdkGC     *gc;
  int        x;
  int        y;
  Mice      *mice;
};


struct _Mice
{
  gboolean     dialog_active;
  GList       *windows;
  MiceWindow  *mainwin;
  int          base_width;
  int          base_height;
  int          step;
  int          direction;
  guint        timeout_id;
};


G_MODULE_EXPORT void config_init (XfsmSplashConfig *config);
G_MODULE_EXPORT void engine_init (XfsmSplashEngine *engine);


static GdkFilterReturn
mice_filter (GdkXEvent *xevent, GdkEvent *event, gpointer user_data)
{
  MiceWindow *mice_window = MICE_WINDOW (user_data);
  XVisibilityEvent *xvisev = (XVisibilityEvent *) xevent;

  switch (xvisev->type)
    {
    case VisibilityNotify:
      if (!mice_window->mice->dialog_active)
        {
          gdk_window_raise (mice_window->window);
          return GDK_FILTER_REMOVE;
        }
      break;
    }

  return GDK_FILTER_CONTINUE;
}


static MiceWindow*
mice_window_new (GdkScreen      *screen,
                 int             monitor,
                 GdkPixmap      *pixmap,
                 GdkGC          *gc,
                 const GdkColor *color,
                 GdkCursor      *cursor,
                 Mice           *mice)
{
  GdkRectangle  geometry;
  GdkWindowAttr attr;
  MiceWindow   *mice_window;

  gdk_screen_get_monitor_geometry (screen, monitor, &geometry);

  mice_window = g_new0 (MiceWindow, 1);
  mice_window->mice = mice;
  mice_window->pixmap = GDK_PIXMAP (g_object_ref (pixmap));
  mice_window->gc = GDK_GC (g_object_ref (gc));

  /* init win attributes */
  attr.x = geometry.x;
  attr.y = geometry.y;
  attr.event_mask = GDK_VISIBILITY_NOTIFY_MASK;
  attr.width = geometry.width;
  attr.height = geometry.height;
  attr.wclass = GDK_INPUT_OUTPUT;
  attr.window_type = GDK_WINDOW_TEMP;
  attr.cursor = cursor;
  attr.override_redirect = TRUE;

  mice_window->window = gdk_window_new (gdk_screen_get_root_window (screen),
                                        &attr, GDK_WA_X | GDK_WA_Y
                                        | GDK_WA_NOREDIR | GDK_WA_CURSOR);

  gdk_window_set_background (mice_window->window, color);

  /* center pixmap */
  mice_window->x = (geometry.width - mice->base_width) / 2;
  mice_window->y = (geometry.height - mice->base_height) / 2;

  return mice_window;
}


static void
mice_step (Mice *mice)
{
  MiceWindow *mice_window;
  GList      *lp;
  int         sx;
  int         sy;

  sx = mice->step * mice->base_width;
  sy = 0;

  for (lp = mice->windows; lp != NULL; lp = lp->next)
    {
      mice_window = MICE_WINDOW (lp->data);
      gdk_draw_drawable (mice_window->window,
                         mice_window->gc,
                         mice_window->pixmap,
                         sx, sy,
                         mice_window->x,
                         mice_window->y,
                         mice->base_width,
                         mice->base_height);
    }

  if (mice->step == 0 && mice->direction < 0)
    {
      mice->step++;
      mice->direction = 1;
    }
  else if (mice->step == STEPS - 1 && mice->direction > 0)
    {
      mice->step--;
      mice->direction = -1;
    }
  else
    {
      mice->step += mice->direction;
    }
}


static gboolean
mice_timeout (gpointer user_data)
{
  Mice *mice = MICE (user_data);

  if (!mice->dialog_active)
    mice_step (mice);

  return TRUE;
}


static void
mice_setup (XfsmSplashEngine *engine,
            XfsmSplashRc     *rc)
{
  MiceWindow   *mice_window;
  GdkGCValues   gc_values;
  GdkColormap  *cmap;
  GdkWindow    *root;
  GdkPixmap    *pixmap;
  GdkPixbuf    *pixbuf;
  GdkColor      color;
  GdkCursor    *cursor;
  GdkScreen    *screen;
  GdkGC        *gc;
  GList        *lp;
  Mice         *mice = MICE (engine->user_data);
  int           pw, ph;
  int           nscreens;
  int           nmonitors;
  int           n, m;

  gdk_color_parse (COLOR, &color);
  cursor = gdk_cursor_new (GDK_WATCH);

  /* load slide pixbuf */
  pixbuf = gdk_pixbuf_new_from_inline (-1, slide, FALSE, NULL);
  pw = gdk_pixbuf_get_width (pixbuf);
  ph = gdk_pixbuf_get_height (pixbuf);

  mice->base_width = pw / STEPS;
  mice->base_height = ph;
  mice->step = 0;
  mice->direction = 1;

  nscreens = gdk_display_get_n_screens (engine->display);
  for (n = 0; n < nscreens; ++n)
    {
      screen = gdk_display_get_screen (engine->display, n);
      nmonitors = gdk_screen_get_n_monitors (screen);
      root = gdk_screen_get_root_window (screen);

      /* allocate color */
      cmap = gdk_drawable_get_colormap (root);
      gdk_rgb_find_color (cmap, &color);

      /* create graphics context for this screen */
      gc_values.function = GDK_COPY;
      gc_values.graphics_exposures = FALSE;
      gc_values.foreground = color;
      gc = gdk_gc_new_with_values (root, &gc_values, GDK_GC_FUNCTION
                                  | GDK_GC_EXPOSURES | GDK_GC_FOREGROUND);

      /* create pixmap for this screen */
      pixmap = gdk_pixmap_new (root, pw, ph, -1);
      gdk_draw_rectangle (pixmap, gc, TRUE, 0, 0, pw, ph);
      gdk_draw_pixbuf (pixmap, gc, pixbuf, 0, 0, 0, 0,
                       pw, ph, GDK_RGB_DITHER_NONE, 0, 0);

      for (m = 0; m < nmonitors; ++m)
        {
          mice_window = mice_window_new (screen, m, pixmap, gc,
                                         &color, cursor, mice);
          mice->windows = g_list_append (mice->windows, mice_window);

          if (screen == engine->primary_screen && m == engine->primary_monitor)
            mice->mainwin = mice_window;
        }

      /* cleanup for this screen */
      g_object_unref (pixmap);
      g_object_unref (gc);
    }

  /* show all windows and connect filters */
  for (lp = mice->windows; lp != NULL; lp = lp->next)
    {
      mice_window = MICE_WINDOW (lp->data);
      gdk_window_show (mice_window->window);
      gdk_window_add_filter (mice_window->window, mice_filter, mice_window);
    }

  /* start timer */
  mice->timeout_id = g_timeout_add (100, mice_timeout, mice);

  /* cleanup */
  g_object_unref (pixbuf);
  gdk_cursor_unref (cursor);
}


static void
mice_next (XfsmSplashEngine *engine, const gchar *text)
{
  /* nothing to be done here */
}


static int
mice_run (XfsmSplashEngine *engine,
          GtkWidget        *dialog)
{
  Mice *mice = MICE (engine->user_data);
  MiceWindow *mainwin = mice->mainwin;
  GtkRequisition requisition;
  int result;
  int x, y;
  int wx, wy;
  int ww, wh;

  mice->dialog_active = TRUE;

  gdk_window_get_origin (mainwin->window, &wx, &wy);
  gdk_drawable_get_size (mainwin->window, &ww, &wh);
  gtk_window_set_screen (GTK_WINDOW (dialog),
                         gdk_drawable_get_screen (mainwin->window));
  gtk_widget_size_request (dialog, &requisition);
  x = wx + (ww - requisition.width) / 2;
  y = wy + (wh - requisition.height) / 2;
  gtk_window_move (GTK_WINDOW (dialog), x, y);
  result = gtk_dialog_run (GTK_DIALOG (dialog));

  mice->dialog_active = FALSE;

  return result;
}


static void
mice_destroy (XfsmSplashEngine *engine)
{
  MiceWindow *mice_window;
  Mice       *mice = MICE (engine->user_data);
  GList      *lp;

  for (lp = mice->windows; lp != NULL; lp = lp->next)
    {
      mice_window = MICE_WINDOW (lp->data);
      gdk_window_remove_filter (mice_window->window, mice_filter, mice);
      gdk_window_destroy (mice_window->window);
      g_object_unref (mice_window->pixmap);
      g_object_unref (mice_window->gc);
      g_free (mice_window);
    }

  g_source_remove (mice->timeout_id);
  g_list_free (mice->windows);
  g_free (mice);
}


G_MODULE_EXPORT void
engine_init (XfsmSplashEngine *engine)
{
  Mice *mice;

  mice = g_new0 (Mice, 1);

  engine->user_data = mice;
  engine->setup = mice_setup;
  engine->next = mice_next;
  engine->run = mice_run;
  engine->destroy = mice_destroy;
}



static GdkPixbuf*
config_preview (XfsmSplashConfig *config)
{
  return gdk_pixbuf_new_from_inline (-1, preview, FALSE, NULL);
}


G_MODULE_EXPORT void
config_init (XfsmSplashConfig *config)
{
  config->name        = g_strdup (_("Mice"));
  config->description = g_strdup (_("Mice Splash Engine"));
  config->version     = g_strdup (VERSION);
  config->author      = g_strdup ("Benedikt Meurer");
  config->homepage    = g_strdup ("http://www.xfce.org/");

  config->preview     = config_preview;
}


