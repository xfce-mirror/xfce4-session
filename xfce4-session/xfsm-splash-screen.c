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
 *
 * XXX - this code is brain damage, needs to be rewritten before 4.2!
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <gdk/gdkx.h>

#include <libxfce4util/libxfce4util.h>

#include <xfce4-session/xfsm-chooser.h>
#include <xfce4-session/xfsm-splash-screen.h>
#include <xfce4-session/xfsm-util.h>

#define INCREMENT 2

struct _XfsmSplashScreen
{
  GdkScreen *main_screen;
  int main_monitor;

  GdkWindow *window;
  GdkPixmap *backbuf;
  GdkPixmap *layout_pm;
  GdkGC *copy_gc;
  GdkGC *set_gc;
  PangoContext *context;
  GdkColormap *colormap;
  GList *other_windows;

  gboolean chooser_always;
  gint chooser_timeout;
  gboolean chooser_display; /* temp. variable */
  gint chooser_seconds_left; /* temp. variable */
  const gchar *chooser_session; /* temp. variable */

  int screen_w, screen_h;

  int text_h;
  int text_w;

  int current_x;

  guint idle_id;

  XfsmSplashTheme *theme;

  /* icons */
  GdkPixmap *skip_pm;
  GdkPixmap *chooser_pm;
  GdkRectangle skip_area;
  GdkRectangle chooser_area;
};


/* workaround to keep the splash windows above all other windows */
static void
xfsm_splash_screen_raise (XfsmSplashScreen *splash)
{
  GList *lp;

  for (lp = splash->other_windows; lp != NULL; lp = lp->next)
    gdk_window_raise (GDK_WINDOW (lp->data));

  gdk_window_raise (splash->window);
}


static gboolean
xfsm_splash_screen_idle_raise (gpointer user_data)
{
  XfsmSplashScreen *splash = (XfsmSplashScreen *) user_data;
  xfsm_splash_screen_raise (splash);
  return TRUE;
}


static void
xfsm_splash_screen_fadein (XfsmSplashScreen *splash, const gchar *text)
{
  PangoLayout *layout;
  char buffer[256];
  int tw, th;
  int top, mid, x;

  g_snprintf (buffer, 256, "<span face=\"Sans\" size=\"xx-large\">%s</span>",
              text);

  layout = pango_layout_new (splash->context);
  pango_layout_set_markup (layout, buffer, -1);
  pango_layout_get_pixel_size (layout, &tw, &th);

  splash->text_w = tw + INCREMENT;
  splash->layout_pm = gdk_pixmap_new (GDK_DRAWABLE (splash->window),
                                      splash->text_w, splash->text_h, -1);
  gdk_draw_rectangle (GDK_DRAWABLE (splash->layout_pm), splash->set_gc,
                      TRUE, 0, 0, splash->text_w, splash->text_h);
  gdk_draw_layout (GDK_DRAWABLE (splash->layout_pm), splash->copy_gc,
                   INCREMENT, (splash->text_h - th) / 2, layout);
  g_object_unref (G_OBJECT (layout));

  top = (splash->screen_h - splash->text_h);
  mid = (splash->screen_w - tw) / 2;

  for (x = 0; (mid - x) > INCREMENT; x += INCREMENT) {
    gdk_draw_drawable (GDK_DRAWABLE (splash->window), splash->copy_gc,
                       GDK_DRAWABLE (splash->layout_pm), 0, 0,
                       x, top, splash->text_w, splash->text_h);
    xfsm_splash_screen_raise (splash);

    gdk_flush ();

    g_main_context_iteration (NULL, FALSE);
  }

  splash->current_x = mid;

  /* sync backbuf */
  gdk_draw_rectangle (GDK_DRAWABLE (splash->backbuf), splash->set_gc,
                      TRUE, 0, top, splash->screen_w, splash->text_h);
  gdk_draw_drawable (GDK_DRAWABLE (splash->backbuf), splash->copy_gc,
                     GDK_DRAWABLE (splash->layout_pm), 0, 0,
                     mid, top, splash->text_w, splash->text_h);

  /* draw on-screen */
  gdk_draw_drawable (GDK_DRAWABLE (splash->window), splash->copy_gc,
                     GDK_DRAWABLE (splash->backbuf), x, top, x, top,
                     (mid - x) + splash->text_w, splash->text_h);

  xfsm_splash_screen_raise (splash);
  gdk_flush ();
}


