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

#include <libxfsm/xfsm-util.h>
#include <xfce4-session/xfsm-error.h>
#include <xfce4-session/xfsm-systemd.h>



#define SYSTEMD_DBUS_NAME               "org.freedesktop.login1"
#define SYSTEMD_DBUS_PATH               "/org/freedesktop/login1"
#define SYSTEMD_DBUS_INTERFACE          "org.freedesktop.login1.Manager"
#define SYSTEMD_REBOOT_ACTION           "Reboot"
#define SYSTEMD_POWEROFF_ACTION         "PowerOff"
#define SYSTEMD_SUSPEND_ACTION          "Suspend"
#define SYSTEMD_HIBERNATE_ACTION        "Hibernate"
#define SYSTEMD_HYBRID_SLEEP_ACTION     "HybridSleep"
#define SYSTEMD_REBOOT_TEST             "CanReboot"
#define SYSTEMD_POWEROFF_TEST           "CanPowerOff"
#define SYSTEMD_SUSPEND_TEST            "CanSuspend"
#define SYSTEMD_HIBERNATE_TEST          "CanHibernate"
#define SYSTEMD_HYBRID_SLEEP_TEST       "CanHybridSleep"



static void     xfsm_systemd_finalize     (GObject         *object);



struct _XfsmSystemdClass
{
  GObjectClass __parent__;
};

struct _XfsmSystemd
{
  GObject __parent__;

  GDBusProxy *proxy;
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
name_appeared (GDBusConnection *connection,
               const gchar *name,
               const gchar *name_owner,
               gpointer user_data)
{
  XfsmSystemd *systemd = user_data;
  GError *error = NULL;

  g_debug ("%s started up, owned by %s", name, name_owner);

  systemd->proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                  G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                                  NULL,
                                                  SYSTEMD_DBUS_NAME,
                                                  SYSTEMD_DBUS_PATH,
                                                  SYSTEMD_DBUS_INTERFACE,
                                                  NULL,
                                                  &error);
  if (error != NULL)
    {
      g_warning ("Failed to get a systemd proxy: %s", error->message);
      g_error_free (error);
    }
}



static void
name_vanished (GDBusConnection *connection,
               const gchar *name,
               gpointer user_data)
{
  XfsmSystemd *systemd = user_data;

  g_debug ("%s vanished", name);

  g_clear_object (&systemd->proxy);
}



static void
xfsm_systemd_init (XfsmSystemd *systemd)
{
  g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                    SYSTEMD_DBUS_NAME,
                    G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                    name_appeared,
                    name_vanished,
                    systemd,
                    NULL);
}



static void
xfsm_systemd_finalize (GObject *object)
{
  XfsmSystemd *systemd = XFSM_SYSTEMD (object);

  g_clear_object (&systemd->proxy);

  (*G_OBJECT_CLASS (xfsm_systemd_parent_class)->finalize) (object);
}



static gboolean
xfsm_systemd_can_method (XfsmSystemd  *systemd,
                         gboolean     *can_method_out,
                         const gchar  *method,
                         GError      **error)
{
  GVariant *variant;
  const gchar *can_string;
  gboolean can_method;

  /* never return true if something fails */
  if (can_method_out != NULL)
    *can_method_out = FALSE;

  if (systemd->proxy == NULL)
    {
      g_debug ("No systemd proxy");
      return FALSE;
    }

  g_debug ("Calling %s", method);

  variant = g_dbus_proxy_call_sync (systemd->proxy,
                                    method,
                                    NULL,
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    NULL,
                                    error);

  if (variant == NULL)
    return FALSE;

  g_variant_get_child (variant, 0, "&s", &can_string);
  can_method = g_strcmp0 (can_string, "yes") == 0;
  g_variant_unref (variant);

  if (can_method_out != NULL)
    *can_method_out = can_method;

  return TRUE;
}



