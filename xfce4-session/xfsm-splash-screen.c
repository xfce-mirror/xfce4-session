/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
  gboolean chooser_display; /* temp. variable */
  gint chooser_seconds_left; /* temp. variable */
  const gchar *chooser_session; /* temp. variable */

  int screen_w, screen_h;

  int text_h;
  int text_w;

  int current_x;
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
create_fullscreen_window (GdkScreen *screen, int monitor)
{
  GdkRectangle geometry;
  GdkWindowAttr attr;
  GdkWindow *window;
  GdkColor color;

  /* acquire monitor geometry */
  gdk_screen_get_monitor_geometry (screen, monitor, &geometry);

  /* init window attributes */
  attr.x = geometry.x;
  attr.y = geometry.y;
  attr.width = geometry.width;
  attr.height = geometry.height;
  attr.wclass = GDK_INPUT_OUTPUT;
  attr.window_type = GDK_WINDOW_CHILD;
  attr.cursor = gdk_cursor_new (GDK_WATCH);
  attr.override_redirect = TRUE;

  window = gdk_window_new (gdk_screen_get_root_window (screen),
                           &attr, GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR |
                           GDK_WA_CURSOR);
  
  gdk_color_parse ("White", &color);
  gdk_rgb_find_color (gdk_screen_get_rgb_colormap (screen), &color);
  gdk_window_set_background (window, &color);
  gdk_window_show (window);

  /* the window now holds a reference on the cursor */
  gdk_cursor_unref (attr.cursor);

  return window;
}


static GdkFilterReturn
splash_window_filter (GdkXEvent *xevent,
                      GdkEvent *event,
                      gpointer data)
{
  XfsmSplashScreen *splash = (XfsmSplashScreen *)data;
  XExposeEvent *xev = (XExposeEvent *)xevent;
  GdkRectangle clip_rect;

  switch (xev->type) {
  case Expose:
    clip_rect.x = xev->x;
    clip_rect.y = xev->y;
    clip_rect.width = xev->width;
    clip_rect.height = xev->height;

    gdk_gc_set_clip_rectangle (splash->copy_gc, &clip_rect);
    gdk_draw_drawable (GDK_DRAWABLE (splash->window),
                       splash->copy_gc, 
                       GDK_DRAWABLE (splash->backbuf),
                       0, 0, 0, 0, -1, -1);
    gdk_gc_set_clip_rectangle (splash->copy_gc, NULL);

    return GDK_FILTER_REMOVE;

  case KeyPress:
    gtk_main_quit ();
    break;

  default:
    break;
  }

  return GDK_FILTER_CONTINUE;
}


