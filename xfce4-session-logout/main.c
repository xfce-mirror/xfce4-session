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
#include <libxfcegui4/libxfcegui4.h>


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


static void
usage (int exit_code)
{
  fprintf (stderr,
           "Usage: %s [OPTION...]\n"
           "\n"
           "GTK+\n"
           "  --display=DISPLAY     X display to use\n"
           "\n"
           "Application options\n"
           "  --logout              Log out without displaying the logout dialog\n"
           "  --halt                Halt without displaing the logout dialog\n"
           "  --reboot              Reboot without displaying the logout dialog\n"
           "  --suspend             Suspend without displaying the logout dialog\n"
           "  --hibernate           Hibernate without displaying the logout dialog\n"
           "  --fast                Log out quickly; don't save the session\n"
           "\n"
           "  --help                Print this help message and exit\n"
           "  --version             Print version information and exit\n"
           "\n",
           PACKAGE_STRING);
  exit (exit_code);
}



int
main (int argc, char **argv)
{
  XfsmShutdownType shutdown_type = XFSM_SHUTDOWN_ASK;
  gboolean allow_save = TRUE;
  DBusConnection *dbus_conn;
  DBusMessage *message, *reply;
  DBusError derror;
  gboolean have_display = gtk_init_check (&argc, &argv);

  for (++argv; --argc > 0; ++argv)
    {
      if (strcmp (*argv, "--logout") == 0)
        {
          shutdown_type = XFSM_SHUTDOWN_LOGOUT;
        }
      else if (strcmp (*argv, "--halt") == 0)
        {
          shutdown_type = XFSM_SHUTDOWN_HALT;
        }
      else if (strcmp (*argv, "--reboot") == 0)
        {
          shutdown_type = XFSM_SHUTDOWN_REBOOT;
        }
      else if (strcmp (*argv, "--suspend") == 0)
        {
          shutdown_type = XFSM_SHUTDOWN_SUSPEND;
        }
      else if (strcmp (*argv, "--hibernate") == 0)
        {
          shutdown_type = XFSM_SHUTDOWN_HIBERNATE;
        }
      else if (strcmp (*argv, "--fast") == 0)
        {
          allow_save = FALSE;
        }
      else if (strcmp (*argv, "--version") == 0)
        {
          printf ("%s (Xfce %s)\n\n"
                  "Copyright (c) 2004\n"
                  "        The Xfce development team. All rights reserved.\n\n"
                  "Written for Xfce by Brian Tarricone <kelnos@xfce.org> and\n"
                  "Benedikt Meurer <benny@xfce.org>.\n\n"
                  "Please report bugs to <%s>.\n",
                  PACKAGE_STRING, xfce_version_string(), PACKAGE_BUGREPORT);
          return EXIT_SUCCESS;
        }
      else
        {
          usage (strcmp (*argv, "--help") == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
        }
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


