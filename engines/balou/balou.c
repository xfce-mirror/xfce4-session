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

#include <engines/balou/balou.h>


#define BALOU_INCREMENT   2




static void            balou_window_init     (BalouWindow  *window,
                                              GdkScreen    *screen,
                                              int           monitor,
                                              GdkWindow    *root,
                                              GdkCursor    *cursor);
static void            balou_window_destroy  (BalouWindow  *window);
#if 0
static void            balou_window_set_text (BalouWindow  *window,
                                              const gchar  *text);
#endif
static GdkFilterReturn balou_window_filter   (GdkXEvent    *xevent,
                                              GdkEvent     *event,
                                              gpointer      data);


struct _BalouWindow
{
  GdkWindow   *window;
  GdkPixmap   *backbuf;
  PangoLayout *layout;
  GdkGC       *gc_copy;
  GdkGC       *gc_set;
  GdkRectangle area;
  GdkRectangle logobox;
  GdkRectangle textbox;

  GtkWidget   *wmwindow;

  gboolean     dialog_active;
};



void
balou_init (Balou        *balou,
            GdkDisplay   *display,
            GdkScreen    *mainscreen,
            gint          mainmonitor,
            BalouTheme   *theme)
{
  PangoFontDescription *description;
  PangoFontMetrics     *metrics;
  PangoContext         *context;
  PangoLayout          *layout;
  BalouWindow          *window;
  GdkColormap          *cmap;
  GdkCursor            *cursor;
  GdkScreen            *screen;
  GdkWindow            *root;
  GdkPixbuf            *pb;
  GdkGCValues           gc_values;
  GdkGCValuesMask       gc_mask;
  GdkGC                *gc_copy;
  GdkGC                *gc_set;
  gint                  layout_height;
  gint                  nmonitors;
  gint                  nscreens;
  gint                  i;
  gint                  n;
  gint                  m;
  gint                  px;
  gint                  py;
  gint                  pw;
  gint                  ph;

  balou->theme = theme;

  balou_theme_get_bgcolor (theme, &balou->bgcolor);
  balou_theme_get_fgcolor (theme, &balou->fgcolor);

  cursor = gdk_cursor_new (GDK_WATCH);
  description = pango_font_description_from_string (balou_theme_get_font (theme));

  /* determine number of required windows */
  nscreens = gdk_display_get_n_screens (display);
  for (n = 0; n < nscreens; ++n)
    {
      screen = gdk_display_get_screen (display, n);
      nmonitors = gdk_screen_get_n_monitors (screen);
      for (m = 0; m < nmonitors; ++m)
        balou->nwindows++;
    }

  /* create windows */
  balou->windows = g_new (BalouWindow, balou->nwindows);
  for (i = n = 0; n < nscreens; ++n)
    {
      screen = gdk_display_get_screen (display, n);
      nmonitors = gdk_screen_get_n_monitors (screen);
      root = gdk_screen_get_root_window (screen);

      /* allocate fore/background colors */
      cmap = gdk_drawable_get_colormap (root);
      gdk_rgb_find_color (cmap, &balou->bgcolor);
      gdk_rgb_find_color (cmap, &balou->fgcolor);

      /* create pango layout for this screen */
      context = gdk_pango_context_get_for_screen (screen);
      pango_context_set_font_description (context, description);
      layout = pango_layout_new (context);
      metrics = pango_context_get_metrics (context, description, NULL);
      layout_height = (pango_font_metrics_get_ascent (metrics)
                      + pango_font_metrics_get_descent (metrics)) / PANGO_SCALE
                     + 3;
      pango_font_metrics_unref (metrics);

      /* create graphics contexts for this screen */
      gc_mask = GDK_GC_FUNCTION | GDK_GC_EXPOSURES;
      gc_values.function = GDK_COPY;
      gc_values.graphics_exposures = FALSE;
      gc_copy = gdk_gc_new_with_values (root, &gc_values, gc_mask);
      gc_mask |= GDK_GC_FOREGROUND | GDK_GC_BACKGROUND;
      gc_values.foreground = balou->bgcolor;
      gc_values.background = balou->fgcolor;
      gc_set = gdk_gc_new_with_values (root, &gc_values, gc_mask);

      for (m = 0; m < nmonitors; ++m)
        {
          window = balou->windows + i;
          balou_window_init (window, screen, m, root, cursor);

          window->gc_copy = GDK_GC (g_object_ref (gc_copy));
          window->gc_set = GDK_GC (g_object_ref (gc_set));
          window->layout = PANGO_LAYOUT (g_object_ref (layout));

          /* calculate box dimensions */
          window->logobox         = window->area;
          window->logobox.x       = 0;
          window->logobox.height -= layout_height;
          window->textbox         = window->area;
          window->textbox.x       = 0;
          window->textbox.y      += window->logobox.height;
          window->textbox.height -= window->logobox.height;

          balou_theme_draw_gradient (balou->theme,
                                     window->backbuf,
                                     gc_copy,
                                     window->logobox,
                                     window->textbox);

          gdk_gc_set_rgb_fg_color (gc_copy, &balou->fgcolor);

          if (mainscreen == screen && mainmonitor == m)
            balou->mainwin = window;

          ++i;
        }

      g_object_unref (context);
      g_object_unref (layout);
      g_object_unref (gc_copy);
      g_object_unref (gc_set);
    }

  /* show splash windows */
  for (i = 0; i < balou->nwindows; ++i)
    {
      window = balou->windows + i;

      gtk_widget_show_now (window->wmwindow);
      /*gdk_window_set_back_pixmap (window->wmwindow->window,
                                  window->backbuf, FALSE);*/
      gdk_window_add_filter (window->wmwindow->window,
                             balou_window_filter,
                             window);

      gdk_window_show (window->window);
      gdk_window_add_filter (window->window,
                             balou_window_filter,
                             window);
    }
  gdk_flush ();

  /* display logo pixbuf (if any) */
  window = balou->mainwin;
  pb = balou_theme_get_logo (balou->theme,
                             window->logobox.width,
                             window->logobox.height);
  if (G_LIKELY (pb != NULL))
    {
      pw = gdk_pixbuf_get_width (pb);
      ph = gdk_pixbuf_get_height (pb);
      px = (window->logobox.width - pw) / 2;
      py = (window->logobox.height - ph) / 2;

      gdk_draw_pixbuf (window->backbuf, window->gc_copy, pb, 0, 0,
                       px, py, pw, ph, GDK_RGB_DITHER_NONE, 0, 0);
      gdk_window_clear_area (window->window, px, py, pw, ph);
      g_object_unref (pb);
    }

  /* create fader pixmap */
  balou->fader_pm = gdk_pixmap_new (window->window,
                                    window->textbox.width,
                                    window->textbox.height,
                                    -1);
  gdk_draw_rectangle (balou->fader_pm, window->gc_set, TRUE, 0, 0,
                      window->textbox.width, window->textbox.height);

  pango_font_description_free (description);
  gdk_cursor_unref (cursor);
}


