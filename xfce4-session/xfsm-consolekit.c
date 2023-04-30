/*-
 * Copyright (c) 2010 Ali Abdallah <aliov@xfce.org>
 * Copyright (c) 2011 Nick Schermer <nick@xfce.org>
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


#include <gio/gio.h>

#include <xfce4-session/xfsm-consolekit.h>
#include <libxfsm/xfsm-util.h>
#include "xfsm-global.h"

#define CK_NAME         "org.freedesktop.ConsoleKit"
#define CK_MANAGER_PATH "/org/freedesktop/ConsoleKit/Manager"
#define CK_MANAGER_NAME CK_NAME ".Manager"



static void     xfsm_consolekit_finalize     (GObject         *object);



struct _XfsmConsolekitClass
{
  GObjectClass __parent__;
};

struct _XfsmConsolekit
{
  GObject __parent__;

  GDBusProxy *proxy;
};



G_DEFINE_TYPE (XfsmConsolekit, xfsm_consolekit, G_TYPE_OBJECT)



static void
xfsm_consolekit_class_init (XfsmConsolekitClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfsm_consolekit_finalize;
}



static void
name_appeared (GDBusConnection *connection,
               const gchar *name,
               const gchar *name_owner,
               gpointer user_data)
{
  XfsmConsolekit *consolekit = user_data;
  GError *error = NULL;

  g_debug ("%s started up, owned by %s", name, name_owner);

  consolekit->proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                     G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                                     NULL,
                                                     CK_NAME,
                                                     CK_MANAGER_PATH,
                                                     CK_MANAGER_NAME,
                                                     NULL,
                                                     &error);
  if (error != NULL)
    {
      g_warning ("Failed to get a ConsoleKit proxy: %s", error->message);
      g_error_free (error);
    }
}



static void
name_vanished (GDBusConnection *connection,
               const gchar *name,
               gpointer user_data)
{
  XfsmConsolekit *consolekit = user_data;

  g_debug ("%s vanished", name);

  g_clear_object (&consolekit->proxy);
}



static void
xfsm_consolekit_init (XfsmConsolekit *consolekit)
{
  g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                    CK_NAME,
                    G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                    name_appeared,
                    name_vanished,
                    consolekit,
                    NULL);
}



static void
xfsm_consolekit_finalize (GObject *object)
{
  XfsmConsolekit *consolekit = XFSM_CONSOLEKIT (object);

  g_clear_object (&consolekit->proxy);

  (*G_OBJECT_CLASS (xfsm_consolekit_parent_class)->finalize) (object);
}



static gboolean
xfsm_consolekit_can_method (XfsmConsolekit  *consolekit,
                            const gchar     *method,
                            gboolean        *can_method_out,
                            GError         **error)
{
  GVariant *variant = NULL;
  gboolean can_method;

  /* never return true if something fails */
  if (can_method_out != NULL)
    *can_method_out = FALSE;

  if (consolekit->proxy == NULL)
    {
      g_debug ("No ConsoleKit proxy");
      return FALSE;
    }

  g_debug ("Calling %s", method);

  variant = g_dbus_proxy_call_sync (consolekit->proxy,
                                    method,
                                    NULL,
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    NULL,
                                    error);

  if (variant == NULL)
    return FALSE;

  g_variant_get_child (variant, 0, "b", &can_method);
  g_variant_unref (variant);

  if (can_method_out != NULL)
    *can_method_out = can_method;

  return TRUE;
}



static gboolean
xfsm_consolekit_can_sleep (XfsmConsolekit  *consolekit,
                           const gchar     *method,
                           gboolean        *can_method_out,
                           gboolean        *auth_method_out,
                           GError         **error)
{
  const gchar *can_string;
  GVariant *variant = NULL;
  gboolean can_method;

  /* never return true if something fails */
  if (can_method_out != NULL)
    *can_method_out = FALSE;
  if (auth_method_out != NULL)
    *auth_method_out = FALSE;

  if (consolekit->proxy == NULL)
    {
      g_debug ("No ConsoleKit proxy");
      return FALSE;
    }

  g_debug ("Calling %s", method);

  variant = g_dbus_proxy_call_sync (consolekit->proxy,
                                    method,
                                    NULL,
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    NULL,
                                    error);

  if (variant == NULL)
    return FALSE;

  g_variant_get_child (variant, 0, "&s", &can_string);

  /* If yes or challenge then we can sleep, it just might take a password */
  can_method = g_strcmp0 (can_string, "yes") == 0 || g_strcmp0 (can_string, "challenge") == 0;

  g_variant_unref (variant);

  if (can_method_out != NULL)
    *can_method_out = can_method;
  if (auth_method_out != NULL)
    *auth_method_out = can_method;

  return TRUE;
}



