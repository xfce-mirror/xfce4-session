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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include <xfce4-session/xfsm-fadeout.h>



#define COLOR "#b6c4d7"



typedef struct _FoScreen FoScreen;



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



XfsmFadeout*
xfsm_fadeout_new (GdkDisplay *display)
{
  GdkWindowAttr  attr;
  XfsmFadeout   *fadeout;
  GdkWindow     *root;
  GdkCursor     *cursor;
  FoScreen      *screen;
  cairo_t       *cr;
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
      GdkPixbuf *root_pixbuf;

      screen = g_new (FoScreen, 1);

      root = gdk_screen_get_root_window (gdk_display_get_screen (display, n));
      gdk_drawable_get_size (GDK_DRAWABLE (root), &width, &height);

      screen->backbuf = gdk_pixmap_new (GDK_DRAWABLE (root), width, height, -1);

      /* Copy the root window */
      root_pixbuf = gdk_pixbuf_get_from_drawable (NULL, GDK_DRAWABLE (root), NULL,
                                                  0, 0, 0, 0, width, height);
      cr = gdk_cairo_create (GDK_DRAWABLE (screen->backbuf));
      gdk_cairo_set_source_pixbuf (cr, root_pixbuf, 0, 0);
      cairo_paint (cr);
      gdk_cairo_set_source_color (cr, &fadeout->color);
      cairo_paint_with_alpha (cr, 0.5);

      attr.width = width;
      attr.height = height;

      screen->window = gdk_window_new (root, &attr, GDK_WA_X | GDK_WA_Y
                                       | GDK_WA_NOREDIR | GDK_WA_CURSOR);
      gdk_window_set_back_pixmap (screen->window, screen->backbuf, FALSE);

      g_object_unref (root_pixbuf);
      cairo_destroy (cr);

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