void
balou_fadein (Balou *balou, const gchar *text)
{
  BalouWindow *window = balou->mainwin;
  GdkRectangle area;
  gint         median;
  gint         th;
  gint         tw;
  gint         x;

  pango_layout_set_text (window->layout, text, -1);
  pango_layout_get_pixel_size (window->layout, &tw, &th);

  area.x      = window->textbox.x + BALOU_INCREMENT;
  area.y      = window->textbox.y + (window->textbox.height - th) / 2;
  area.width  = tw + BALOU_INCREMENT;
  area.height = th;

  gdk_draw_rectangle (balou->fader_pm, window->gc_set, TRUE, 0, 0,
                      window->textbox.width, window->textbox.height);
  gdk_draw_layout (balou->fader_pm, window->gc_copy,
                   BALOU_INCREMENT, 0, window->layout);

  median = (window->area.width - area.width) / 2;
  for (x = 0; (median - x) > BALOU_INCREMENT; x += BALOU_INCREMENT)
    {
      gdk_draw_drawable (window->window, window->gc_copy, balou->fader_pm,
                         0, 0, area.x + x, area.y, area.width, area.height);

      gdk_flush ();

      g_main_context_iteration (NULL, FALSE);
    }

  area.x += median;
  balou->fader_area = area;

  gdk_draw_rectangle (window->backbuf,
                      window->gc_set, TRUE,
                      window->textbox.x,
                      window->textbox.y,
                      window->textbox.width,
                      window->textbox.height);

  gdk_draw_drawable (window->backbuf, window->gc_copy, balou->fader_pm,
                     0, 0, area.x, area.y, area.width, area.height);

  gdk_window_clear_area (window->window,
                         window->textbox.x,
                         window->textbox.y,
                         window->textbox.width,
                         window->textbox.height);
}


void
balou_fadeout (Balou *balou)
{
  BalouWindow *window = balou->mainwin;
  GdkRectangle area = balou->fader_area;
  gint         left;

  left = (window->textbox.x + window->textbox.width) - BALOU_INCREMENT;
  for (; area.x < left; area.x += BALOU_INCREMENT)
    {
      gdk_draw_drawable (window->window, window->gc_copy, balou->fader_pm,
                         0, 0, area.x, area.y, area.width, area.height);

      gdk_flush ();

      g_main_context_iteration (NULL, FALSE);
    }

  gdk_draw_rectangle (window->backbuf,
                      window->gc_set, TRUE,
                      window->textbox.x,
                      window->textbox.y,
                      window->textbox.width,
                      window->textbox.height);

  gdk_window_clear_area (window->window,
                         window->textbox.x,
                         window->textbox.y,
                         window->textbox.width,
                         window->textbox.height);
}