static void
xfsm_splash_screen_fadeout (XfsmSplashScreen *splash)
{
  int top, x;

  if (splash->layout_pm == NULL)
    return;

  top = splash->screen_h - splash->text_h;

  /* sync backbuf */
  gdk_draw_rectangle (GDK_DRAWABLE (splash->backbuf), splash->set_gc,
                      TRUE, 0, top, splash->screen_w, splash->text_h);

  /* clear text area on-screen */
  gdk_draw_drawable (GDK_DRAWABLE (splash->window), splash->copy_gc,
                     GDK_DRAWABLE (splash->backbuf), 0, top, 0, top,
                     splash->screen_w, splash->text_h);

  for (x = splash->current_x; x < splash->screen_w; x += INCREMENT) {
    gdk_draw_drawable (GDK_DRAWABLE (splash->window), splash->copy_gc,
                       GDK_DRAWABLE (splash->layout_pm), 0, 0,
                       x, top, splash->text_w, splash->text_h);
    xfsm_splash_screen_raise (splash);

    gdk_flush ();

    g_main_context_iteration (NULL, FALSE);
  }

  g_object_unref (G_OBJECT (splash->layout_pm));
  splash->layout_pm = NULL;
}


static GdkWindow*
create_fullscreen_window (GdkScreen *screen,
                          int monitor,
                          const GdkColor *bgcolor)
{
  GdkRectangle geometry;
  GdkWindowAttr attr;
  GdkWindow *window;
  GdkCursor *cursor;
  GdkColor *color;

  /* acquire monitor geometry */
  gdk_screen_get_monitor_geometry (screen, monitor, &geometry);

  /* set watch cursor */
  cursor = gdk_cursor_new (GDK_WATCH);

  /* init window attributes */
  attr.x = geometry.x;
  attr.y = geometry.y;
  attr.event_mask = 0;
  attr.width = geometry.width;
  attr.height = geometry.height;
  attr.wclass = GDK_INPUT_OUTPUT;
  attr.window_type = GDK_WINDOW_TOPLEVEL;
  attr.cursor = cursor;
  attr.override_redirect = TRUE;

  window = gdk_window_new (gdk_screen_get_root_window (screen),
                           &attr, GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR |
                           GDK_WA_CURSOR);
  
  /* set background color */
  color = gdk_color_copy (bgcolor);
  gdk_rgb_find_color (gdk_drawable_get_colormap (window), color);
  gdk_window_set_background (window, color);
  gdk_color_free (color);

  gdk_window_show (window);

  /* the window now holds a reference on the cursor */
  gdk_cursor_unref (cursor);

  return window;
}


/* XXX - move away!!! */
static gboolean
xfsm_area_contains (const GdkRectangle *area, int x, int y)
{
  if ((x >= area->x) && (x < area->x + area->width)
      && (y >= area->y) && (y < area->y + area->height))
    return TRUE;
  return FALSE;
}


static GdkFilterReturn
splash_window_filter (GdkXEvent *xevent,
                      GdkEvent *event,
                      gpointer data)
{
  XfsmSplashScreen *splash = (XfsmSplashScreen *)data;
  XExposeEvent *expose = (XExposeEvent *)xevent;
  XMotionEvent *motion = (XMotionEvent *)xevent;
  XButtonEvent *button = (XButtonEvent *)xevent;
  GdkRectangle clip_rect;
  GdkCursor *cursor;

  switch (expose->type) {
  case Expose:
    clip_rect.x = expose->x;
    clip_rect.y = expose->y;
    clip_rect.width = expose->width;
    clip_rect.height = expose->height;

    gdk_gc_set_clip_rectangle (splash->copy_gc, &clip_rect);
    gdk_draw_drawable (GDK_DRAWABLE (splash->window),
                       splash->copy_gc, 
                       GDK_DRAWABLE (splash->backbuf),
                       0, 0, 0, 0, -1, -1);
    gdk_gc_set_clip_rectangle (splash->copy_gc, NULL);

    return GDK_FILTER_REMOVE;

  case MotionNotify:
    if (xfsm_area_contains (&splash->skip_area, motion->x, motion->y)
        || xfsm_area_contains (&splash->chooser_area, motion->x, motion->y))
      {
        cursor = gdk_cursor_new (GDK_HAND2);
        gdk_window_set_cursor (splash->window, cursor);
        gdk_cursor_unref (cursor);
      }
    else
      {
        cursor = gdk_cursor_new (GDK_WATCH);
        gdk_window_set_cursor (splash->window, cursor);
        gdk_cursor_unref (cursor);
      }
    break;

  case ButtonRelease:
    if (xfsm_area_contains (&splash->skip_area, button->x, button->y))
      {
        splash->chooser_display = FALSE;
        gtk_main_quit ();
      }
    else if (xfsm_area_contains (&splash->chooser_area, button->x, button->y))
      {
        splash->chooser_display = TRUE;
        gtk_main_quit ();
      }
    break;

  default:
    break;
  }

  return GDK_FILTER_CONTINUE;
}


