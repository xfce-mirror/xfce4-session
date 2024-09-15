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
#include "config.h"
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

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <stdio.h>


gboolean opt_logout = FALSE;
gboolean opt_halt = FALSE;
gboolean opt_reboot = FALSE;
gboolean opt_suspend = FALSE;
gboolean opt_hibernate = FALSE;
gboolean opt_hybrid_sleep = FALSE;
gboolean opt_switch_user = FALSE;
gboolean opt_fast = FALSE;
gboolean opt_version = FALSE;

static GOptionEntry option_entries[] = {
  { "logout", 'l', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_logout,
    N_ ("Log out without displaying the logout dialog"),
    NULL },
  { "halt", 'h', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_halt,
    N_ ("Halt without displaying the logout dialog"),
    NULL },
  { "reboot", 'r', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_reboot,
    N_ ("Reboot without displaying the logout dialog"),
    NULL },
  { "suspend", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_suspend,
    N_ ("Suspend without displaying the logout dialog"),
    NULL },
  { "hibernate", 'i', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_hibernate,
    N_ ("Hibernate without displaying the logout dialog"),
    NULL },
  { "hybrid-sleep", 'b', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_hybrid_sleep,
    N_ ("Hybrid Sleep without displaying the logout dialog"),
    NULL },
  { "switch-user", 'u', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_switch_user,
    N_ ("Switch user without displaying the logout dialog"),
    NULL },
  { "fast", 'f', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_fast,
    N_ ("Log out quickly; don't save the session"),
    NULL },
  { "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_version,
    N_ ("Print version information and exit"),
    NULL },
  { NULL }
};


static void
xfce_session_logout_notify_error (const gchar *message,
                                  GError *error,
                                  gboolean have_display)
{
  if (G_LIKELY (have_display))
    {
      xfce_dialog_show_error (NULL, error, "%s", message);
    }
  else
    {
      g_warning ("%s (%s)", message, error != NULL ? error->message : _("Unknown error"));
    }
}



int
main (int argc, char **argv)
{
  GDBusProxy *proxy = NULL;
  GError *err = NULL;
  gboolean have_display;
  gboolean show_dialog;
  GVariant *result = NULL;
  gboolean allow_save;

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  have_display = gtk_init_with_args (&argc, &argv, NULL, option_entries, PACKAGE, NULL);

  if (opt_version)
    {
      g_print ("%s %s (Xfce %s)\n\n", PACKAGE_NAME, PACKAGE_VERSION, xfce_version_string ());
      g_print ("%s\n", "Copyright (c) 2004-2024");
      g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
      g_print ("%s\n", _("Written by Benedikt Meurer <benny@xfce.org>"));
      g_print ("%s\n\n", _("and Brian Tarricone <kelnos@xfce.org>."));
      g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
      g_print ("\n");
      return EXIT_SUCCESS;
    }

  /* save the session, unless fast is provided */
  allow_save = !opt_fast;

  /* create messsage */
  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         NULL,
                                         "org.xfce.SessionManager",
                                         "/org/xfce/SessionManager",
                                         "org.xfce.Session.Manager",
                                         NULL,
                                         &err);
  if (proxy == NULL)
    {
      xfce_session_logout_notify_error (_("Received error while trying to log out"), err, have_display);
      g_error_free (err);
      return EXIT_FAILURE;
    }

  if (opt_halt)
    {
      result = g_dbus_proxy_call_sync (proxy, "Shutdown",
                                       g_variant_new ("(b)", allow_save),
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1,
                                       NULL,
                                       &err);
    }
  else if (opt_reboot)
    {
      result = g_dbus_proxy_call_sync (proxy, "Restart",
                                       g_variant_new ("(b)", allow_save),
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1,
                                       NULL,
                                       &err);
    }
  else if (opt_suspend)
    {
      result = g_dbus_proxy_call_sync (proxy, "Suspend",
                                       g_variant_new ("()"),
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1,
                                       NULL,
                                       &err);
    }
  else if (opt_hibernate)
    {
      result = g_dbus_proxy_call_sync (proxy, "Hibernate",
                                       g_variant_new ("()"),
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1,
                                       NULL,
                                       &err);
    }
  else if (opt_hybrid_sleep)
    {
      result = g_dbus_proxy_call_sync (proxy, "HybridSleep",
                                       g_variant_new ("()"),
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1,
                                       NULL,
                                       &err);
    }
  else if (opt_switch_user)
    {
      result = g_dbus_proxy_call_sync (proxy, "SwitchUser",
                                       g_variant_new ("()"),
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1,
                                       NULL,
                                       &err);
    }
  else
    {
      show_dialog = !opt_logout;
      result = g_dbus_proxy_call_sync (proxy, "Logout",
                                       g_variant_new ("(bb)", show_dialog, allow_save),
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1,
                                       NULL,
                                       &err);
    }

  if (proxy != NULL)
    g_object_unref (proxy);

  if (!result)
    {
      xfce_session_logout_notify_error (_("Received error while trying to log out"), err, have_display);
      g_clear_error (&err);
      return EXIT_FAILURE;
    }
  else
    {
      g_variant_unref (result);
    }

  return EXIT_SUCCESS;
}
