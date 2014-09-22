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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
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
#include <libxfce4ui/libxfce4ui.h>

#include <libxfsm/xfsm-util.h>

#include <xfce4-session/ice-layer.h>
#include <xfce4-session/sm-layer.h>
#include <xfce4-session/xfsm-dns.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-manager.h>
#include <xfce4-session/xfsm-shutdown.h>
#include <xfce4-session/xfsm-startup.h>
#include <xfce4-session/xfsm-error.h>

static gboolean opt_disable_tcp = FALSE;
static gboolean opt_version = FALSE;

static GOptionEntry option_entries[] =
{
  { "disable-tcp", '\0', 0, G_OPTION_ARG_NONE, &opt_disable_tcp, N_("Disable binding to TCP ports"), NULL },
  { "version", 'V', 0, G_OPTION_ARG_NONE, &opt_version, N_("Print version information and exit"), NULL },
  { NULL }
};

static void
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
      g_printerr ("%s: Another session manager is already running\n", PACKAGE_NAME);
      exit (EXIT_FAILURE);
    }

  /* check if running in verbose mode */
  if (g_getenv ("XFSM_VERBOSE") != NULL)
    xfsm_enable_verbose ();

  /* pass correct DISPLAY to children, in case of --display in argv */
  g_setenv ("DISPLAY", gdk_display_get_name (gdk_display_get_default ()), TRUE);

  /* this is for compatibility with the GNOME Display Manager */
  lang = g_getenv ("GDM_LANG");
  if (lang != NULL && strlen (lang) > 0)
    {
      g_setenv ("LANG", lang, TRUE);
      g_unsetenv ("GDM_LANG");
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

  /* gtk resource files may have changed */
  gtk_rc_reparse_all ();
}


static void
xfsm_dbus_init (void)
{
  DBusGConnection *dbus_conn;
  int              ret;
  GError          *error = NULL;

  xfsm_error_dbus_init ();

  dbus_conn = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
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
      g_printerr ("%s: Another session manager is already running\n", PACKAGE_NAME);
      exit (EXIT_FAILURE);
    }
}

static void
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

static gboolean
xfsm_dbus_require_session (gint argc, gchar **argv)
{
  gchar **new_argv;
  gchar  *path;
  gint    i;
  guint   m = 0;

  if (g_getenv ("DBUS_SESSION_BUS_ADDRESS") != NULL)
    return TRUE;

  path = g_find_program_in_path ("dbus-launch");
  if (path == NULL)
    {
      g_critical ("dbus-launch not found, the desktop will not work properly!");
      return TRUE;
    }

  /* avoid rondtrips */
  g_assert (!g_str_has_prefix (*argv, "dbus-launch"));

  new_argv = g_new0 (gchar *, argc + 4);
  new_argv[m++] = path;
  new_argv[m++] = "--sh-syntax";
  new_argv[m++] = "--exit-with-session";

  for (i = 0; i < argc; i++)
    new_argv[m++] = argv[i];

  if (!execvp ("dbus-launch", new_argv))
    {
      g_critical ("Could not spawn %s: %s", path, g_strerror (errno));
    }

  g_free (path);
  g_free (new_argv);

  return FALSE;
}

int
main (int argc, char **argv)
{
  XfsmManager      *manager;
  GError           *error = NULL;
  GdkDisplay       *dpy;
  XfconfChannel    *channel;
  XfsmShutdownType  shutdown_type;
  XfsmShutdown     *shutdown_helper;
  gboolean          succeed = TRUE;

  if (!xfsm_dbus_require_session (argc, argv))
    return EXIT_SUCCESS;

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  /* install required signal handlers */
  signal (SIGPIPE, SIG_IGN);

  if (!gtk_init_with_args (&argc, &argv, "", option_entries, GETTEXT_PACKAGE, &error))
    {
      g_print ("%s: %s.\n", G_LOG_DOMAIN, error->message);
      g_print (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
      g_print ("\n");
      g_error_free (error);
      return EXIT_FAILURE;
    }

  if (opt_version)
    {
      g_print ("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, PACKAGE_VERSION, xfce_version_string ());
      g_print ("%s\n", "Copyright (c) 2003-2014");
      g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
      g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
      g_print ("\n");

      return EXIT_SUCCESS;
    }

  if (!xfconf_init (&error))
    {
      xfce_dialog_show_error (NULL, error, _("Unable to contact settings server"));
      g_error_free (error);
    }

  /* fake a client id for the manager, so the legacy management does not
   * recognize us to be a session client.
   */
  gdk_set_sm_client_id (xfsm_generate_client_id (NULL));

  xfsm_dbus_init ();

  manager = xfsm_manager_new ();
  setup_environment ();

  channel = xfsm_open_config ();

  dpy = gdk_display_get_default ();
  init_display (manager, dpy, channel, opt_disable_tcp);

  if (!opt_disable_tcp && xfconf_channel_get_bool (channel, "/security/EnableTcp", FALSE))
    {
      /* verify that the DNS settings are ok */
      xfsm_splash_screen_next (splash_screen, _("Verifying DNS settings"));
      xfsm_dns_check ();
    }

  xfsm_splash_screen_next (splash_screen, _("Loading session data"));

  xfsm_startup_init (channel);
  xfsm_manager_load (manager, channel);
  xfsm_manager_restart (manager);

  gtk_main ();

  xfsm_startup_shutdown ();

  shutdown_type = xfsm_manager_get_shutdown_type (manager);

  /* take over the ref before we release the manager */
  shutdown_helper = xfsm_shutdown_get ();

  g_object_unref (manager);
  g_object_unref (channel);

  xfsm_dbus_cleanup ();
  ice_cleanup ();

  if (shutdown_type == XFSM_SHUTDOWN_SHUTDOWN
      || shutdown_type == XFSM_SHUTDOWN_RESTART)
    {
      succeed = xfsm_shutdown_try_type (shutdown_helper, shutdown_type, &error);
      if (!succeed)
        g_warning ("Failed to shutdown/restart: %s", ERROR_MSG (error));
    }

  g_object_unref (shutdown_helper);

  return succeed ? EXIT_SUCCESS : EXIT_FAILURE;
}