int
balou_run (Balou     *balou,
           GtkWidget *dialog)
{
  GtkRequisition requisition;
  BalouWindow   *window = balou->mainwin;
  int            result;
  int            x;
  int            y;

  window->dialog_active = TRUE;

  gtk_widget_size_request (dialog, &requisition);
  x = window->area.x + (window->area.width - requisition.width) / 2;
  y = window->area.y + (window->area.height - requisition.height) / 2;
  gtk_window_move (GTK_WINDOW (dialog), x, y);
  result = gtk_dialog_run (GTK_DIALOG (dialog));

  window->dialog_active = FALSE;

  return result;
}


void
balou_destroy (Balou *balou)
{
  gint i;

  balou_theme_destroy (balou->theme);

  for (i = 0; i < balou->nwindows; ++i)
    balou_window_destroy (balou->windows + i);
  g_free (balou->windows);

  if (balou->fader_pm != NULL)
    g_object_unref (balou->fader_pm);
}


static void
balou_window_init (BalouWindow  *window,
                   GdkScreen    *screen,
                   int           monitor,
                   GdkWindow    *root,
                   GdkCursor    *cursor)
{
  GdkWindowAttr attr;

  /* acquire monitor geometry */
  gdk_screen_get_monitor_geometry (screen, monitor, &window->area);

  /* create splash window */
  attr.x                  = window->area.x;
  attr.y                  = window->area.y;
  attr.event_mask         = GDK_VISIBILITY_NOTIFY_MASK;
  attr.width              = window->area.width;
  attr.height             = window->area.height;
  attr.wclass             = GDK_INPUT_OUTPUT;
  attr.window_type        = GDK_WINDOW_TEMP;
  attr.cursor             = cursor;
  attr.override_redirect  = TRUE;

  window->window = gdk_window_new (root, &attr, GDK_WA_X | GDK_WA_Y
                                  | GDK_WA_CURSOR | GDK_WA_NOREDIR);

  /* create wm window (for tricking the window manager to avoid flicker) */
  window->wmwindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_move (GTK_WINDOW (window->wmwindow), 0, 0);
  gtk_window_resize (GTK_WINDOW (window->wmwindow), 1, 1);
  gtk_window_set_decorated (GTK_WINDOW (window->wmwindow), FALSE);
  gtk_window_set_screen (GTK_WINDOW (window->wmwindow), screen);
  gtk_window_set_skip_pager_hint (GTK_WINDOW (window->wmwindow), TRUE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window->wmwindow), TRUE);

  window->backbuf = gdk_pixmap_new (window->window,
                                    window->area.width,
                                    window->area.height,
                                    -1);

  gdk_window_set_back_pixmap (window->window, window->backbuf, FALSE);
}


static void
balou_window_destroy (BalouWindow *window)
{
  gdk_window_remove_filter (window->window, balou_window_filter, window);
  if (GTK_WIDGET_REALIZED (window->wmwindow))
    {
      gdk_window_remove_filter (window->wmwindow->window,
                                balou_window_filter,
                                window);
    }

  gdk_window_destroy (window->window);
  gtk_widget_destroy (window->wmwindow);
  g_object_unref (window->backbuf);
  g_object_unref (window->layout);
  g_object_unref (window->gc_copy);
  g_object_unref (window->gc_set);
}


#if 0
static void
balou_window_set_text (BalouWindow *window, const gchar *text)
{
  gint text_x;
  gint text_y;
  gint text_width;
  gint text_height;

  pango_layout_set_markup (window->layout, text, -1);
  pango_layout_get_pixel_size (window->layout, &text_width, &text_height);

  text_x = (window->textbox.width - text_width) / 2
          + window->textbox.x;
  text_y = (window->textbox.height - text_height) / 2
          + window->textbox.y;

  gdk_draw_rectangle (window->backbuf,
                      window->gc_set,
                      TRUE,
                      window->textbox.x,
                      window->textbox.y,
                      window->textbox.width,
                      window->textbox.height);

  gdk_draw_layout (window->backbuf, window->gc_copy,
                   text_x, text_y, window->layout);

  gdk_window_clear_area (window->window,
                         window->textbox.x,
                         window->textbox.y,
                         window->textbox.width,
                         window->textbox.height);
}
#endif


static GdkFilterReturn
balou_window_filter (GdkXEvent *xevent,
                     GdkEvent  *event,
                     gpointer   data)
{
  XVisibilityEvent *xvisev = (XVisibilityEvent *) xevent;
  BalouWindow      *window = (BalouWindow *) data;

  if (!window->dialog_active)
    {
      switch (xvisev->type)
        {
        case VisibilityNotify:
          /* something obscured the splash window */
          gdk_window_raise (window->window);
          return GDK_FILTER_REMOVE;

        case ReparentNotify:
          /* window manager is up */
          gdk_window_raise (window->window);
          break;
        }
    }

  return GDK_FILTER_CONTINUE;
}