XfsmSplashScreen*
xfsm_splash_screen_new (GdkDisplay *display,
                        gboolean display_chooser,
                        gint chooser_timeout,
                        const XfsmSplashTheme *theme)
{
  XfsmSplashScreen *splash;
  char buffer[128];
  GdkColor color;
  GdkPixbuf *skip_pb, *chooser_pb;
  GdkPixbuf *pb;
  int n;

  splash = g_new0 (XfsmSplashScreen, 1);
  splash->chooser_always = display_chooser;
  splash->chooser_timeout = chooser_timeout;
  splash->theme = xfsm_splash_theme_copy (theme);

  /* the other screens */
  for (n = 0; n < gdk_display_get_n_screens (display); ++n)
    {
      PangoLayout *layout;
      GdkWindow *window;
      GdkScreen *screen;
      GdkGC *gc;
      int w, h;
      int m;

      screen = gdk_display_get_screen (display, n);

      layout = pango_layout_new (gdk_pango_context_get_for_screen (screen));
      g_snprintf (buffer, 128, "<span face=\"Sans\" size=\"x-large\">%s</span>",
          _("Starting Xfce, please wait..."));
      pango_layout_set_markup (layout, buffer, -1);
      pango_layout_get_pixel_size (layout, &w, &h);

      for (m = 0; m < gdk_screen_get_n_monitors (screen); ++m)
        {
          GdkRectangle area;

          xfsm_splash_theme_get_bgcolor (theme, &color);
          window = create_fullscreen_window (screen, m, &color);

          gc = gdk_gc_new (GDK_DRAWABLE (window));
          xfsm_splash_theme_get_fgcolor (theme, &color);
          gdk_gc_set_rgb_fg_color (gc, &color);
          gdk_gc_set_function (gc, GDK_COPY);

          gdk_screen_get_monitor_geometry (screen, m, &area);

          /* draw the gradient (if any) */
          xfsm_splash_theme_draw_gradient (theme,
                                           GDK_DRAWABLE (window),
                                           area.x,
                                           area.y,
                                           area.width,
                                           area.height);

          if (n == 0 && m == 0) {
            /* first window is handled special */
            splash->main_screen = screen;
            splash->main_monitor = m;
            splash->window = window;
            splash->copy_gc = gc;
            splash->screen_w = area.width;
            splash->screen_h = area.height;
            splash->colormap = gdk_screen_get_rgb_colormap (screen);

            splash->backbuf = gdk_pixmap_new (GDK_DRAWABLE (splash->window),
                                              splash->screen_w,
                                              splash->screen_h,
                                              -1);
          }
          else {
            gdk_draw_layout (GDK_DRAWABLE (window), gc,
                             area.x + (area.width - w) / 2,
                             area.y + (area.height - h) / 2,
                             layout);

            splash->other_windows = g_list_append (splash->other_windows,
                                                   window);

            g_object_unref (G_OBJECT (gc));
          }
        }

      g_object_unref (G_OBJECT (layout));
    }

  /* duplicate gc and swap fg/bg colors */
  splash->set_gc = gdk_gc_new (GDK_DRAWABLE (splash->window));
  gdk_gc_copy (splash->set_gc, splash->copy_gc);
  xfsm_splash_theme_get_fgcolor (theme, &color);
  gdk_gc_set_rgb_bg_color (splash->set_gc, &color);
  xfsm_splash_theme_get_bgcolor (theme, &color);
  gdk_gc_set_rgb_fg_color (splash->set_gc, &color);

  /* clear back buffer */
  gdk_draw_rectangle (GDK_DRAWABLE (splash->backbuf), splash->set_gc, TRUE,
                      0, 0, splash->screen_w, splash->screen_h);

  splash->context = gdk_pango_context_get_for_screen (
      gdk_drawable_get_screen (GDK_DRAWABLE (splash->window)));
  /* test text height */ {
    int tw, th;
    PangoLayout *l = pango_layout_new (splash->context);

    g_snprintf (buffer, 128, "<span face=\"Sans\" size=\"xx-large\">%s</span>",
                "|()89jgGLA");
    pango_layout_set_markup (l, buffer, -1);
    pango_layout_get_pixel_size (l, &tw, &th);
    splash->text_h = th + 10;

    g_snprintf (buffer, 128, "<span face=\"Sans\" size=\"x-large\">%s</span>",
        _("Please wait, loading session data..."));
    pango_layout_set_markup (l, buffer, -1);
    pango_layout_get_pixel_size (l, &tw, &th);

    gdk_draw_layout (GDK_DRAWABLE (splash->window),
                     splash->copy_gc,
                     (splash->screen_w - tw) / 2,
                     (splash->screen_h - th) / 2,
                     l);
    gdk_flush ();
    g_object_unref (l);
  }

  /* draw the gradient (if any) */
  xfsm_splash_theme_draw_gradient (theme,
                                   GDK_DRAWABLE (splash->backbuf),
                                   0,
                                   0,
                                   splash->screen_w,
                                   splash->screen_h - splash->text_h);

  /* display the logo pixbuf (if any) */
  pb = xfsm_splash_theme_get_logo (theme,
                                   splash->screen_w,
                                   splash->screen_h - splash->text_h);
  if (pb != NULL)
    {
      int pw, ph;

      pw = gdk_pixbuf_get_width (pb);
      ph = gdk_pixbuf_get_height (pb);

      gdk_draw_pixbuf (GDK_DRAWABLE (splash->backbuf),
                       splash->copy_gc,
                       pb, 0, 0,
                       (splash->screen_w - pw) / 2,
                       ((splash->screen_h - splash->text_h) - ph) / 2,
                       pw, ph,
                       GDK_RGB_DITHER_NONE,
                       0, 0);

      gdk_draw_drawable (GDK_DRAWABLE (splash->window),
                         splash->copy_gc,
                         GDK_DRAWABLE (splash->backbuf),
                         0, 0,
                         0, 0,
                         splash->screen_w, splash->screen_h);

      g_object_unref (pb);
    }
  else
    {
      gdk_draw_rectangle (GDK_DRAWABLE (splash->window),
                          splash->set_gc, TRUE, 0, 0,
                          splash->screen_w, splash->screen_h);
    }

  skip_pb = xfsm_splash_theme_get_skip_icon (splash->theme,
                                             splash->screen_w / 3,
                                             splash->text_h);

  if (skip_pb != NULL)
    {
      GdkRectangle area;
      int realh;

      realh = gdk_pixbuf_get_height (skip_pb);
      area.width = gdk_pixbuf_get_width (skip_pb);
      area.height = splash->text_h;
      area.x = splash->screen_w - area.width;
      area.y = splash->screen_h - splash->text_h;

      splash->skip_pm = gdk_pixmap_new (GDK_DRAWABLE (splash->window),
                                        area.width, area.height, -1);

      gdk_draw_rectangle (GDK_DRAWABLE (splash->skip_pm), splash->set_gc,
                          TRUE, 0, 0, area.width, area.height);

      gdk_draw_pixbuf (GDK_DRAWABLE (splash->skip_pm), splash->copy_gc,
                       skip_pb, 0, 0,
                       0, (splash->text_h - realh) / 2,
                       area.width, realh,
                       GDK_RGB_DITHER_NONE, 0, 0);

      splash->skip_area = area;

      g_object_unref (G_OBJECT (skip_pb));
    }

  chooser_pb = xfsm_splash_theme_get_chooser_icon (splash->theme,
                                                   splash->screen_w / 3,
                                                   splash->text_h);

  if (chooser_pb != NULL)
    {
      GdkRectangle area;
      int realh;

      realh = gdk_pixbuf_get_height (chooser_pb);
      area.width = gdk_pixbuf_get_width (chooser_pb);
      area.height = splash->text_h;
      area.x = splash->screen_w - (area.width + splash->skip_area.width);
      area.y = splash->screen_h - splash->text_h;

      splash->chooser_pm = gdk_pixmap_new (GDK_DRAWABLE (splash->window),
                                           area.width, area.height, -1);

      gdk_draw_rectangle (GDK_DRAWABLE (splash->chooser_pm), splash->set_gc,
                          TRUE, 0, 0, area.width, splash->text_h);

      gdk_draw_pixbuf (GDK_DRAWABLE (splash->chooser_pm), splash->copy_gc,
                       chooser_pb, 0, 0,
                       0, (splash->text_h - realh) / 2,
                       area.width, realh,
                       GDK_RGB_DITHER_NONE, 0, 0);

      splash->chooser_area = area;

      g_object_unref (G_OBJECT (chooser_pb));
    }


  gdk_window_add_filter (splash->window, splash_window_filter, splash);
  gdk_window_set_events (splash->window, GDK_EXPOSURE_MASK);

  return splash;
}