static gboolean
xfsm_consolekit_try_method (XfsmConsolekit  *consolekit,
                            const gchar     *method,
                            GError         **error)
{
  GVariant *variant = NULL;

  if (consolekit->proxy == NULL)
    {
      g_debug ("No ConsoleKit proxy");
      return FALSE;
    }

  g_debug ("Calling %s", method);

  variant = g_dbus_proxy_call_sync (consolekit->proxy,
                                    method,
                                    NULL,
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    NULL,
                                    error);

  if (variant == NULL)
    return FALSE;

  g_variant_unref (variant);
  return TRUE;
}



static gboolean
xfsm_consolekit_try_sleep (XfsmConsolekit  *consolekit,
                           const gchar     *method,
                           GError         **error)
{
  GVariant *variant = NULL;

  if (consolekit->proxy == NULL)
    {
      g_debug ("No ConsoleKit proxy");
      return FALSE;
    }

  g_debug ("Calling %s", method);

  variant = g_dbus_proxy_call_sync (consolekit->proxy,
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



XfsmConsolekit *
xfsm_consolekit_get (void)
{
  static XfsmConsolekit *object = NULL;

  if (G_LIKELY (object != NULL))
    {
      g_object_ref (G_OBJECT (object));
    }
  else
    {
      object = g_object_new (XFSM_TYPE_CONSOLEKIT, NULL);
      g_object_add_weak_pointer (G_OBJECT (object), (gpointer) &object);
    }

  return object;
}



gboolean
xfsm_consolekit_try_restart (XfsmConsolekit  *consolekit,
                             GError         **error)
{
  g_return_val_if_fail (XFSM_IS_CONSOLEKIT (consolekit), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return xfsm_consolekit_try_method (consolekit, "Restart", error);
}



gboolean
xfsm_consolekit_try_shutdown (XfsmConsolekit  *consolekit,
                              GError         **error)
{
  g_return_val_if_fail (XFSM_IS_CONSOLEKIT (consolekit), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return xfsm_consolekit_try_method (consolekit, "Stop", error);
}



gboolean
xfsm_consolekit_can_restart (XfsmConsolekit  *consolekit,
                             gboolean        *can_restart,
                             GError         **error)
{
  g_return_val_if_fail (XFSM_IS_CONSOLEKIT (consolekit), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return xfsm_consolekit_can_method (consolekit, "CanRestart",
                                     can_restart, error);
}



gboolean
xfsm_consolekit_can_shutdown (XfsmConsolekit  *consolekit,
                              gboolean        *can_shutdown,
                              GError         **error)
{
  g_return_val_if_fail (XFSM_IS_CONSOLEKIT (consolekit), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return xfsm_consolekit_can_method (consolekit, "CanStop",
                                     can_shutdown, error);
}



gboolean
xfsm_consolekit_try_suspend (XfsmConsolekit  *consolekit,
                             GError         **error)
{
  g_return_val_if_fail (XFSM_IS_CONSOLEKIT (consolekit), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return xfsm_consolekit_try_sleep (consolekit, "Suspend", error);
}



gboolean
xfsm_consolekit_try_hibernate (XfsmConsolekit  *consolekit,
                               GError         **error)
{
  g_return_val_if_fail (XFSM_IS_CONSOLEKIT (consolekit), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return xfsm_consolekit_try_sleep (consolekit, "Hibernate", error);
}



gboolean
xfsm_consolekit_try_hybrid_sleep (XfsmConsolekit  *consolekit,
                                  GError         **error)
{
  g_return_val_if_fail (XFSM_IS_CONSOLEKIT (consolekit), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return xfsm_consolekit_try_sleep (consolekit, "HybridSleep", error);
}



gboolean
xfsm_consolekit_can_suspend (XfsmConsolekit  *consolekit,
                             gboolean        *can_suspend,
                             gboolean        *auth_suspend,
                             GError         **error)
{
  g_return_val_if_fail (XFSM_IS_CONSOLEKIT (consolekit), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return xfsm_consolekit_can_sleep (consolekit, "CanSuspend",
                                    can_suspend, auth_suspend, error);
}



gboolean
xfsm_consolekit_can_hibernate (XfsmConsolekit  *consolekit,
                               gboolean        *can_hibernate,
                               gboolean        *auth_hibernate,
                               GError         **error)
{
  g_return_val_if_fail (XFSM_IS_CONSOLEKIT (consolekit), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return xfsm_consolekit_can_sleep (consolekit, "CanHibernate",
                                    can_hibernate, auth_hibernate, error);
}



gboolean
xfsm_consolekit_can_hybrid_sleep (XfsmConsolekit  *consolekit,
                                  gboolean        *can_hybrid_sleep,
                                  gboolean        *auth_hybrid_sleep,
                                  GError         **error)
{
  g_return_val_if_fail (XFSM_IS_CONSOLEKIT (consolekit), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return xfsm_consolekit_can_sleep (consolekit, "CanHybridSleep",
                                    can_hybrid_sleep, auth_hybrid_sleep, error);
}
