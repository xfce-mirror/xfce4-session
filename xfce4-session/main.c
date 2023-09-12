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

#include <gio/gio.h>

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
static XfconfChannel *channel = NULL;
static guint name_id = 0;

static GOptionEntry option_entries[] =
{
  { "disable-tcp", '\0', 0, G_OPTION_ARG_NONE, &opt_disable_tcp, N_("Disable binding to TCP ports"), NULL },
  { "version", 'V', 0, G_OPTION_ARG_NONE, &opt_version, N_("Print version information and exit"), NULL },
  { NULL }
};


static void name_lost (GDBusConnection *connection,
                       const gchar *name,
                       gpointer user_data);


static void
setup_environment (void)
{
  const gchar *sm;
  gchar       *authfile;
  int          fd;

  /* check that no other session manager is running */
  sm = g_getenv ("SESSION_MANAGER");
  if (sm != NULL && strlen (sm) > 0)
    {
      g_warning ("Another session manager is already running");
      exit (EXIT_FAILURE);
    }

  /* check if running in verbose mode */
  if (g_getenv ("XFSM_VERBOSE") != NULL)
    xfsm_enable_verbose ();

  /* pass correct DISPLAY to children, in case of --display in argv */
  g_setenv ("DISPLAY", gdk_display_get_name (gdk_display_get_default ()), TRUE);

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
              gboolean       disable_tcp)
{
  gdk_display_flush (dpy);

  sm_init (channel, disable_tcp, manager);
}



static void
manager_quit_cb (XfsmManager *manager, gpointer user_data)
{
  GDBusConnection *connection = G_DBUS_CONNECTION (user_data);

  g_bus_unown_name (name_id);

  name_lost (connection, "org.xfce.SessionManager", &manager);
}



static void
bus_acquired (GDBusConnection *connection,
              const gchar *name,
              gpointer user_data)
{
  XfsmManager **manager = user_data;
  GdkDisplay *dpy;

  xfsm_verbose ("bus_acquired %s\n", name);

  *manager = xfsm_manager_new (connection);

  if (*manager == NULL) {
          g_critical ("Could not create XfsmManager");
          return;
  }

  g_signal_connect(G_OBJECT(*manager),
                   "manager-quit",
                   G_CALLBACK(manager_quit_cb),
                   connection);

  setup_environment ();

  channel = xfconf_channel_get (SETTINGS_CHANNEL);

  dpy = gdk_display_get_default ();
  init_display (*manager, dpy, opt_disable_tcp);

  if (!opt_disable_tcp && xfconf_channel_get_bool (channel, "/security/EnableTcp", FALSE))
    {
      /* verify that the DNS settings are ok */      
      xfsm_dns_check ();
    }


  xfsm_startup_init (channel);
  xfsm_manager_load (*manager, channel);
  xfsm_manager_restart (*manager);
}



static void
name_acquired (GDBusConnection *connection,
               const gchar *name,
               gpointer user_data)
{
  xfsm_verbose ("name_acquired\n");
}



static void
name_lost (GDBusConnection *connection,
           const gchar *name,
           gpointer user_data)
{
  XfsmManager      **manager = user_data;
  GError           *error = NULL;
  XfsmShutdownType  shutdown_type;
  XfsmShutdown     *shutdown_helper;
  gboolean          succeed = TRUE;

  xfsm_verbose ("name_lost\n");

  if (*manager == NULL)
    {
      if (connection == NULL)
        xfsm_verbose ("Failed to connect to D-Bus");
      else
        xfsm_verbose ("Failed to acquire name %s", name);

      gtk_main_quit ();
      return;
    }

  /* Release the  object */
  xfsm_verbose ("Disconnected from D-Bus");

  shutdown_type = xfsm_manager_get_shutdown_type (*manager);

  /* take over the ref before we release the manager */
  shutdown_helper = xfsm_shutdown_get ();

  ice_cleanup ();

  if (shutdown_type == XFSM_SHUTDOWN_SHUTDOWN
      || shutdown_type == XFSM_SHUTDOWN_RESTART)
    {
      succeed = xfsm_shutdown_try_type (shutdown_helper, shutdown_type, &error);
      if (!succeed)
        g_warning ("Failed to shutdown/restart: %s", ERROR_MSG (error));
    }

  g_object_unref (shutdown_helper);
  g_object_unref (*manager);
  g_clear_error (&error);

  shutdown_helper = NULL;
  *manager = NULL;

  gtk_main_quit ();
}



static void
xfsm_dbus_init (XfsmManager **manager)
{
  name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                            "org.xfce.SessionManager",
                            G_BUS_NAME_OWNER_FLAGS_NONE,
                            bus_acquired, name_acquired, name_lost,
                            manager,
                            NULL);

  if (name_id == 0)
    {
      g_warning ("Another session manager is already running");
      exit (EXIT_FAILURE);
    }
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
  XfsmManager      *manager = NULL;
  GError           *error = NULL;

  if (!xfsm_dbus_require_session (argc, argv))
    return EXIT_SUCCESS;

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  /* install required signal handlers */
  signal (SIGPIPE, SIG_IGN);
  signal (SIGCHLD, SIG_IGN);

  if (!gtk_init_with_args (&argc, &argv, NULL, option_entries, GETTEXT_PACKAGE, &error))
    {
      g_printerr ("%s: %s.\n", G_LOG_DOMAIN, error->message);
      g_printerr (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
      g_printerr ("\n");
      g_error_free (error);
      return EXIT_FAILURE;
    }

  if (opt_version)
    {
      g_print ("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, PACKAGE_VERSION, xfce_version_string ());
      g_print ("%s\n", "Copyright (c) 2003-2023");
      g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
      g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
      g_print ("\n");

      return EXIT_SUCCESS;
    }

  if (!xfconf_init (&error))
    {
      xfce_dialog_show_error (NULL, error, _("Unable to contact settings server"));
      g_error_free (error);
      return EXIT_FAILURE;
    }

  /* Process all pending events prior to start DBUS */
  while (gtk_events_pending ())
    gtk_main_iteration ();

  /* fake a client id for the manager, so the legacy management does not
   * recognize us to be a session client.
   */
  gdk_x11_set_sm_client_id (xfsm_generate_client_id (NULL));

  xfsm_dbus_init (&manager);

  gtk_main ();

  xfsm_startup_shutdown ();
  xfconf_shutdown ();

  return EXIT_SUCCESS;
}