void
xfsm_splash_screen_next (XfsmSplashScreen *splash, const gchar *text)
{
  if (splash->idle_id == 0)
    {
      splash->idle_id = g_idle_add (xfsm_splash_screen_idle_raise, splash);
    }

  xfsm_splash_screen_fadeout (splash);
  xfsm_splash_screen_fadein (splash, text);
}


static void
display_chooser_text (XfsmSplashScreen *splash,
                      const gchar *text)
{
  PangoContext *context;
  PangoLayout *layout;
  gchar markup[256];
  gint tw, th;

  if (text != NULL)
    {
      /* prevent from flicker */
      gdk_draw_rectangle (GDK_DRAWABLE (splash->backbuf), splash->set_gc,
                          TRUE, 0, splash->screen_h -splash->text_h,
                          splash->screen_w, splash->text_h);

      g_snprintf (markup,256,"<span face=\"Sans\" size=\"xx-large\">%s</span>",
                  text);
      context = gdk_pango_context_get_for_screen (splash->main_screen);
      layout = pango_layout_new (context);
      pango_layout_set_markup (layout, markup, -1);
      pango_layout_get_pixel_size (layout, &tw, &th);
      gdk_draw_layout (GDK_DRAWABLE (splash->backbuf), splash->copy_gc,
                       (splash->screen_w - tw) / 2,
                       splash->screen_h - splash->text_h
                        + (splash->text_h - th) / 2,
                       layout);

      if (splash->skip_pm != NULL)
        {
          gdk_draw_drawable (GDK_DRAWABLE (splash->backbuf), splash->copy_gc,
                             GDK_DRAWABLE (splash->skip_pm),
                             0, 0,
                             splash->skip_area.x,
                             splash->skip_area.y,
                             splash->skip_area.width,
                             splash->skip_area.height);
        }

      if (splash->chooser_pm != NULL)
        {
          gdk_draw_drawable (GDK_DRAWABLE (splash->backbuf), splash->copy_gc,
                             GDK_DRAWABLE (splash->chooser_pm),
                             0, 0,
                             splash->chooser_area.x,
                             splash->chooser_area.y,
                             splash->chooser_area.width,
                             splash->chooser_area.height);
        }

      gdk_draw_drawable (GDK_DRAWABLE (splash->window), splash->copy_gc,
                         GDK_DRAWABLE (splash->backbuf),
                         0, splash->screen_h - splash->text_h,
                         0, splash->screen_h - splash->text_h,
                         splash->screen_w, splash->text_h);
      g_object_unref (G_OBJECT (layout));
    }
  else
    {
      gdk_draw_rectangle (GDK_DRAWABLE (splash->backbuf), splash->set_gc,
                          TRUE, 0, splash->screen_h -splash->text_h,
                          splash->screen_w, splash->text_h);
      gdk_draw_drawable (GDK_DRAWABLE (splash->window), splash->copy_gc,
                         GDK_DRAWABLE (splash->backbuf),
                         0, splash->screen_h - splash->text_h,
                         0, splash->screen_h - splash->text_h,
                         splash->screen_w, splash->text_h);
    }
}


