/* $Id$ */
/*-
 * Copyright (c) 2004-2006 Benedikt Meurer <benny@xfce.org>
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

#include <gtk/gtk.h>

#include <xfce4-session/xfsm-fadeout.h>



#define COLOR "#b6c4d7"



typedef struct _FoScreen FoScreen;



static void xfsm_fadeout_drawable_mono (XfsmFadeout *fadeout,
                                        GdkDrawable *drawable);



struct _FoScreen
{
  GdkWindow *window;
  GdkPixmap *backbuf;
};

struct _XfsmFadeout
{
  GdkColor color;
  GList   *screens;
};



#if !GTK_CHECK_VERSION(2,8,0)
static char stipple_data[] = {
  ' ', '.',
  '.', ' ',
};
#endif



XfsmFadeout*
xfsm_fadeout_new (GdkDisplay *display)
{
  GdkWindowAttr  attr;
  GdkGCValues    values;
  XfsmFadeout   *fadeout;
  GdkWindow     *root;
  GdkCursor     *cursor;
  FoScreen      *screen;
  GdkGC         *gc;
  GList         *lp;
  gint           width;
  gint           height;
  gint           n;

  fadeout = g_new0 (XfsmFadeout, 1);
  gdk_color_parse (COLOR, &fadeout->color);

  cursor = gdk_cursor_new (GDK_WATCH);

  attr.x = 0;
  attr.y = 0;
  attr.event_mask = 0;
  attr.wclass = GDK_INPUT_OUTPUT;
  attr.window_type = GDK_WINDOW_TEMP;
  attr.cursor = cursor;
  attr.override_redirect = TRUE;

  for (n = 0; n < gdk_display_get_n_screens (display); ++n)
    {
      screen = g_new (FoScreen, 1);

      root = gdk_screen_get_root_window (gdk_display_get_screen (display, n));
      gdk_drawable_get_size (GDK_DRAWABLE (root), &width, &height);

      values.function = GDK_COPY;
      values.graphics_exposures = FALSE;
      values.subwindow_mode = TRUE;
      gc = gdk_gc_new_with_values (root, &values, GDK_GC_FUNCTION | GDK_GC_EXPOSURES | GDK_GC_SUBWINDOW);

      screen->backbuf = gdk_pixmap_new (GDK_DRAWABLE (root), width, height, -1);
      gdk_draw_drawable (GDK_DRAWABLE (screen->backbuf),
                         gc, GDK_DRAWABLE (root),
                         0, 0, 0, 0, width, height);
      xfsm_fadeout_drawable_mono (fadeout, GDK_DRAWABLE (screen->backbuf));

      attr.width = width;
      attr.height = height;

      screen->window = gdk_window_new (root, &attr, GDK_WA_X | GDK_WA_Y
                                       | GDK_WA_NOREDIR | GDK_WA_CURSOR);
      gdk_window_set_back_pixmap (screen->window, screen->backbuf, FALSE);

      g_object_unref (G_OBJECT (gc));

      fadeout->screens = g_list_append (fadeout->screens, screen);
    }

  for (lp = fadeout->screens; lp != NULL; lp = lp->next)
    gdk_window_show (((FoScreen *) lp->data)->window);

  gdk_cursor_unref (cursor);

  return fadeout;
}


void
xfsm_fadeout_destroy (XfsmFadeout *fadeout)
{
  FoScreen *screen;
  GList    *lp;

  for (lp = fadeout->screens; lp != NULL; lp = lp->next)
    {
      screen = lp->data;

      gdk_window_destroy (screen->window);
      g_object_unref (G_OBJECT (screen->backbuf));
      g_free (screen);
    }

  g_list_free (fadeout->screens);
  g_free (fadeout);
}


static void
xfsm_fadeout_drawable_mono (XfsmFadeout *fadeout,
                            GdkDrawable *drawable)
{
#if GTK_CHECK_VERSION(2,8,0)
  cairo_t *cr;

  /* using Xrender gives better results */
  cr = gdk_cairo_create (drawable);
  gdk_cairo_set_source_color (cr, &fadeout->color);
  cairo_paint_with_alpha (cr, 0.5);
  cairo_destroy (cr);
#else
  GdkGCValues  values;
  GdkBitmap   *bm;
  GdkGC       *gc;
  gint         width;
  gint         height;

  bm = gdk_bitmap_create_from_data (drawable, stipple_data, 2, 2);

  values.function = GDK_COPY;
  values.fill = GDK_STIPPLED;
  values.stipple = GDK_PIXMAP (bm);
  values.subwindow_mode = TRUE;

  gc = gdk_gc_new_with_values (drawable, &values,
                               GDK_GC_FUNCTION | GDK_GC_FILL |
                               GDK_GC_STIPPLE | GDK_GC_SUBWINDOW);

  gdk_gc_set_rgb_fg_color (gc, &fadeout->color);

  gdk_drawable_get_size (drawable, &width, &height);
  gdk_draw_rectangle (drawable, gc, TRUE,
                      0, 0, width, height);

  g_object_unref (G_OBJECT (gc));
  g_object_unref (G_OBJECT (bm));
#endif
}

