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


#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <xfce4-session/xfsm-consolekit.h>
#include <libxfsm/xfsm-util.h>


#define CK_NAME         "org.freedesktop.ConsoleKit"
#define CK_MANAGER_PATH "/org/freedesktop/ConsoleKit/Manager"
#define CK_MANAGER_NAME CK_NAME ".Manager"



static void     xfsm_consolekit_finalize     (GObject         *object);
static gboolean xfsm_consolekit_proxy_ensure (XfsmConsolekit  *consolekit,
                                              GError         **error);
static void     xfsm_consolekit_proxy_free   (XfsmConsolekit *consolekit);



struct _XfsmConsolekitClass
{
  GObjectClass __parent__;
};

struct _XfsmConsolekit
{
  GObject __parent__;

  DBusGConnection *dbus_conn;
  DBusGProxy      *ck_proxy;
  DBusGProxy      *dbus_proxy;
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
xfsm_consolekit_init (XfsmConsolekit *consolekit)
{
}



static void
xfsm_consolekit_finalize (GObject *object)
{
  xfsm_consolekit_proxy_free (XFSM_CONSOLEKIT (object));

  (*G_OBJECT_CLASS (xfsm_consolekit_parent_class)->finalize) (object);
}



static DBusHandlerResult
xfsm_consolekit_dbus_filter (DBusConnection *connection,
                             DBusMessage    *message,
                             gpointer        data)
{
  g_return_val_if_fail (XFSM_IS_CONSOLEKIT (data), DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

  if (dbus_message_is_signal (message, DBUS_INTERFACE_LOCAL, "Disconnected")
      && g_strcmp0 (dbus_message_get_path (message), DBUS_PATH_LOCAL) == 0)
    {
      g_debug ("Consolekit disconnected");
      xfsm_consolekit_proxy_free (XFSM_CONSOLEKIT (data));
    }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}



static void
xfsm_consolekit_name_owner_changed (DBusGProxy     *dbus_proxy,
                                    const gchar    *name,
                                    const gchar    *prev_owner,
                                    const gchar    *new_owner,
                                    XfsmConsolekit *consolekit)
{
  GError *err = NULL;

  g_return_if_fail (XFSM_IS_CONSOLEKIT (consolekit));
  g_return_if_fail (consolekit->dbus_proxy == dbus_proxy);

  if (g_strcmp0 (name, CK_NAME) == 0)
    {
      g_debug ("Consolekit owner changed");

      /* only reconnect the consolekit proxy */
      if (consolekit->ck_proxy != NULL)
        {
          g_object_unref (G_OBJECT (consolekit->ck_proxy));
          consolekit->ck_proxy = NULL;
        }

      if (!xfsm_consolekit_proxy_ensure (consolekit, &err))
        {
          g_warning ("Failed to reconnect to consolekit: %s", err->message);
          g_error_free (err);
        }
    }
}



static gboolean
xfsm_consolekit_proxy_ensure (XfsmConsolekit  *consolekit,
                              GError         **error)
{
  GError         *err = NULL;
  DBusConnection *connection;

  if (consolekit->dbus_conn == NULL)
    {
      consolekit->dbus_conn = dbus_g_bus_get (DBUS_BUS_SYSTEM, &err);
      if (consolekit->dbus_conn == NULL)
        goto error1;

      connection = dbus_g_connection_get_connection (consolekit->dbus_conn);
      dbus_connection_set_exit_on_disconnect (connection, FALSE);
      dbus_connection_add_filter (connection, xfsm_consolekit_dbus_filter, consolekit, NULL);
    }

  if (consolekit->dbus_proxy == NULL)
    {
      consolekit->dbus_proxy = dbus_g_proxy_new_for_name_owner (consolekit->dbus_conn,
                                                                DBUS_SERVICE_DBUS,
                                                                DBUS_PATH_DBUS,
                                                                DBUS_INTERFACE_DBUS,
                                                                &err);
      if (consolekit->dbus_proxy == NULL)
        goto error1;

      /* (dis)connect to consolekit if stopped/started */
      dbus_g_proxy_add_signal (consolekit->dbus_proxy,
                               "NameOwnerChanged",
                               G_TYPE_STRING,
                               G_TYPE_STRING,
                               G_TYPE_STRING,
                               G_TYPE_INVALID);

      dbus_g_proxy_connect_signal (consolekit->dbus_proxy,
                                   "NameOwnerChanged",
                                   G_CALLBACK (xfsm_consolekit_name_owner_changed),
                                   consolekit, NULL);
    }

  if (consolekit->ck_proxy == NULL)
    {
      consolekit->ck_proxy = dbus_g_proxy_new_for_name_owner (consolekit->dbus_conn,
                                                              CK_NAME,
                                                              CK_MANAGER_PATH,
                                                              CK_MANAGER_NAME,
                                                              &err);
      if (consolekit->ck_proxy == NULL)
        goto error1;
    }

  return TRUE;

  error1:

  g_propagate_error (error, err);
  xfsm_consolekit_proxy_free (consolekit);

  return FALSE;
}



static void
xfsm_consolekit_proxy_free (XfsmConsolekit *consolekit)
{
  DBusConnection *connection;

  if (consolekit->ck_proxy != NULL)
    {
      g_object_unref (G_OBJECT (consolekit->ck_proxy));
      consolekit->ck_proxy = NULL;
    }

  if (consolekit->dbus_proxy != NULL)
    {
      g_object_unref (G_OBJECT (consolekit->dbus_proxy));
      consolekit->dbus_proxy = NULL;
    }

  if (consolekit->dbus_conn != NULL)
    {
      connection = dbus_g_connection_get_connection (consolekit->dbus_conn);
      dbus_connection_remove_filter (connection,
                                     xfsm_consolekit_dbus_filter,
                                     consolekit);

      dbus_g_connection_unref (consolekit->dbus_conn);
      consolekit->dbus_conn = NULL;
    }
}



static gboolean
xfsm_consolekit_can_method (XfsmConsolekit  *consolekit,
                            const gchar     *method,
                            gboolean        *can_method,
                            GError         **error)
{
  g_return_val_if_fail (can_method != NULL, FALSE);

  /* never return true if something fails */
  *can_method = FALSE;

  if (!xfsm_consolekit_proxy_ensure (consolekit, error))
    return FALSE;

  return dbus_g_proxy_call (consolekit->ck_proxy, method,
                            error, G_TYPE_INVALID,
                            G_TYPE_BOOLEAN, can_method,
                            G_TYPE_INVALID);
}



static gboolean
xfsm_consolekit_can_sleep (XfsmConsolekit  *consolekit,
                           const gchar     *method,
                           gboolean        *can_method,
                           gboolean        *auth_method,
                           GError         **error)
{
  gboolean ret;
  gchar *can_string;
  g_return_val_if_fail (can_method != NULL, FALSE);

  /* never return true if something fails */
  *can_method = FALSE;

  if (!xfsm_consolekit_proxy_ensure (consolekit, error))
    return FALSE;

  ret = dbus_g_proxy_call (consolekit->ck_proxy, method,
                            error, G_TYPE_INVALID,
                            G_TYPE_STRING, &can_string,
                            G_TYPE_INVALID);

  if (ret == FALSE)
    return FALSE;

  /* If yes or challenge then we can sleep, it just might take a password */
  if (g_strcmp0 (can_string, "yes") == 0 || g_strcmp0 (can_string, "challenge") == 0)
    {
      *can_method = TRUE;
      *auth_method = TRUE;
    }
  else
    {
      *can_method = FALSE;
      *auth_method = FALSE;
    }

  return TRUE;
}



static gboolean
xfsm_consolekit_try_method (XfsmConsolekit  *consolekit,
                            const gchar     *method,
                            GError         **error)
{
  if (!xfsm_consolekit_proxy_ensure (consolekit, error))
    return FALSE;

  return dbus_g_proxy_call (consolekit->ck_proxy, method, error,
                            G_TYPE_INVALID, G_TYPE_INVALID);
}



static gboolean
xfsm_consolekit_try_sleep (XfsmConsolekit  *consolekit,
                           const gchar     *method,
                           GError         **error)
{
  if (!xfsm_consolekit_proxy_ensure (consolekit, error))
    return FALSE;

  return dbus_g_proxy_call (consolekit->ck_proxy, method, error,
                            G_TYPE_BOOLEAN, TRUE,
                            G_TYPE_INVALID, G_TYPE_INVALID);
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

  return xfsm_consolekit_try_method (consolekit, "Restart", error);
}



gboolean
xfsm_consolekit_try_shutdown (XfsmConsolekit  *consolekit,
                              GError         **error)
{
  g_return_val_if_fail (XFSM_IS_CONSOLEKIT (consolekit), FALSE);

  return xfsm_consolekit_try_method (consolekit, "Stop", error);
}



gboolean
xfsm_consolekit_can_restart (XfsmConsolekit  *consolekit,
                             gboolean        *can_restart,
                             GError         **error)
{
  g_return_val_if_fail (XFSM_IS_CONSOLEKIT (consolekit), FALSE);


  return xfsm_consolekit_can_method (consolekit, "CanRestart",
                                     can_restart, error);
}



gboolean
xfsm_consolekit_can_shutdown (XfsmConsolekit  *consolekit,
                              gboolean        *can_shutdown,
                              GError         **error)
{
  g_return_val_if_fail (XFSM_IS_CONSOLEKIT (consolekit), FALSE);

  return xfsm_consolekit_can_method (consolekit, "CanStop",
                                     can_shutdown, error);
}


static gboolean
lock_screen (GError **error)
{
  XfconfChannel *channel;
  gboolean       ret = TRUE;

  channel = xfsm_open_config ();
  if (xfconf_channel_get_bool (channel, "/shutdown/LockScreen", FALSE))
      ret = g_spawn_command_line_async ("xflock4", error);

  return ret;
}

gboolean
xfsm_consolekit_try_suspend (XfsmConsolekit  *consolekit,
                             GError         **error)
{
  gboolean can_suspend, auth_suspend;

  g_return_val_if_fail (XFSM_IS_CONSOLEKIT (consolekit), FALSE);

  /* Check if consolekit can suspend before we call lock screen. */
  if (xfsm_consolekit_can_suspend (consolekit, &can_suspend, &auth_suspend, NULL))
    {
      if (!can_suspend)
        return FALSE;
    }
  else
    {
      return FALSE;
    }

  if (!lock_screen (error))
    return FALSE;

  return xfsm_consolekit_try_sleep (consolekit, "Suspend", error);
}



gboolean
xfsm_consolekit_try_hibernate (XfsmConsolekit  *consolekit,
                               GError         **error)
{
  gboolean can_hibernate, auth_hibernate;

  g_return_val_if_fail (XFSM_IS_CONSOLEKIT (consolekit), FALSE);

  /* Check if consolekit can hibernate before we call lock screen. */
  if (xfsm_consolekit_can_hibernate (consolekit, &can_hibernate, &auth_hibernate, NULL))
    {
      if (!can_hibernate)
        return FALSE;
    }
  else
    {
      return FALSE;
    }

  if (!lock_screen (error))
    return FALSE;

  return xfsm_consolekit_try_sleep (consolekit, "Hibernate", error);
}



gboolean
xfsm_consolekit_can_suspend (XfsmConsolekit  *consolekit,
                             gboolean        *can_suspend,
                             gboolean        *auth_suspend,
                             GError         **error)
{
  g_return_val_if_fail (XFSM_IS_CONSOLEKIT (consolekit), FALSE);

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

  return xfsm_consolekit_can_sleep (consolekit, "CanHibernate",
                                    can_hibernate, auth_hibernate, error);
}
