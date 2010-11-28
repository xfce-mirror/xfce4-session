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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
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

#include <dbus/dbus.h>

#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>


/* copied from xfce4-session/shutdown.h -- ORDER MATTERS.  The numbers
 * correspond to the 'type' parameter of org.xfce.Session.Manager.Shutdown
 */
typedef enum
{
  XFSM_SHUTDOWN_ASK = 0,
  XFSM_SHUTDOWN_LOGOUT,
  XFSM_SHUTDOWN_HALT,
  XFSM_SHUTDOWN_REBOOT,
  XFSM_SHUTDOWN_SUSPEND,
  XFSM_SHUTDOWN_HIBERNATE,
} XfsmShutdownType;

gboolean opt_logout = FALSE;
gboolean opt_halt = FALSE;
gboolean opt_reboot = FALSE;
gboolean opt_suspend = FALSE;
gboolean opt_hibernate = FALSE;
gboolean opt_fast = FALSE;
gboolean opt_version = FALSE;

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
xfce_session_logout_notify_error (const gchar *primary_message,
                                  DBusError *derror,
                                  gboolean have_display)
{
  if (G_LIKELY (have_display))
    {
      xfce_message_dialog (NULL, _("Logout Error"), GTK_STOCK_DIALOG_ERROR,
                           primary_message,
                           derror && dbus_error_is_set (derror)
                           ? derror->message : _("Unknown error"),
                           GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
    }
  else
    {
      g_critical ("%s: %s", primary_message,
                  derror && dbus_error_is_set (derror) ? derror->message
                                                       : _("Unknown error"));
    }
}



int
main (int argc, char **argv)
{
  XfsmShutdownType shutdown_type = XFSM_SHUTDOWN_ASK;
  gboolean allow_save = TRUE;
  DBusConnection *dbus_conn;
  DBusMessage *message, *reply;
  DBusError derror;
  gboolean have_display;

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  have_display = gtk_init_with_args (&argc, &argv, "", option_entries, PACKAGE, NULL);

  if (opt_logout)
    {
      shutdown_type = XFSM_SHUTDOWN_LOGOUT;
    }
  else if (opt_halt)
    {
      shutdown_type = XFSM_SHUTDOWN_HALT;
    }
  else if (opt_reboot)
    {
      shutdown_type = XFSM_SHUTDOWN_REBOOT;
    }
  else if (opt_suspend)
    {
      shutdown_type = XFSM_SHUTDOWN_SUSPEND;
    }
  else if (opt_hibernate)
    {
      shutdown_type = XFSM_SHUTDOWN_HIBERNATE;
    }
  else if (opt_fast)
    {
      allow_save = FALSE;
    }
  else if (opt_version)
    {
      printf ("%s (Xfce %s)\n\n"
              "Copyright (c) 2010\n"
              "        The Xfce development team. All rights reserved.\n\n"
              "Written for Xfce by Brian Tarricone <kelnos@xfce.org> and\n"
              "Benedikt Meurer <benny@xfce.org>.\n\n"
              "Please report bugs to <%s>.\n",
              PACKAGE_STRING, xfce_version_string(), PACKAGE_BUGREPORT);
      return EXIT_SUCCESS;
    }

  dbus_error_init (&derror);
  dbus_conn = dbus_bus_get (DBUS_BUS_SESSION, &derror);
  if (G_UNLIKELY (dbus_conn == NULL))
    {
      xfce_session_logout_notify_error (_("Unable to contact D-Bus session bus."),
                                        &derror, have_display);
      dbus_error_free (&derror);
      return EXIT_FAILURE;
    }

  message = dbus_message_new_method_call ("org.xfce.SessionManager",
                                          "/org/xfce/SessionManager",
                                          "org.xfce.Session.Manager",
                                          "Shutdown");
  if (G_UNLIKELY (message == NULL))
    {
      xfce_session_logout_notify_error (_("Failed to create new D-Bus message"),
                                        NULL, have_display);
      return EXIT_FAILURE;
    }

  dbus_message_append_args (message,
                            DBUS_TYPE_UINT32, &shutdown_type,
                            DBUS_TYPE_BOOLEAN, &allow_save,
                            DBUS_TYPE_INVALID);

  reply = dbus_connection_send_with_reply_and_block (dbus_conn, message,
                                                     -1, &derror);
  dbus_message_unref (message);

  if (G_UNLIKELY (reply == NULL))
    {
      xfce_session_logout_notify_error (_("Failed to receive a reply from the session manager"),
                                        &derror, have_display);
      dbus_error_free (&derror);
      return EXIT_FAILURE;
    }
  else if (dbus_message_get_type (reply) == DBUS_MESSAGE_TYPE_ERROR)
    {
      dbus_set_error_from_message (&derror, reply);
      xfce_session_logout_notify_error (_("Received error while trying to log out"),
                                        &derror, have_display);
      dbus_error_free (&derror);
      return EXIT_FAILURE;
    }
  dbus_message_unref (reply);

  return EXIT_SUCCESS;
}


