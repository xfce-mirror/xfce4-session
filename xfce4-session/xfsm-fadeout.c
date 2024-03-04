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

#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <cairo-xlib.h>



struct _XfsmFadeout
{
  Display *xdisplay;
  Window  *xwindow;
};



static Window
xfsm_x11_fadeout_new_window (GdkDisplay *display,
                             GdkScreen *screen)
{
  XSetWindowAttributes  attr;
  Display              *xdisplay;
  Window                xwindow;
  GdkWindow            *root;
  GdkCursor            *cursor;
  cairo_t              *cr;
  gint                  width;
  gint                  height;
  GdkPixbuf            *root_pixbuf;
  cairo_surface_t      *surface;
  gulong                mask = 0;
  gulong                opacity;
  gboolean              composited;
  gint                  scale;

  gdk_x11_display_error_trap_push (display);

  xdisplay = gdk_x11_display_get_xdisplay (display);
  root = gdk_screen_get_root_window (screen);

  width = gdk_window_get_width (root);
  height = gdk_window_get_height (root);

  composited = gdk_screen_is_composited (screen)
               && gdk_screen_get_rgba_visual (screen) != NULL;

  cursor = gdk_cursor_new_for_display (display, GDK_WATCH);

  scale = gdk_window_get_scale_factor (root);
  width *= scale;
  height *= scale;

  if (!composited)
    {
      /* create a copy of root window before showing the fadeout */
      root_pixbuf = gdk_pixbuf_get_from_window (root, 0, 0, width, height);
    }

  attr.cursor = gdk_x11_cursor_get_xcursor (cursor);
  mask |= CWCursor;

  attr.override_redirect = TRUE;
  mask |= CWOverrideRedirect;

  attr.background_pixel = BlackPixel (xdisplay, gdk_x11_screen_get_screen_number (screen));
  mask |= CWBackPixel;

  xwindow = XCreateWindow (xdisplay, gdk_x11_window_get_xid (root),
                           0, 0, width, height, 0, CopyFromParent,
                           InputOutput, CopyFromParent, mask, &attr);

  g_object_unref (cursor);

  if (composited)
    {
      /* apply transparency before map */
      opacity = 0.5 * 0xffffffff;
      XChangeProperty (xdisplay, xwindow,
                       gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_OPACITY"),
                       XA_CARDINAL, 32, PropModeReplace, (guchar *)&opacity, 1);
    }

  XMapWindow (xdisplay, xwindow);

  if (!composited)
    {
      /* create background for window */
      surface = cairo_xlib_surface_create (xdisplay, xwindow,
                                           gdk_x11_visual_get_xvisual (gdk_screen_get_system_visual (screen)),
                                           0, 0);
      cairo_xlib_surface_set_size (surface, width, height);
      cr = cairo_create (surface);

      /* draw the copy of the root window */
      gdk_cairo_set_source_pixbuf (cr, root_pixbuf, 0, 0);
      cairo_paint (cr);
      g_object_unref (root_pixbuf);

      /* draw black transparent layer */
      cairo_set_source_rgba (cr, 0, 0, 0, 0.5);
      cairo_paint (cr);
      cairo_destroy (cr);
      cairo_surface_destroy (surface);
    }

  gdk_display_flush (display);
  gdk_x11_display_error_trap_pop_ignored (display);

  return xwindow;
}



XfsmFadeout*
xfsm_fadeout_new (GdkDisplay *display)
{
  XfsmFadeout     *fadeout;
  GdkScreen       *screen;

  fadeout = g_slice_new0 (XfsmFadeout);

  fadeout->xdisplay = gdk_x11_display_get_xdisplay (display);
  screen = gdk_display_get_default_screen (display);
  fadeout->xwindow = GINT_TO_POINTER (xfsm_x11_fadeout_new_window (display, screen));

  return fadeout;
}



void
xfsm_fadeout_destroy (XfsmFadeout *fadeout)
{
  gdk_x11_display_error_trap_push (gdk_display_get_default ());
  XDestroyWindow (fadeout->xdisplay, GPOINTER_TO_INT (fadeout->xwindow));
  gdk_display_flush (gdk_display_get_default ());
  gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());

  g_slice_free (XfsmFadeout, fadeout);
}
