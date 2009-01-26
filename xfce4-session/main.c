/* $Id$ */
/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
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

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <xfconf/xfconf.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include <libxfsm/xfsm-util.h>

#include <xfce4-session/ice-layer.h>
#include <xfce4-session/shutdown.h>
#include <xfce4-session/sm-layer.h>
#include <xfce4-session/xfsm-dns.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-manager.h>
#include <xfce4-session/xfsm-startup.h>
#include "xfsm-error.h"

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

  /* check access to $ICEAUTHORITY or $HOME/.ICEauthority if unset */
  if (g_getenv ("ICEAUTHORITY"))
    authfile = g_strdup (g_getenv ("ICEAUTHORITY"));
  else
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
init_display (XfsmManager   *manager,
              GdkDisplay    *dpy,
              XfconfChannel *channel,
              gboolean       disable_tcp)
{
  gchar *engine;

  engine = xfconf_channel_get_string (channel, "/splash/Engine", "mice");

  splash_screen = xfsm_splash_screen_new (dpy, engine);  
  g_free (engine);
  xfsm_splash_screen_next (splash_screen, _("Loading desktop settings"));

  gdk_flush ();

  sm_init (channel, disable_tcp, manager);

  /* start xfsettingsd */
  if ( !g_spawn_command_line_async ("xfsettingsd", NULL))
  {
    g_warning ("Could not start xfsettingsd");
  }
  
  
  /* gtk resource files may have changed */
  gtk_rc_reparse_all ();
}


static void
initialize (XfsmManager *manager,
            int          argc,
            char       **argv)
{
  gboolean disable_tcp = FALSE;
  GdkDisplay *dpy;
  XfconfChannel *channel;
  
  for (++argv; --argc > 0; ++argv)
    {
      if (strcmp (*argv, "--version") == 0)
        {
          printf ("%s (Xfce %s)\n\n"
                  "Copyright (c) 2003-2006\n"
                  "        The Xfce development team. All rights reserved.\n\n"
                  "Written for Xfce by Benedikt Meurer <benny@xfce.org>.\n\n"
                  "Built with Gtk+-%d.%d.%d, running with Gtk+-%d.%d.%d.\n\n"
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

  channel = xfsm_open_config ();

  dpy = gdk_display_get_default ();
  init_display (manager, dpy, channel, disable_tcp);

  /* verify that the DNS settings are ok */
  xfsm_splash_screen_next (splash_screen, _("Verifying DNS settings"));
  xfsm_dns_check ();

  xfsm_splash_screen_next (splash_screen, _("Loading session data"));

  xfsm_startup_init (channel);
  xfsm_manager_load (manager, channel);
}


void
xfsm_dbus_init (void)
{
  DBusGConnection *dbus_conn;
  int              ret;
  GError          *error = NULL;

  xfsm_error_dbus_init ();

  dbus_conn = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
  if (G_UNLIKELY (!dbus_conn))
    {
      g_critical ("Unable to contact D-Bus session bus: %s", error ? error->message : "Unknown error");
      if (error)
        g_error_free (error);
      return;
    }

  ret = dbus_bus_request_name (dbus_g_connection_get_connection (dbus_conn),
                               "org.xfce.SessionManager",
                               DBUS_NAME_FLAG_DO_NOT_QUEUE,
                               NULL);
  if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret)
    {
      g_error ("Another session manager is already running");
      exit (1);
    }
}

void
xfsm_dbus_cleanup (void)
{
  DBusGConnection *dbus_conn;

  /* this is all not really necessary, but... */

  dbus_conn = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
  if (G_UNLIKELY (!dbus_conn))
    return;

  dbus_bus_release_name (dbus_g_connection_get_connection (dbus_conn),
                         "org.xfce.SessionManager", NULL);
}

int
main (int argc, char **argv)
{
  XfsmManager     *manager;
  XfsmShutdownType shutdown_type;
  GError          *error = NULL;

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
  
  /* install required signal handlers */
  signal (SIGPIPE, SIG_IGN);

  gtk_init (&argc, &argv);

  if (G_UNLIKELY (!xfconf_init (&error))) {
    xfce_message_dialog (NULL, _("Xfce Session Manager"),
                         GTK_STOCK_DIALOG_ERROR,
                         _("Unable to contact settings server"),
                         error->message,
                         GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
                         NULL);
    g_error_free (error);
  }

  /* fake a client id for the manager, so the legacy management does not
   * recognize us to be a session client.
   */
  gdk_set_sm_client_id (xfsm_generate_client_id (NULL));

  xfsm_dbus_init ();

  manager = xfsm_manager_new ();
  initialize (manager, argc, argv);
  xfsm_manager_restart (manager);
  
  gtk_main ();

  shutdown_type = xfsm_manager_get_shutdown_type (manager);
  g_object_unref (manager);
 
  xfsm_dbus_cleanup ();
  ice_cleanup ();

  return xfsm_shutdown (shutdown_type);
}
