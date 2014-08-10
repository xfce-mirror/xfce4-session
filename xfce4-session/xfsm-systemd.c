/*-
 * Copyright (C) 2012 Christian Hesse
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

#include <config.h>

#include <gio/gio.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#ifdef HAVE_POLKIT
#include <polkit/polkit.h>
#endif

#include <libxfsm/xfsm-util.h>
#include <xfce4-session/xfsm-systemd.h>



#define SYSTEMD_DBUS_NAME               "org.freedesktop.login1"
#define SYSTEMD_DBUS_PATH               "/org/freedesktop/login1"
#define SYSTEMD_DBUS_INTERFACE          "org.freedesktop.login1.Manager"
#define SYSTEMD_REBOOT_ACTION           "Reboot"
#define SYSTEMD_POWEROFF_ACTION         "PowerOff"
#define SYSTEMD_SUSPEND_ACTION          "Suspend"
#define SYSTEMD_HIBERNATE_ACTION        "Hibernate"
#define SYSTEMD_REBOOT_TEST             "org.freedesktop.login1.reboot"
#define SYSTEMD_POWEROFF_TEST           "org.freedesktop.login1.power-off"
#define SYSTEMD_SUSPEND_TEST            "org.freedesktop.login1.suspend"
#define SYSTEMD_HIBERNATE_TEST          "org.freedesktop.login1.hibernate"



static void     xfsm_systemd_finalize     (GObject         *object);



struct _XfsmSystemdClass
{
  GObjectClass __parent__;
};

struct _XfsmSystemd
{
  GObject __parent__;
#ifdef HAVE_POLKIT
  PolkitAuthority *authority;
  PolkitSubject   *subject;
#endif
};



G_DEFINE_TYPE (XfsmSystemd, xfsm_systemd, G_TYPE_OBJECT)



static void
xfsm_systemd_class_init (XfsmSystemdClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfsm_systemd_finalize;
}



static void
xfsm_systemd_init (XfsmSystemd *systemd)
{
#ifdef HAVE_POLKIT
  systemd->authority = polkit_authority_get_sync (NULL, NULL);
  systemd->subject = polkit_unix_process_new_for_owner (getpid(), 0, -1);
#endif
}



static void
xfsm_systemd_finalize (GObject *object)
{
#ifdef HAVE_POLKIT
  XfsmSystemd *systemd = XFSM_SYSTEMD (object);

  g_object_unref (G_OBJECT (systemd->authority));
  g_object_unref (G_OBJECT (systemd->subject));
#endif

  (*G_OBJECT_CLASS (xfsm_systemd_parent_class)->finalize) (object);
}



static gboolean
xfsm_systemd_lock_screen (GError **error)
{
  XfconfChannel *channel;
  gboolean       ret = TRUE;

  channel = xfsm_open_config ();
  if (xfconf_channel_get_bool (channel, "/shutdown/LockScreen", FALSE))
      ret = g_spawn_command_line_async ("xflock4", error);

  return ret;
}



static gboolean
xfsm_systemd_can_method (XfsmSystemd  *systemd,
                         gboolean     *can_method,
                         const gchar  *method,
                         GError      **error)
{
#ifdef HAVE_POLKIT
  PolkitAuthorizationResult *res;
  GError                    *local_error = NULL;
#endif

  *can_method = FALSE;

#ifdef HAVE_POLKIT
  res = polkit_authority_check_authorization_sync (systemd->authority,
                                                   systemd->subject,
                                                   method,
                                                   NULL,
                                                   POLKIT_CHECK_AUTHORIZATION_FLAGS_NONE,
                                                   NULL,
                                                   &local_error);

  if (res == NULL)
    {
      g_propagate_error (error, local_error);
      return FALSE;
    }

  *can_method = polkit_authorization_result_get_is_authorized (res)
                || polkit_authorization_result_get_is_challenge (res);

  g_object_unref (G_OBJECT (res));
#endif

  return TRUE;
}



static gboolean
xfsm_systemd_try_method (XfsmSystemd  *systemd,
                         const gchar  *method,
                         GError      **error)
{
  GDBusConnection *bus;
  GError          *local_error = NULL;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, error);
  if (G_UNLIKELY (bus == NULL))
    return FALSE;

  g_dbus_connection_call_sync (bus,
                               SYSTEMD_DBUS_NAME,
                               SYSTEMD_DBUS_PATH,
                               SYSTEMD_DBUS_INTERFACE,
                               method,
                               g_variant_new ("(b)", TRUE),
                               NULL, 0, G_MAXINT, NULL,
                               &local_error);

  g_object_unref (G_OBJECT (bus));

  if (local_error != NULL)
    {
      g_propagate_error (error, local_error);
      return FALSE;
    }

  return TRUE;
}



XfsmSystemd *
xfsm_systemd_get (void)
{
  static XfsmSystemd *object = NULL;

  if (G_LIKELY (object != NULL))
    {
      g_object_ref (G_OBJECT (object));
    }
  else
    {
      object = g_object_new (XFSM_TYPE_SYSTEMD, NULL);
      g_object_add_weak_pointer (G_OBJECT (object), (gpointer) &object);
    }

  return object;
}



gboolean
xfsm_systemd_try_restart (XfsmSystemd  *systemd,
                          GError      **error)
{
  return xfsm_systemd_try_method (systemd,
                                  SYSTEMD_REBOOT_ACTION,
                                  error);
}



gboolean
xfsm_systemd_try_shutdown (XfsmSystemd  *systemd,
                           GError      **error)
{
  return xfsm_systemd_try_method (systemd,
                                  SYSTEMD_POWEROFF_ACTION,
                                  error);
}



gboolean
xfsm_systemd_try_suspend (XfsmSystemd  *systemd,
                          GError      **error)
{
  if (!xfsm_systemd_lock_screen (error))
    return FALSE;

  return xfsm_systemd_try_method (systemd,
                                  SYSTEMD_SUSPEND_ACTION,
                                  error);
}



gboolean
xfsm_systemd_try_hibernate (XfsmSystemd  *systemd,
                            GError      **error)
{
  if (!xfsm_systemd_lock_screen (error))
    return FALSE;

  return xfsm_systemd_try_method (systemd,
                                  SYSTEMD_HIBERNATE_ACTION,
                                  error);
}



gboolean
xfsm_systemd_can_restart (XfsmSystemd  *systemd,
                          gboolean     *can_restart,
                          GError      **error)
{
  return xfsm_systemd_can_method (systemd,
                                  can_restart,
                                  SYSTEMD_REBOOT_TEST,
                                  error);
}



gboolean
xfsm_systemd_can_shutdown (XfsmSystemd  *systemd,
                           gboolean     *can_shutdown,
                           GError      **error)
{
  return xfsm_systemd_can_method (systemd,
                                  can_shutdown,
                                  SYSTEMD_POWEROFF_TEST,
                                  error);
}



gboolean
xfsm_systemd_can_suspend (XfsmSystemd  *systemd,
                          gboolean     *can_suspend,
                          gboolean     *auth_suspend,
                          GError      **error)
{
  gboolean ret = FALSE;

  ret = xfsm_systemd_can_method (systemd,
                                 can_suspend,
                                 SYSTEMD_SUSPEND_TEST,
                                 error);
  *auth_suspend = *can_suspend;
  return ret;
}



gboolean
xfsm_systemd_can_hibernate (XfsmSystemd  *systemd,
                            gboolean     *can_hibernate,
                            gboolean     *auth_hibernate,
                            GError      **error)
{
  gboolean ret = FALSE;

  ret = xfsm_systemd_can_method (systemd,
                                 can_hibernate,
                                 SYSTEMD_HIBERNATE_TEST,
                                 error);
  *auth_hibernate = *can_hibernate;
  return ret;
}