static gboolean
xfsm_systemd_try_method (XfsmSystemd  *systemd,
                         const gchar  *method,
                         GError      **error)
{
  GVariant *variant;

  if (systemd->proxy == NULL)
    {
      g_debug ("No systemd proxy");
      return FALSE;
    }

  g_debug ("Calling %s", method);

  variant = g_dbus_proxy_call_sync (systemd->proxy,
                                    method,
                                    g_variant_new ("(b)", TRUE),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    NULL,
                                    error);

  if (variant == NULL)
    return FALSE;

  g_variant_unref (variant);

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
  g_return_val_if_fail (XFSM_IS_SYSTEMD (systemd), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return xfsm_systemd_try_method (systemd,
                                  SYSTEMD_REBOOT_ACTION,
                                  error);
}



gboolean
xfsm_systemd_try_shutdown (XfsmSystemd  *systemd,
                           GError      **error)
{
  g_return_val_if_fail (XFSM_IS_SYSTEMD (systemd), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return xfsm_systemd_try_method (systemd,
                                  SYSTEMD_POWEROFF_ACTION,
                                  error);
}



gboolean
xfsm_systemd_try_suspend (XfsmSystemd  *systemd,
                          GError      **error)
{
  g_return_val_if_fail (XFSM_IS_SYSTEMD (systemd), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return xfsm_systemd_try_method (systemd,
                                  SYSTEMD_SUSPEND_ACTION,
                                  error);
}



gboolean
xfsm_systemd_try_hibernate (XfsmSystemd  *systemd,
                            GError      **error)
{
  g_return_val_if_fail (XFSM_IS_SYSTEMD (systemd), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return xfsm_systemd_try_method (systemd,
                                  SYSTEMD_HIBERNATE_ACTION,
                                  error);
}



gboolean
xfsm_systemd_try_hybrid_sleep (XfsmSystemd  *systemd,
                               GError      **error)
{
  g_return_val_if_fail (XFSM_IS_SYSTEMD (systemd), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return xfsm_systemd_try_method (systemd,
                                  SYSTEMD_HYBRID_SLEEP_ACTION,
                                  error);
}



gboolean
xfsm_systemd_can_restart (XfsmSystemd  *systemd,
                          gboolean     *can_restart,
                          GError      **error)
{
  g_return_val_if_fail (XFSM_IS_SYSTEMD (systemd), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

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
  g_return_val_if_fail (XFSM_IS_SYSTEMD (systemd), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

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
  gboolean ret, can_method;

  g_return_val_if_fail (XFSM_IS_SYSTEMD (systemd), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = xfsm_systemd_can_method (systemd,
                                 &can_method,
                                 SYSTEMD_SUSPEND_TEST,
                                 error);

  if (can_suspend != NULL)
    *can_suspend = can_method;
  if (auth_suspend != NULL)
    *auth_suspend = can_method;

  return ret;
}



gboolean
xfsm_systemd_can_hibernate (XfsmSystemd  *systemd,
                            gboolean     *can_hibernate,
                            gboolean     *auth_hibernate,
                            GError      **error)
{
  gboolean ret, can_method;

  g_return_val_if_fail (XFSM_IS_SYSTEMD (systemd), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = xfsm_systemd_can_method (systemd,
                                 &can_method,
                                 SYSTEMD_HIBERNATE_TEST,
                                 error);

  if (can_hibernate != NULL)
    *can_hibernate = can_method;
  if (auth_hibernate != NULL)
    *auth_hibernate = can_method;

  return ret;
}



gboolean
xfsm_systemd_can_hybrid_sleep (XfsmSystemd  *systemd,
                               gboolean     *can_hybrid_sleep,
                               gboolean     *auth_hybrid_sleep,
                               GError      **error)
{
  gboolean ret, can_method;

  g_return_val_if_fail (XFSM_IS_SYSTEMD (systemd), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = xfsm_systemd_can_method (systemd,
                                 &can_method,
                                 SYSTEMD_HYBRID_SLEEP_TEST,
                                 error);

  if (can_hybrid_sleep != NULL)
    *can_hybrid_sleep = can_method;
  if (auth_hybrid_sleep != NULL)
    *auth_hybrid_sleep = can_method;

  return ret;
}
