/*-
 * Copyright (C) 2021 Matias De lellis
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

#include <gio/gio.h>

#include "xfsm-packagekit.h"



#define PACKAGEKIT_DBUS_NAME "org.freedesktop.PackageKit"
#define PACKAGEKIT_DBUS_PATH "/org/freedesktop/PackageKit"
#define PACKAGEKIT_OFFLINE_DBUS_INTERFACE "org.freedesktop.PackageKit.Offline"

#define DBUS_PROPERTIES_INTERFACE "org.freedesktop.DBus.Properties"

#define PACKAGEKIT_GET_PROPERTY_METHOD "Get"
#define PACKAGEKIT_UPDATE_PREPARED_PROPERTY "UpdatePrepared"

#define PACKAGEKIT_TRIGGER_ACTION_METHOD "Trigger"
#define PACKAGEKIT_TRIGGER_ACTION_RESTART "reboot"
#define PACKAGEKIT_TRIGGER_ACTION_SHUTDOWN "power-off"



static void
xfsm_packagekit_finalize (GObject *object);



struct _XfsmPackagekit
{
  GObject __parent__;
};



G_DEFINE_TYPE (XfsmPackagekit, xfsm_packagekit, G_TYPE_OBJECT)



static void
xfsm_packagekit_class_init (XfsmPackagekitClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfsm_packagekit_finalize;
}



static void
xfsm_packagekit_init (XfsmPackagekit *packagekit)
{
}



static void
xfsm_packagekit_finalize (GObject *object)
{
  (*G_OBJECT_CLASS (xfsm_packagekit_parent_class)->finalize) (object);
}



static gboolean
xfsm_packagekit_get_bool_property (XfsmPackagekit *packagekit,
                                   gboolean *bool_property,
                                   const gchar *property_name,
                                   GError **error)
{
  GDBusConnection *bus;
  GError *local_error = NULL;
  GVariant *dbus_ret;
  GVariant *prop;

  g_return_val_if_fail (XFSM_IS_PACKAGEKIT (packagekit), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, error);
  if (G_UNLIKELY (bus == NULL))
    return FALSE;

  dbus_ret = g_dbus_connection_call_sync (bus,
                                          PACKAGEKIT_DBUS_NAME,
                                          PACKAGEKIT_DBUS_PATH,
                                          DBUS_PROPERTIES_INTERFACE,
                                          PACKAGEKIT_GET_PROPERTY_METHOD,
                                          g_variant_new ("(ss)",
                                                         PACKAGEKIT_OFFLINE_DBUS_INTERFACE,
                                                         property_name),
                                          G_VARIANT_TYPE ("(v)"),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1,
                                          NULL,
                                          &local_error);

  if (dbus_ret != NULL)
    {
      g_variant_get (dbus_ret, "(v)", &prop);
      g_variant_unref (dbus_ret);

      *bool_property = g_variant_get_boolean (prop);
      g_variant_unref (prop);
    }

  g_object_unref (G_OBJECT (bus));

  if (local_error == NULL)
    return TRUE;

  if (g_error_matches (local_error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN))
    {
      *bool_property = FALSE;
      g_error_free (local_error);
      return TRUE;
    }

  g_propagate_error (error, local_error);

  return FALSE;
}



static gboolean
xfsm_packagekit_trigger_update_action (XfsmPackagekit *packagekit,
                                       const gchar *action,
                                       GError **error)
{
  GDBusConnection *bus;
  GError *local_error = NULL;

  g_return_val_if_fail (XFSM_IS_PACKAGEKIT (packagekit), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, error);
  if (G_UNLIKELY (bus == NULL))
    return FALSE;

  g_dbus_connection_call_sync (bus,
                               PACKAGEKIT_DBUS_NAME,
                               PACKAGEKIT_DBUS_PATH,
                               PACKAGEKIT_OFFLINE_DBUS_INTERFACE,
                               PACKAGEKIT_TRIGGER_ACTION_METHOD,
                               g_variant_new ("(s)", action),
                               NULL,
                               G_DBUS_CALL_FLAGS_NONE,
                               G_MAXINT,
                               NULL,
                               &local_error);

  g_object_unref (G_OBJECT (bus));

  if (local_error != NULL)
    {
      g_propagate_error (error, local_error);
      return FALSE;
    }

  return TRUE;
}



XfsmPackagekit *
xfsm_packagekit_get (void)
{
  static XfsmPackagekit *object = NULL;

  if (G_LIKELY (object != NULL))
    {
      g_object_ref (G_OBJECT (object));
    }
  else
    {
      object = g_object_new (XFSM_TYPE_PACKAGEKIT, NULL);
      g_object_add_weak_pointer (G_OBJECT (object), (gpointer) &object);
    }

  return object;
}



gboolean
xfsm_packagekit_has_update_prepared (XfsmPackagekit *packagekit,
                                     gboolean *update_prepared,
                                     GError **error)
{
  return xfsm_packagekit_get_bool_property (packagekit,
                                            update_prepared,
                                            PACKAGEKIT_UPDATE_PREPARED_PROPERTY,
                                            error);
}



gboolean
xfsm_packagekit_try_trigger_shutdown (XfsmPackagekit *packagekit,
                                      GError **error)
{
  return xfsm_packagekit_trigger_update_action (packagekit,
                                                PACKAGEKIT_TRIGGER_ACTION_SHUTDOWN,
                                                error);
}



gboolean
xfsm_packagekit_try_trigger_restart (XfsmPackagekit *packagekit,
                                     GError **error)
{
  return xfsm_packagekit_trigger_update_action (packagekit,
                                                PACKAGEKIT_TRIGGER_ACTION_RESTART,
                                                error);
}
