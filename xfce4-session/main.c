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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libxfce4mcs/mcs-client.h>
#include <libxfce4util/libxfce4util.h>

#include <xfce4-session/ice-layer.h>
#include <xfce4-session/shutdown.h>
#include <xfce4-session/sm-layer.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-manager.h>
#include <xfce4-session/xfsm-startup.h>


void
setup_environment (void)
{
  const gchar *lang;

  /* check that no other session manager is running */  
  if (g_getenv ("SESSION_MANAGER") != NULL)
    {
      fprintf (stderr, "xfce4-session: Another session manager is already running\n");
      exit (EXIT_FAILURE);
    }

  /* check if running in verbose mode */
  if (g_getenv ("XFSM_VERBOSE") != NULL)
    xfsm_enable_verbose ();

  /* pass correct DISPLAY to children, in case of --display in argv */
  xfce_setenv ("DISPLAY", gdk_display_get_name (gdk_display_get_default ()), TRUE);

  /* this is for compatibility with the GNOME Display Manager */
  lang = g_getenv ("GDM_LANG");
  if (lang != NULL)
    {
      xfce_setenv ("LANG", lang, TRUE);
      xfce_unsetenv ("GDM_LANG");
    }
}


static void
usage (int exit_code)
{
  fprintf (stderr,
           "Usage: xfce4-session [OPTION...]\n"
           "\n"
           "GTK+\n"
           "  --display=DISPLAY        X display to use\n"
           "  --screen=SCREEN          X screen to use\n"
           "\n"
           "Application options\n"
           "  --disable-tcp            Disable binding to TCP ports\n"
           "  --help                   Print this help message and exit\n"
           "  --version                Print version information and exit\n"
           "\n");
  exit (exit_code);
}


static void
init_display (GdkDisplay *dpy)
{
  PangoContext *context;
  PangoLayout *layout;
  GdkRectangle area;
  GdkColormap *cmap;
  GdkCursor *cursor;
  GdkScreen *screen;
  GdkWindow *root;
  GdkColor black;
  GdkColor white;
  char text[256];
  int tw, th;
  GdkGC *gc;
  int n;

  gdk_color_parse ("Black", &black);
  gdk_color_parse ("White", &white);

  g_snprintf (text, 256, "<span face=\"Sans\" size=\"x-large\">%s</span>",
              _("Restoring the desktop settings, please wait..."));

  cursor = gdk_cursor_new_for_display (dpy, GDK_LEFT_PTR);

  for (n = 0; n < gdk_display_get_n_screens (dpy); ++n)
    {
      screen = gdk_display_get_screen (dpy, n);
      gdk_screen_get_monitor_geometry (screen, 0, &area);
      root = gdk_screen_get_root_window (screen);
      cmap = gdk_drawable_get_colormap (GDK_DRAWABLE (root));
      gdk_rgb_find_color (cmap, &white);
      gdk_window_set_background (root, &white);
      gdk_window_set_cursor (root, cursor);
      gdk_window_clear (root);

      gc = gdk_gc_new (GDK_DRAWABLE (root));
      gdk_gc_set_function (gc, GDK_COPY);
      gdk_gc_set_rgb_fg_color (gc, &black);

      gdk_flush ();

      context = gdk_pango_context_get_for_screen (screen);
      layout = pango_layout_new (context);
      pango_layout_set_markup (layout, text, -1);
      pango_layout_get_pixel_size (layout, &tw, &th);
      gdk_draw_layout (GDK_DRAWABLE (root), gc,
                       area.x + (area.width - tw) / 2,
                       area.y + (area.height - th) / 2,
                       layout);

      g_object_unref (G_OBJECT (layout));
      g_object_unref (G_OBJECT (gc));
    }

  gdk_cursor_unref (cursor);

  gdk_flush ();

  g_usleep (1000 * 1000);

  /* start a MCS manager process per screen */
  for (n = 0; n < gdk_display_get_n_screens (dpy); ++n)
    {
      mcs_client_check_manager (gdk_x11_display_get_xdisplay (dpy), n,
                                "xfce-mcs-manager");
    }
}


static void
init_splash (GdkDisplay *dpy, XfceRc *rc)
{
  gboolean chooser;

  /* boot the splash screen */
  chooser = xfce_rc_read_bool_entry (rc, "AlwaysDisplayChooser", FALSE);
  splash_screen = xfsm_splash_screen_new (dpy, chooser);
}


static void
initialize (int argc, char **argv)
{
  gboolean disable_tcp = FALSE;
  GdkDisplay *dpy;
  XfceRc *rc;
  
  for (++argv; --argc > 0; ++argv)
    {
      if (strcmp (*argv, "--version") == 0)
        {
          printf ("Xfce %s\n\n"
                  "Copyright (c) 2003-2004\n"
                  "        The Xfce development team. All rights reserved.\n\n"
                  "Written for Xfce by Benedikt Meurer <benny@xfce.org>.\n\n"
                  "Please report bugs to <%s>.\n",
                  PACKAGE_STRING, PACKAGE_BUGREPORT);
          exit (EXIT_SUCCESS);
        }
      else if (strcmp (*argv, "--disable-tcp") == 0)
        {
          disable_tcp = TRUE;
        }
      else
        {
          usage (strcmp (*argv, "--help") == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
        }
    }

  setup_environment ();

  dpy = gdk_display_get_default ();
  init_display (dpy);

  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG,
                            "xfce4-session/xfce4-session.rc",
                            TRUE);
  xfce_rc_set_group (rc, "General");

  init_splash (dpy, rc);
  sm_init (rc, disable_tcp);
  xfsm_startup_init (rc);
  xfsm_manager_init (rc);

  xfce_rc_close (rc);
}


int
main (int argc, char **argv)
{
  /* imported from xfsm-manager.c */
  extern gint shutdown_type;

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
  
  gtk_init (&argc, &argv);
  
  /* fake a client id for the manager, so smproxy does not recognize
   * us to be a session client.
   */
  gdk_set_sm_client_id (xfsm_manager_generate_client_id (NULL));

  initialize (argc, argv);
  
  xfsm_manager_restart ();
  
  gtk_main ();
  
  ice_cleanup ();

  return shutdown (shutdown_type);
}
