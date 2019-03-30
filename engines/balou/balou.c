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

#include <engines/balou/balou.h>


#define BALOU_INCREMENT   2




static void            balou_window_init     (BalouWindow  *window,
                                              GdkScreen    *screen,
                                              int           monitor,
                                              GdkWindow    *root,
                                              GdkCursor    *cursor);
static void            balou_window_destroy  (BalouWindow  *window);

static GdkFilterReturn balou_window_filter   (GdkXEvent    *xevent,
                                              GdkEvent     *event,
                                              gpointer      data);


struct _BalouWindow
{
  GdkWindow   *window;
  PangoLayout *layout;
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
  GdkCursor            *cursor;
  GdkScreen            *screen;
  GdkWindow            *root;
  GdkPixbuf            *pb;
  cairo_t              *cr;
  gint                  layout_height;
  gint                  nmonitors;
  gint                  i;
  gint                  m;
  gint                  px;
  gint                  py;
  gint                  pw;
  gint                  ph;
  gint                  ww;
  gint                  wh;

  balou->theme = theme;

  balou_theme_get_bgcolor (theme, &balou->bgcolor);
  balou_theme_get_fgcolor (theme, &balou->fgcolor);

  cursor = gdk_cursor_new_for_display (display, GDK_WATCH);
  description = pango_font_description_from_string (balou_theme_get_font (theme));

  /* determine number of required windows */
  screen = gdk_display_get_default_screen (display);
  nmonitors = gdk_screen_get_n_monitors (screen);
  for (m = 0; m < nmonitors; ++m)
    balou->nwindows++;

  /* create windows */
  balou->windows = g_new (BalouWindow, balou->nwindows);
  screen = gdk_display_get_default_screen (display);
  nmonitors = gdk_screen_get_n_monitors (screen);
  root = gdk_screen_get_root_window (screen);

  /* create pango layout for this screen */
  context = gdk_pango_context_get_for_screen (screen);
  pango_context_set_font_description (context, description);
  layout = pango_layout_new (context);
  metrics = pango_context_get_metrics (context, description, NULL);
  layout_height = (pango_font_metrics_get_ascent (metrics)
                 + pango_font_metrics_get_descent (metrics)) / PANGO_SCALE
                 + 3;
  pango_font_metrics_unref (metrics);

  for (m = 0; m < nmonitors; ++m)
    {
      cairo_t *window_cr;
      window = balou->windows + m;
      balou_window_init (window, screen, m, root, cursor);

      window->layout = PANGO_LAYOUT (g_object_ref (layout));

      /* calculate box dimensions */
      window->logobox         = window->area;
      window->logobox.x       = 0;
      window->logobox.height -= layout_height;
      window->textbox         = window->area;
      window->textbox.x       = 0;
      window->textbox.y      += window->logobox.height;
      window->textbox.height -= window->logobox.height;

      window_cr = gdk_cairo_create (window->window);

      balou_theme_draw_gradient (balou->theme,
                                 window_cr,
                                 window->logobox,
                                 window->textbox);

      cairo_destroy (window_cr);

      if (mainscreen == screen && mainmonitor == m)
        balou->mainwin = window;
    }

  g_object_unref (context);
  g_object_unref (layout);

  /* show splash windows */
  for (i = 0; i < balou->nwindows; ++i)
    {
      window = balou->windows + i;

      gtk_widget_show_now (window->wmwindow);

      gdk_window_add_filter (gtk_widget_get_window (window->wmwindow),
                             balou_window_filter,
                             window);

      gdk_window_show (window->window);
      gdk_window_add_filter (window->window,
                             balou_window_filter,
                             window);
    }
  gdk_flush ();

  /* draw the background and display logo pixbuf (if any) */
  window = balou->mainwin;

  cr = gdk_cairo_create (window->window);

  ww = gdk_window_get_width (GDK_WINDOW (window->window));
  wh = gdk_window_get_height (GDK_WINDOW (window->window));

  gdk_cairo_set_source_rgba (cr, &balou->bgcolor);
  cairo_rectangle (cr, 0, 0, ww, wh);
  cairo_fill (cr);

  pb = balou_theme_get_logo (balou->theme,
                             window->logobox.width,
                             window->logobox.height);
  if (G_LIKELY (pb != NULL))
    {
      pw = gdk_pixbuf_get_width (pb);
      ph = gdk_pixbuf_get_height (pb);
      px = (window->logobox.width - pw) / 2;
      py = (window->logobox.height - ph) / 2;

      gdk_cairo_set_source_pixbuf (cr, pb, px, py);
      cairo_paint (cr);

      g_object_unref (pb);
      cairo_destroy (cr);
    }

  pango_font_description_free (description);
  g_object_unref (cursor);
}


void
balou_fadein (Balou *balou, const gchar *text)
{
  BalouWindow *window = balou->mainwin;
  GdkRectangle area;
  cairo_t     *cr;
  GdkPixbuf   *pb;
  gint         median;
  gint         th;
  gint         tw;
  gint         x;
  gint         ww;
  gint         wh;

  pango_layout_set_text (window->layout, text, -1);
  pango_layout_get_pixel_size (window->layout, &tw, &th);

  area.x      = window->textbox.x + BALOU_INCREMENT;
  area.y      = window->textbox.y + (window->textbox.height - th) / 2;
  area.width  = tw + BALOU_INCREMENT;
  area.height = th;

  ww = gdk_window_get_width (GDK_WINDOW (window->window));
  wh = gdk_window_get_height (GDK_WINDOW (window->window));

  cr = gdk_cairo_create (window->window);

  gdk_cairo_set_source_rgba (cr, &balou->bgcolor);
  cairo_rectangle (cr, 0, 0, ww, wh);
  cairo_fill (cr);

  pb = balou_theme_get_logo (balou->theme,
                             window->logobox.width,
                             window->logobox.height);
  if (G_LIKELY (pb != NULL))
    {
      gint pw = gdk_pixbuf_get_width (pb);
      gint ph = gdk_pixbuf_get_height (pb);
      gint px = (window->logobox.width - pw) / 2;
      gint py = (window->logobox.height - ph) / 2;

      gdk_cairo_set_source_pixbuf (cr, pb, px, py);
      cairo_paint (cr);
  
      g_object_unref (pb);
    }

  median = (window->area.width - area.width) / 2;
  for (x = 0; (median - x) > BALOU_INCREMENT; x += BALOU_INCREMENT)
    {
      gdk_cairo_set_source_rgba (cr, &balou->bgcolor);
      gdk_cairo_rectangle (cr, &window->textbox);
      cairo_fill (cr);

      gdk_cairo_set_source_rgba (cr, &balou->fgcolor);
      cairo_move_to (cr, x, window->textbox.y);
      pango_cairo_show_layout (cr, window->layout);

      gdk_flush ();

      g_main_context_iteration (NULL, FALSE);
    }

  area.x += median;
  balou->fader_area = area;

  cairo_destroy (cr);
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

  gtk_widget_get_preferred_size (dialog, NULL, &requisition);
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
}


static void
balou_window_destroy (BalouWindow *window)
{
  gdk_window_remove_filter (window->window, balou_window_filter, window);
  if (gtk_widget_get_realized (window->wmwindow))
    {
      gdk_window_remove_filter (gtk_widget_get_window(window->wmwindow),
                                balou_window_filter,
                                window);
    }

  gdk_window_destroy (window->window);
  gtk_widget_destroy (window->wmwindow);
  g_object_unref (window->layout);
}



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
