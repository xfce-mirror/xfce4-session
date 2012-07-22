/* $Id$ */
/*-
 * Copyright (c) 2004,2008 Brian Tarricone <kelnos@xfce.org>
 * Copyright (c) 2004 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>


gboolean opt_logout = FALSE;
gboolean opt_halt = FALSE;
gboolean opt_reboot = FALSE;
gboolean opt_suspend = FALSE;
gboolean opt_hibernate = FALSE;
gboolean opt_fast = FALSE;
gboolean opt_version = FALSE;

enum
{
  XFSM_SHUTDOWN_ASK = 0,
  XFSM_SHUTDOWN_LOGOUT,
  XFSM_SHUTDOWN_HALT,
  XFSM_SHUTDOWN_REBOOT,
  XFSM_SHUTDOWN_SUSPEND,
  XFSM_SHUTDOWN_HIBERNATE
};

static GOptionEntry option_entries[] =
{
  { "logout", 'l', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_logout,
    N_("Log out without displaying the logout dialog"),
    NULL
  },
  { "halt", 'h', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_halt,
    N_("Halt without displaying the logout dialog"),
    NULL
  },
  { "reboot", 'r', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_reboot,
    N_("Reboot without displaying the logout dialog"),
    NULL
  },
  { "suspend", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_suspend,
    N_("Suspend without displaying the logout dialog"),
    NULL
  },
  { "hibernate", 'h', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_hibernate,
    N_("Hibernate without displaying the logout dialog"),
    NULL
  },
  { "fast", 'f', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_fast,
    N_("Log out quickly; don't save the session"),
    NULL
  },
  { "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_version,
    N_("Print version information and exit"),
    NULL
  },
  { NULL }
};


static void
xfce_session_logout_notify_error (const gchar *message,
                                  GError      *error,
                                  gboolean     have_display)
{
  if (G_LIKELY (have_display))
    {
      xfce_dialog_show_error (NULL, error, "%s", message);
    }
  else
    {
      g_printerr (PACKAGE_NAME ": %s (%s).\n", message,
                  error != NULL ? error->message : _("Unknown error"));
    }
}



int
main (int argc, char **argv)
{
  DBusGConnection *conn;
  DBusGProxy      *proxy = NULL;
  GError          *err = NULL;
  gboolean         have_display;
  gboolean         show_dialog;
  gboolean         result = FALSE;
  guint            shutdown_type;
  gboolean         allow_save;

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  have_display = gtk_init_with_args (&argc, &argv, "", option_entries, PACKAGE, NULL);

  if (opt_version)
    {
      g_print ("%s %s (Xfce %s)\n\n", PACKAGE_NAME, PACKAGE_VERSION, xfce_version_string ());
      g_print ("%s\n", "Copyright (c) 2004-2012");
      g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
      g_print ("%s\n", _("Written by Benedikt Meurer <benny@xfce.org>"));
      g_print ("%s\n\n", _("and Brian Tarricone <kelnos@xfce.org>."));
      g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
      g_print ("\n");
      return EXIT_SUCCESS;
    }

  /* open session bus */
  conn = dbus_g_bus_get (DBUS_BUS_SESSION, &err);
  if (conn == NULL)
    {
      xfce_session_logout_notify_error (_("Unable to contact D-Bus session bus"), err, have_display);
      g_error_free (err);
      return EXIT_FAILURE;
    }

  /* save the session, unless fast is provided */
  allow_save = !opt_fast;

  /* create messsage */
  proxy = dbus_g_proxy_new_for_name_owner (conn,
                                           "org.xfce.SessionManager",
                                           "/org/xfce/SessionManager",
                                           "org.xfce.Session.Manager",
                                           &err);
  if (proxy != NULL)
    {
      if (opt_halt)
        {
          result = dbus_g_proxy_call (proxy, "Shutdown", &err,
                                      G_TYPE_BOOLEAN, allow_save,
                                      G_TYPE_INVALID, G_TYPE_INVALID);
        }
      else if (opt_reboot)
        {
          result = dbus_g_proxy_call (proxy, "Restart", &err,
                                      G_TYPE_BOOLEAN, allow_save,
                                      G_TYPE_INVALID, G_TYPE_INVALID);
        }
      else if (opt_suspend)
        {
          result = dbus_g_proxy_call (proxy, "Suspend", &err,
                                      G_TYPE_INVALID, G_TYPE_INVALID);
        }
      else if (opt_hibernate)
        {
          result = dbus_g_proxy_call (proxy, "Hibernate", &err,
                                      G_TYPE_INVALID, G_TYPE_INVALID);
        }
      else
        {
          show_dialog = !opt_logout;
          result = dbus_g_proxy_call (proxy, "Logout", &err,
                                      G_TYPE_BOOLEAN, show_dialog,
                                      G_TYPE_BOOLEAN, allow_save,
                                      G_TYPE_INVALID, G_TYPE_INVALID);
        }
    }

  /* fallback for the old 4.8 API dbus, so users can logout if
   * they upgraded their system, see bug #8630 */
  if (!result && g_error_matches (err, DBUS_GERROR, DBUS_GERROR_UNKNOWN_METHOD))
    {
      g_clear_error (&err);

      if (opt_logout)
        shutdown_type = XFSM_SHUTDOWN_LOGOUT;
      else if (opt_halt)
        shutdown_type = XFSM_SHUTDOWN_HALT;
      else if (opt_reboot)
        shutdown_type = XFSM_SHUTDOWN_REBOOT;
      else if (opt_suspend)
        shutdown_type = XFSM_SHUTDOWN_SUSPEND;
      else if (opt_hibernate)
        shutdown_type = XFSM_SHUTDOWN_HIBERNATE;
      else
        shutdown_type = XFSM_SHUTDOWN_ASK;

      result = dbus_g_proxy_call (proxy, "Shutdown", &err,
                                  G_TYPE_UINT, shutdown_type,
                                  G_TYPE_BOOLEAN, allow_save,
                                  G_TYPE_INVALID, G_TYPE_INVALID);
    }


  if (proxy != NULL)
    g_object_unref (proxy);

  if (!result)
    {
      xfce_session_logout_notify_error (_("Received error while trying to log out"), err, have_display);
      g_error_free (err);
      return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}


