/* $Id$ */
/*-
 * Copyright (c) 2004 Benedikt Meurer <benny@xfce.org>
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

#include <xfce4-session/xfsm-fadeout.h>


struct _XfsmFadeout
{
  GdkColor color1;
  GdkColor color2;
  GList   *screens;
};


static void xfsm_fadeout_screen_mono (XfsmFadeout *fadeout, GdkScreen *screen);
static void xfsm_fadeout_screen_poly (XfsmFadeout *fadeout, GdkScreen *screen);


static char stipple_data[] = {
  ' ', '.',
  '.', ' ',
};


XfsmFadeout*
xfsm_fadeout_new (GdkDisplay *display)
{
  XfsmFadeout *fadeout;
  gint n;

  fadeout = g_new0 (XfsmFadeout, 1);
  gdk_color_parse ("#b6c4d7", &fadeout->color1);
  gdk_color_parse ("DarkRed", &fadeout->color2);

  if (gdk_color_equal (&fadeout->color1, &fadeout->color2))
    {
      for (n = 0; n < gdk_display_get_n_screens (display); ++n)
        xfsm_fadeout_screen_mono (fadeout, gdk_display_get_screen (display, n));
    }
  else
    {
      for (n = 0; n < gdk_display_get_n_screens (display); ++n)
        xfsm_fadeout_screen_poly (fadeout, gdk_display_get_screen (display, n));
    }

  return fadeout;
}


void
xfsm_fadeout_destroy (XfsmFadeout *fadeout)
{
  GdkWindowAttr attr;
  GdkWindow *window;
  GdkWindow *root;
  GList *lp;

  attr.x = 0;
  attr.y = 0;
  attr.event_mask = 0;
  attr.window_type = GDK_WINDOW_TOPLEVEL;
  attr.wclass = GDK_INPUT_OUTPUT;
  attr.override_redirect = TRUE;

  for (lp = fadeout->screens; lp != NULL; lp = lp->next)
    {
      root = gdk_screen_get_root_window (GDK_SCREEN (lp->data));

      gdk_drawable_get_size (GDK_DRAWABLE (root), &attr.width, &attr.height);

      window = gdk_window_new (root, &attr, GDK_WA_X | GDK_WA_Y |
                               GDK_WA_NOREDIR);

      gdk_window_show (window);
      gdk_flush ();
      gdk_window_hide (window);
      g_object_unref (G_OBJECT (window));
    }

  g_list_free (fadeout->screens);
  g_free (fadeout);
}


static void
xfsm_fadeout_screen_mono (XfsmFadeout *fadeout,
                          GdkScreen   *screen)
{
  GdkRectangle geometry;
  GdkGCValues values;
  GdkWindow *root;
  GdkBitmap *bm;
  GdkGC *gc;
  int m;

  root = gdk_screen_get_root_window (screen);

  bm = gdk_bitmap_create_from_data (root, stipple_data, 2, 2);

  values.function = GDK_COPY;
  values.fill = GDK_STIPPLED;
  values.stipple = GDK_PIXMAP (bm);
  values.subwindow_mode = TRUE;

  gc = gdk_gc_new_with_values (GDK_DRAWABLE (root), &values,
                               GDK_GC_FUNCTION | GDK_GC_FILL |
                               GDK_GC_STIPPLE | GDK_GC_SUBWINDOW);

  gdk_gc_set_rgb_fg_color (gc, &fadeout->color1);

  for (m = 0; m < gdk_screen_get_n_monitors (screen); ++m)
    {
      gdk_screen_get_monitor_geometry (screen, m, &geometry);

      gdk_draw_rectangle (GDK_DRAWABLE (root), gc, TRUE,
                          geometry.x, geometry.y,
                          geometry.width, geometry.height);
    }

  g_object_unref (G_OBJECT (gc));
  g_object_unref (G_OBJECT (bm));

  fadeout->screens = g_list_prepend (fadeout->screens, screen);
}


static void
fadeout_monitor_poly (GdkDrawable    *drawable,
                      GdkGC          *gc,
                      const GdkColor *color1,
                      const GdkColor *color2,
                      gint            x,
                      gint            y, 
                      gint            width,
                      gint            height)
{
  GdkColor color;
  gint     i;

  for (i = y; i < y + height; ++i)
    {
      color.red = color2->red + (i * (color1->red
            - color2->red) / height);
      color.green = color2->green + (i * (color1->green
            - color2->green) / height);
      color.blue = color2->blue + (i * (color1->blue
            - color2->blue) / height);

      gdk_gc_set_rgb_fg_color (gc, &color);
      gdk_draw_rectangle (drawable, gc, TRUE, x, y + i, width, 1);
    }
}


static void
xfsm_fadeout_screen_poly (XfsmFadeout *fadeout,
                          GdkScreen   *screen)
{
  GdkRectangle geometry;
  GdkGCValues values;
  GdkWindow *root;
  GdkBitmap *bm;
  GdkGC *gc;
  int m;

  root = gdk_screen_get_root_window (screen);

  bm = gdk_bitmap_create_from_data (root, stipple_data, 2, 2);

  values.function = GDK_COPY;
  values.fill = GDK_STIPPLED;
  values.stipple = GDK_PIXMAP (bm);
  values.subwindow_mode = TRUE;

  gc = gdk_gc_new_with_values (GDK_DRAWABLE (root), &values,
                               GDK_GC_FUNCTION | GDK_GC_FILL |
                               GDK_GC_STIPPLE | GDK_GC_SUBWINDOW);

  for (m = 0; m < gdk_screen_get_n_monitors (screen); ++m)
    {
      gdk_screen_get_monitor_geometry (screen, m, &geometry);

      fadeout_monitor_poly (GDK_DRAWABLE (root), gc,
                            &fadeout->color1, &fadeout->color2,
                            geometry.x, geometry.y,
                            geometry.width, geometry.height);
    }

  g_object_unref (G_OBJECT (gc));
  g_object_unref (G_OBJECT (bm));

  fadeout->screens = g_list_prepend (fadeout->screens, screen);
}

