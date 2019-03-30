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
#include <gdk/gdkx.h>
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
  GdkPixbuf *pixbuf;
  GdkRGBA   *color;
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
  int          pixbuf_width;
  int          pixbuf_height;
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
                 GdkPixbuf      *pixbuf,
                 const GdkRGBA  *color,
                 GdkCursor      *cursor,
                 Mice           *mice)
{
  GdkRectangle  geometry;
  GdkWindowAttr attr;
  MiceWindow   *mice_window;

  gdk_screen_get_monitor_geometry (screen, monitor, &geometry);

  mice_window = g_new0 (MiceWindow, 1);
  mice_window->mice = mice;
  mice_window->pixbuf = GDK_PIXBUF (g_object_ref (pixbuf));
  mice_window->color = gdk_rgba_copy (color);

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

  gdk_window_set_background_rgba (mice_window->window, color);

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
  cairo_t    *cr;
  int         sx;
  int         sy;
  int         ww;
  int         wh;

  sx = mice->step * mice->base_width;
  sy = 0;

  for (lp = mice->windows; lp != NULL; lp = lp->next)
    {
      mice_window = MICE_WINDOW (lp->data);

      cr = gdk_cairo_create (mice_window->window);

      ww = gdk_window_get_width (GDK_WINDOW (mice_window->window));
      wh = gdk_window_get_height (GDK_WINDOW (mice_window->window));

      /* Paint the background */
      gdk_cairo_set_source_rgba (cr, mice_window->color);
      cairo_rectangle (cr, 0, 0, ww, wh);
      cairo_fill (cr);

      /* Paint the mouse. Cairo is fun, we have to move the source image
       * around and clip to the box to show the mouse animation */
      gdk_cairo_set_source_pixbuf (cr, mice_window->pixbuf, mice_window->x - sx, mice_window->y - sy);
      cairo_rectangle (cr, mice_window->x, mice_window->y, mice->base_width, mice->base_height);
      cairo_clip (cr);
      cairo_paint (cr);

      cairo_destroy (cr);
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
  GdkWindow    *root;
  GdkPixbuf    *pixbuf;
  GdkRGBA       color;
  GdkCursor    *cursor;
  GdkScreen    *screen;
  cairo_t      *cr;
  GList        *lp;
  Mice         *mice = MICE (engine->user_data);
  int           nmonitors;
  int           m;

  gdk_rgba_parse (&color, COLOR);
  cursor = gdk_cursor_new_for_display (engine->display, GDK_WATCH);

  /* load slide pixbuf */
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  /* TODO: use GResource or load it as a normal pixbuf */
  pixbuf = gdk_pixbuf_new_from_inline (-1, slide, FALSE, NULL);
  G_GNUC_END_IGNORE_DEPRECATIONS
  mice->pixbuf_width = gdk_pixbuf_get_width (pixbuf);
  mice->pixbuf_height = gdk_pixbuf_get_height (pixbuf);

  mice->base_width = mice->pixbuf_width / STEPS;
  mice->base_height = mice->pixbuf_height;
  mice->step = 0;
  mice->direction = 1;

  screen = gdk_display_get_default_screen (engine->display);
  nmonitors = gdk_screen_get_n_monitors (screen);
  root = gdk_screen_get_root_window (screen);

  /* create graphics context for this screen */
  cr = gdk_cairo_create (root);
  gdk_cairo_set_source_rgba (cr, &color);

  cairo_rectangle (cr, 0, 0, mice->pixbuf_width, mice->pixbuf_height);
  cairo_fill (cr);

  cairo_move_to (cr, 0, 0);
  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_paint (cr);

  for (m = 0; m < nmonitors; ++m)
    {
      mice_window = mice_window_new (screen, m, pixbuf,
                                     &color, cursor, mice);
      mice->windows = g_list_append (mice->windows, mice_window);

      if (screen == engine->primary_screen && m == engine->primary_monitor)
        mice->mainwin = mice_window;
    }
  cairo_destroy (cr);


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
  g_object_unref (cursor);
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

  ww = gdk_window_get_width (GDK_WINDOW (mainwin->window));
  wh = gdk_window_get_height (GDK_WINDOW (mainwin->window));

  gtk_window_set_screen (GTK_WINDOW (dialog),
                         gdk_window_get_screen (mainwin->window));
  gtk_widget_get_preferred_size (dialog, NULL, &requisition);
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
      g_object_unref (mice_window->pixbuf);
      gdk_rgba_free (mice_window->color);
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
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  /* TODO: use GResource or load it as a normal pixbuf */
  return gdk_pixbuf_new_from_inline (-1, preview, FALSE, NULL);
  G_GNUC_END_IGNORE_DEPRECATIONS
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