XfsmSplashScreen*
xfsm_splash_screen_new (GdkDisplay *display, gboolean display_chooser)
{
  XfsmSplashScreen *splash;
  char buffer[128];
  GdkColor color;
  GdkPixbuf *pb;
  int n;

  splash = g_new0 (XfsmSplashScreen, 1);
  splash->chooser_always = display_chooser;

  /* the other screens */
  for (n = 0; n < gdk_display_get_n_screens (display); ++n) {
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

    for (m = 0; m < gdk_screen_get_n_monitors (screen); ++m) {
      window = create_fullscreen_window (screen, m);

      gc = gdk_gc_new (GDK_DRAWABLE (window));
      gdk_color_parse ("Black", &color);
      gdk_rgb_find_color (gdk_screen_get_rgb_colormap (screen), &color);
      gdk_gc_set_foreground (gc, &color);
      gdk_gc_set_function (gc, GDK_COPY);

      if (n == 0 && m == 0) {
        /* first window is handled special */
        splash->main_screen = screen;
        splash->main_monitor = m;
        splash->window = window;
        splash->copy_gc = gc;
        gdk_drawable_get_size (GDK_DRAWABLE (splash->window),
                               &splash->screen_w,
                               &splash->screen_h);
        splash->colormap = gdk_screen_get_rgb_colormap (screen);

        splash->backbuf = gdk_pixmap_new (GDK_DRAWABLE (splash->window),
                                          splash->screen_w,
                                          splash->screen_h,
                                          -1);
      }
      else {
        gdk_draw_layout (GDK_DRAWABLE (window), gc,
                         (gdk_screen_get_width (screen) - w) / 2,
                         (gdk_screen_get_height (screen) - h) / 2,
                         layout);

        splash->other_windows = g_list_append (splash->other_windows, window);

        g_object_unref (G_OBJECT (gc));
      }
    }

    g_object_unref (G_OBJECT (layout));
  }

  splash->set_gc = gdk_gc_new (GDK_DRAWABLE (splash->window));
  gdk_gc_copy (splash->set_gc, splash->copy_gc);
  gdk_gc_set_function (splash->set_gc, GDK_SET);

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

  pb = gdk_pixbuf_new_from_file (SPLASH_THEME_LOGO, NULL);
  if (pb != NULL) {
    int aw, ah;
    int pw, ph;

    /* determine available geometry */
    aw = splash->screen_w;
    ah = splash->screen_h - splash->text_h;

    pw = gdk_pixbuf_get_width (pb);
    ph = gdk_pixbuf_get_height (pb);

    /* check if we need to scale down */
    if (pw > aw || ph > ah) {
      double wratio, hratio;
      GdkPixbuf *opb = pb;

      wratio = (double)pw / (double)aw;
      hratio = (double)ph / (double)ah;

      if (hratio > wratio) {
        pw = rint (pw / hratio);
        ph = ah;
      }
      else {
        pw = aw;
        ph = rint (ph / wratio);
      }
      
      pb = gdk_pixbuf_scale_simple (opb, pw, ph, GDK_INTERP_BILINEAR);
      g_object_unref (opb);
    }

    gdk_draw_pixbuf (GDK_DRAWABLE (splash->backbuf),
                     splash->copy_gc,
                     pb,
                     0, 0,
                     (aw - pw) / 2, (ah - ph) / 2,
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
  else {
    gdk_draw_rectangle (GDK_DRAWABLE (splash->window),
                        splash->set_gc, TRUE, 0, 0,
                        splash->screen_w, splash->screen_h);
  }

  gdk_window_add_filter (splash->window, splash_window_filter, splash);
  gdk_window_set_events (splash->window, GDK_EXPOSURE_MASK);

  return splash;
}


void
xfsm_splash_screen_next (XfsmSplashScreen *splash, const gchar *text)
{
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
      g_snprintf (markup,256,"<span face=\"Sans\" size=\"xx-large\">%s</span>",
                  text);
      context = gdk_pango_context_get_for_screen (splash->main_screen);
      layout = pango_layout_new (context);
      pango_layout_set_markup (layout, markup, -1);
      pango_layout_get_pixel_size (layout, &tw, &th);
      gdk_draw_layout (GDK_DRAWABLE (splash->backbuf), splash->copy_gc,
                       (splash->screen_w - tw) / 2, 
                       splash->screen_h - splash->text_h,
                       layout);
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

  display_chooser_text (splash, NULL);

  g_snprintf (buffer, 256, "Restoring <b>%s</b> in %d seconds, press any "
              "key to interrupt.", splash->chooser_session,
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
  GList *lp;
  guint id;

  g_assert (default_session != NULL);

  splash->chooser_display = TRUE;

  if (!splash->chooser_always)
    {
      splash->chooser_seconds_left = 4;
      splash->chooser_session = default_session;

      chooser_timeout_display (splash);
      
      mask = gdk_window_get_events (splash->window);
      gdk_window_set_events (splash->window, mask | GDK_KEY_PRESS_MASK);

      id = g_timeout_add (1000, chooser_timeout, splash);
      gtk_main ();
      g_source_remove (id);

      gdk_window_set_events (splash->window, mask);

      display_chooser_text (splash, NULL);
    }

  if (!splash->chooser_display)
    {
      if (name_return != NULL)
        *name_return = g_strdup (default_session);

      for (lp = sessions; lp != NULL; lp = lp->next)
        if (strcmp ((const gchar *) lp->data, default_session) == 0)
          break;

      return (lp != NULL);
    }

  display_chooser_text (splash, _("Pick a session or create a new one"));

  chooser = g_object_new (XFSM_TYPE_CHOOSER,
                          "type", GTK_WINDOW_POPUP,
                          NULL);
  xfsm_chooser_set_sessions (chooser, sessions, default_session);

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

  return (result == XFSM_CHOOSER_LOAD);
}


void
xfsm_splash_screen_destroy (XfsmSplashScreen *splash)
{
  GList *lp;

  gdk_window_remove_filter (splash->window, splash_window_filter, splash);

  if (splash->layout_pm != NULL)
    xfsm_splash_screen_fadeout (splash);

  for (lp = splash->other_windows; lp != NULL; lp = lp->next)
    gdk_window_destroy (GDK_WINDOW (lp->data));

  gdk_window_destroy (GDK_WINDOW (splash->window));
  g_object_unref (G_OBJECT (splash->copy_gc));
  g_object_unref (G_OBJECT (splash->set_gc));
  g_list_free (splash->other_windows);
  g_free (splash);
}


