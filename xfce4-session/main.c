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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libxfce4mcs/mcs-client.h>
#include <libxfce4util/libxfce4util.h>

#include <libxfsm/xfsm-util.h>

#include <xfce4-session/ice-layer.h>
#include <xfce4-session/shutdown.h>
#include <xfce4-session/sm-layer.h>
#include <xfce4-session/xfsm-dns.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-manager.h>
#include <xfce4-session/xfsm-startup.h>


void
setup_environment (void)
{
  const gchar *lang;
  const gchar *sm;
  gchar       *authfile;
  int          fd;

  /* check that no other session manager is running */  
  sm = g_getenv ("SESSION_MANAGER");
  if (sm != NULL && strlen (sm) > 0)
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

  /* check access to $HOME/.ICEauthority */
  authfile = xfce_get_homefile (".ICEauthority", NULL);
  fd = open (authfile, O_RDWR | O_CREAT, 0600);
  if (fd < 0)
    {
      fprintf (stderr, "xfce4-session: Unable to access file %s: %s\n",
               authfile, g_strerror (errno));
      exit (EXIT_FAILURE);
    }
  g_free (authfile);
  close (fd);
}


static void
usage (int exit_code)
{
  fprintf (stderr,
           "Usage: xfce4-session [OPTION...]\n"
           "\n"
           "GTK+\n"
           "  --display=DISPLAY        X display to use\n"
           "\n"
           "Application options\n"
           "  --disable-tcp            Disable binding to TCP ports\n"
           "  --help                   Print this help message and exit\n"
           "  --version                Print version information and exit\n"
           "\n");
  exit (exit_code);
}


static void
init_display (GdkDisplay *dpy,
              XfceRc     *rc)
{
  const gchar *engine;
  gint n;

  xfce_rc_set_group (rc, "Splash Screen");
  engine = xfce_rc_read_entry (rc, "Engine", NULL);

  splash_screen = xfsm_splash_screen_new (dpy, engine);
  xfsm_splash_screen_next (splash_screen, _("Loading desktop settings"));

  gdk_flush ();

  /* start a MCS manager process per screen (FIXME: parallel to loading logo) */
  for (n = 0; n < gdk_display_get_n_screens (dpy); ++n)
    {
      mcs_client_check_manager (gdk_x11_display_get_xdisplay (dpy), n,
                                "xfce-mcs-manager");
    }

  /* gtk resource files may have changed */
  gtk_rc_reparse_all ();
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
          printf ("%s (Xfce %s)\n\n"
                  "Copyright (c) 2003-2004\n"
                  "        The Xfce development team. All rights reserved.\n\n"
                  "Written for Xfce by Benedikt Meurer <benny@xfce.org>.\n\n"
                  "Built with Gtk+-%d.%d.%d, running with Gtk+-%d.%d%d.\n\n"
                  "Please report bugs to <%s>.\n",
                  PACKAGE_STRING, xfce_version_string (),
                  GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION,
                  gtk_major_version, gtk_minor_version, gtk_micro_version,
                  PACKAGE_BUGREPORT);
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

  rc = xfsm_open_config (TRUE);

  dpy = gdk_display_get_default ();
  init_display (dpy, rc);

  /* verify that the DNS settings are ok */
  xfsm_splash_screen_next (splash_screen, _("Verifying DNS settings"));
  xfsm_dns_check ();

  xfsm_splash_screen_next (splash_screen, _("Loading session data"));

  xfce_rc_set_group (rc, "Compatibility");
  compat_gnome = xfce_rc_read_bool_entry (rc, "LaunchGnome", FALSE);
  compat_kde = xfce_rc_read_bool_entry (rc, "LaunchKDE", FALSE);

  xfce_rc_set_group (rc, "General");
  sm_init (rc, disable_tcp);
  xfsm_startup_init (rc);
  xfsm_manager_init (rc);

  /* cleanup obsolete entries */
  xfce_rc_set_group (rc, "General");
  if (xfce_rc_has_entry (rc, "ConfirmLogout"))
    xfce_rc_delete_entry (rc, "ConfirmLogout", FALSE);
  if (xfce_rc_has_entry (rc, "AlwaysDisplayChooser"))
    xfce_rc_delete_entry (rc, "AlwaysDisplayChooser", FALSE);
  xfce_rc_delete_group (rc, "Splash Theme", FALSE);

  xfce_rc_close (rc);
}


int
main (int argc, char **argv)
{
  /* imported from xfsm-manager.c */
  extern gint shutdown_type;

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
  
  /* stupid, damn f*ck*ng stupid linux! */
  signal (SIGPIPE, SIG_IGN);

  gtk_init (&argc, &argv);

  /* fake a client id for the manager, so the legacy management does not
   * recognize us to be a session client.
   */
  gdk_set_sm_client_id (xfsm_manager_generate_client_id (NULL));

  initialize (argc, argv);
  
  xfsm_manager_restart ();
  
  gtk_main ();
  
  ice_cleanup ();

  return xfsm_shutdown (shutdown_type);
}