static void
chooser_timeout_display (XfsmSplashScreen *splash)
{
  char buffer[256];

  g_snprintf (buffer, 256, "Restoring <b>%s</b> in %d seconds",
              splash->chooser_session,
              splash->chooser_seconds_left);

  display_chooser_text (splash, buffer);
}


static gboolean
chooser_timeout (gpointer user_data)
{
  XfsmSplashScreen *splash = (XfsmSplashScreen *) user_data;

  if (--splash->chooser_seconds_left == 0)
    {
      splash->chooser_display = FALSE;
      gtk_main_quit ();
    }
  else
    {
      chooser_timeout_display (splash);
    }

  return TRUE;
}


gboolean
xfsm_splash_screen_choose (XfsmSplashScreen *splash,
                           GList *sessions,
                           const gchar *default_session,
                           gchar **name_return)
{
  XfsmChooserReturn result;
  XfsmChooser *chooser;
  GdkEventMask mask;
  GdkCursor *cursor;
  GList *lp;
  guint id;

  g_assert (default_session != NULL);

  splash->chooser_display = TRUE;

  if (!splash->chooser_always)
    {
      if (splash->chooser_timeout > 0)
        {
          splash->chooser_seconds_left = splash->chooser_timeout;
          splash->chooser_session = default_session;

          chooser_timeout_display (splash);
          
          mask = gdk_window_get_events (splash->window);
          gdk_window_set_events (splash->window, mask | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);

          id = g_timeout_add (1000, chooser_timeout, splash);
          gtk_main ();
          g_source_remove (id);

          gdk_window_set_events (splash->window, mask);

          display_chooser_text (splash, NULL);
        }
      else
        splash->chooser_display = FALSE;
    }

  if (!splash->chooser_display)
    {
      if (name_return != NULL)
        *name_return = g_strdup (default_session);

      for (lp = sessions; lp != NULL; lp = lp->next)
        {
          XfsmChooserSession *session = (XfsmChooserSession *) lp->data;
        
          if (strcmp (session->name, default_session) == 0)
            break;
        }

      return (lp != NULL);
    }

  display_chooser_text (splash, NULL);

  chooser = g_object_new (XFSM_TYPE_CHOOSER,
                          "type", GTK_WINDOW_POPUP,
                          NULL);
  xfsm_chooser_set_sessions (chooser, sessions, default_session);

  xfsm_window_add_border (GTK_WINDOW (chooser));
  gtk_window_set_screen (GTK_WINDOW (chooser), splash->main_screen);
  xfsm_center_window_on_screen (GTK_WINDOW (chooser),
                                splash->main_screen,
                                splash->main_monitor);
  gtk_widget_show_now (GTK_WIDGET (chooser));

  result = xfsm_chooser_run (chooser, name_return);

  gtk_widget_destroy (GTK_WIDGET (chooser));

  display_chooser_text (splash, NULL);

  g_main_context_iteration (NULL, FALSE);

  if (result == XFSM_CHOOSER_LOGOUT)
    exit (EXIT_SUCCESS);

  cursor = gdk_cursor_new (GDK_WATCH);
  gdk_window_set_cursor (splash->window, cursor);
  gdk_cursor_unref (cursor);

  return (result == XFSM_CHOOSER_LOAD);
}


void
xfsm_splash_screen_destroy (XfsmSplashScreen *splash)
{
  GList *lp;

  gdk_window_remove_filter (splash->window, splash_window_filter, splash);

  if (splash->layout_pm != NULL)
    xfsm_splash_screen_fadeout (splash);

  if (splash->idle_id != 0)
    {
      g_source_remove (splash->idle_id);
      splash->idle_id = 0;
    }

  for (lp = splash->other_windows; lp != NULL; lp = lp->next)
    gdk_window_destroy (GDK_WINDOW (lp->data));

  xfsm_splash_theme_destroy (splash->theme);

  if (splash->skip_pm != NULL)
    g_object_unref (G_OBJECT (splash->skip_pm));

  if (splash->chooser_pm != NULL)
    g_object_unref (G_OBJECT (splash->chooser_pm));

  gdk_window_destroy (GDK_WINDOW (splash->window));
  g_object_unref (G_OBJECT (splash->copy_gc));
  g_object_unref (G_OBJECT (splash->set_gc));
  g_list_free (splash->other_windows);
  g_free (splash);
}


