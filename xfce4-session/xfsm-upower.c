/*-
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
#ifdef HAVE_UPOWER
#include <upower.h>
#endif /* HAVE_UPOWER */

#include <libxfsm/xfsm-util.h>
#include <xfce4-session/xfsm-upower.h>



#define UPOWER_NAME      "org.freedesktop.UPower"
#define UPOWER_PATH      "/org/freedesktop/UPower"
#define UPOWER_INTERFACE UPOWER_NAME



static void     xfsm_upower_finalize     (GObject     *object);
static gboolean xfsm_upower_proxy_ensure (XfsmUPower  *upower,
                                          GError     **error);
static void     xfsm_upower_proxy_free   (XfsmUPower  *upower);



struct _XfsmUPowerClass
{
  GObjectClass __parent__;
};

struct _XfsmUPower
{
  GObject __parent__;

  DBusGConnection *dbus_conn;
  DBusGProxy      *upower_proxy;
  DBusGProxy      *props_proxy;
};



G_DEFINE_TYPE (XfsmUPower, xfsm_upower, G_TYPE_OBJECT)



static void
xfsm_upower_class_init (XfsmUPowerClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfsm_upower_finalize;
}



static void
xfsm_upower_init (XfsmUPower *upower)
{
}



static void
xfsm_upower_finalize (GObject *object)
{
  xfsm_upower_proxy_free (XFSM_UPOWER (object));

  (*G_OBJECT_CLASS (xfsm_upower_parent_class)->finalize) (object);
}



static gboolean
xfsm_upower_proxy_ensure (XfsmUPower  *upower,
                          GError     **error)
{
  GError         *err = NULL;
  DBusConnection *connection;

  if (upower->dbus_conn == NULL)
    {
      upower->dbus_conn = dbus_g_bus_get (DBUS_BUS_SYSTEM, &err);
      if (upower->dbus_conn == NULL)
        goto error1;

      connection = dbus_g_connection_get_connection (upower->dbus_conn);
      dbus_connection_set_exit_on_disconnect (connection, FALSE);
    }

  if (upower->upower_proxy == NULL)
    {
      upower->upower_proxy = dbus_g_proxy_new_for_name (upower->dbus_conn,
                                                        UPOWER_NAME,
                                                        UPOWER_PATH,
                                                        UPOWER_INTERFACE);
      if (upower->upower_proxy == NULL)
        {
          g_set_error (&err, DBUS_GERROR, DBUS_GERROR_FAILED,
                       "Failed to get proxy for %s",
                       UPOWER_NAME);
          goto error1;
        }
    }

  if (upower->props_proxy == NULL)
    {
      upower->props_proxy = dbus_g_proxy_new_for_name (upower->dbus_conn,
                                                       UPOWER_NAME,
                                                       UPOWER_PATH,
                                                       DBUS_INTERFACE_PROPERTIES);
      if (upower->props_proxy == NULL)
        {
          g_set_error (&err, DBUS_GERROR, DBUS_GERROR_FAILED,
                       "Failed to get proxy for %s properties",
                       UPOWER_NAME);
          goto error1;
        }
    }

  return TRUE;

  error1:

  g_propagate_error (error, err);
  xfsm_upower_proxy_free (upower);

  return FALSE;
}



static void
xfsm_upower_proxy_free (XfsmUPower *upower)
{
  if (upower->upower_proxy != NULL)
    {
      g_object_unref (G_OBJECT (upower->upower_proxy));
      upower->upower_proxy = NULL;
    }

  if (upower->props_proxy != NULL)
    {
      g_object_unref (G_OBJECT (upower->props_proxy));
      upower->props_proxy = NULL;
    }

  if (upower->dbus_conn != NULL)
    {
      dbus_g_connection_unref (upower->dbus_conn);
      upower->dbus_conn = NULL;
    }
}



static gboolean
xfsm_upower_get_property (XfsmUPower   *upower,
                          const gchar  *prop_name,
                          gboolean     *bool_return,
                          GError      **error)
{
  GValue   val = { 0, };
  gboolean succeed;

  succeed = dbus_g_proxy_call (upower->props_proxy, "Get",
                               error,
                               G_TYPE_STRING, UPOWER_INTERFACE,
                               G_TYPE_STRING, prop_name,
                               G_TYPE_INVALID,
                               G_TYPE_VALUE, &val,
                               G_TYPE_INVALID);

  if (succeed)
    {
      g_assert (G_VALUE_HOLDS_BOOLEAN (&val));
      *bool_return = g_value_get_boolean (&val);
    }
  else
    {
      *bool_return = FALSE;
    }

  return succeed;
}



static gboolean
xfsm_upower_can_method (XfsmUPower   *upower,
                        const gchar  *can_property, /* if the system is capable */
                        gboolean     *can_return,
                        const gchar  *auth_method, /* if the user is allowed */
                        gboolean     *auth_return,
                        GError      **error)
{
  g_return_val_if_fail (can_return != NULL, FALSE);
  g_return_val_if_fail (auth_return != NULL, FALSE);

  /* never return true if something fails */
  *can_return = FALSE;
  *auth_return = FALSE;

  if (!xfsm_upower_proxy_ensure (upower, error))
    return FALSE;

  if (xfsm_upower_get_property (upower, can_property, can_return, error))
    {
      if (*can_return)
        {
          /* check authentication if system is capable of this action */
          return dbus_g_proxy_call (upower->upower_proxy, auth_method,
                                    error, G_TYPE_INVALID,
                                    G_TYPE_BOOLEAN, auth_return,
                                    G_TYPE_INVALID);
        }

      return TRUE;
    }

  return FALSE;
}



static void
xfsm_upower_try_method_cb (DBusGProxy     *proxy,
                           DBusGProxyCall *call,
                           gpointer        user_data)
{
  GError *error = NULL;

  if (!dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID, G_TYPE_INVALID))
    {
      if (g_error_matches (error, DBUS_GERROR, DBUS_GERROR_NO_REPLY))
        g_message ("Reply after suspend/hibernate timed out. Continuing...");
      else
        g_warning ("Failed to suspend the system: %s", error->message);

      g_error_free (error);
    }
}



static gboolean
xfsm_upower_try_method (XfsmUPower   *upower,
                        const gchar  *method,
                        GError      **error)
{
  DBusGProxyCall *call;

  if (!xfsm_upower_proxy_ensure (upower, error))
    return FALSE;

  call = dbus_g_proxy_begin_call (upower->upower_proxy,
                                  method,
                                  xfsm_upower_try_method_cb,
                                  upower,
                                  NULL,
                                  G_TYPE_INVALID,
                                  G_TYPE_INVALID);

  return call != NULL;
}



gboolean
xfsm_upower_lock_screen (XfsmUPower   *upower,
                         const gchar  *sleep_kind,
                         GError      **error)
{
  XfconfChannel *channel;
  gboolean       ret = TRUE;

  g_return_val_if_fail (sleep_kind != NULL, FALSE);

  channel = xfsm_open_config ();
  if (xfconf_channel_get_bool (channel, "/shutdown/LockScreen", FALSE))
    {
      if (xfsm_upower_proxy_ensure (upower, error))
        {
#ifdef HAVE_UPOWER
#if !UP_CHECK_VERSION(0, 99, 0)
          GError        *err = NULL;

          /* tell upower we're going to sleep, this saves some
           * time while we sleep 1 second if xflock4 is spawned */
          ret = dbus_g_proxy_call (upower->upower_proxy,
                                   "AboutToSleep", &err,
                                   G_TYPE_STRING, sleep_kind,
                                   G_TYPE_INVALID, G_TYPE_INVALID);

          /*  we don't abort on this, since it is not so critical */
          if (!ret)
            {
              g_warning ("Couldn't sent that we were about to sleep: %s", err->message);
              g_error_free (err);
            }
#endif /* UP_CHECK_VERSION */
#endif /* HAVE_UPOWER */
        }
      else
        {
          /* proxy failed */
          return FALSE;
        }

      ret = g_spawn_command_line_async ("xflock4", error);
      if (ret)
        {
          /* sleep 1 second so locking has time to startup */
          g_usleep (G_USEC_PER_SEC);
        }
    }

  return ret;
}



XfsmUPower *
xfsm_upower_get (void)
{
  static XfsmUPower *object = NULL;

  if (G_LIKELY (object != NULL))
    {
      g_object_ref (G_OBJECT (object));
    }
  else
    {
      object = g_object_new (XFSM_TYPE_UPOWER, NULL);
      g_object_add_weak_pointer (G_OBJECT (object), (gpointer) &object);
    }

  return object;
}



gboolean
xfsm_upower_try_suspend (XfsmUPower  *upower,
                         GError     **error)
{
  g_return_val_if_fail (XFSM_IS_UPOWER (upower), FALSE);

  if (!xfsm_upower_lock_screen (upower, "suspend", error))
    return FALSE;

  return xfsm_upower_try_method (upower, "Suspend", error);
}



gboolean
xfsm_upower_try_hibernate (XfsmUPower  *upower,
                           GError     **error)
{
  g_return_val_if_fail (XFSM_IS_UPOWER (upower), FALSE);

  if (!xfsm_upower_lock_screen (upower, "hibernate", error))
    return FALSE;

  return xfsm_upower_try_method (upower, "Hibernate", error);
}



gboolean
xfsm_upower_can_suspend (XfsmUPower  *upower,
                         gboolean    *can_suspend,
                         gboolean    *auth_suspend,
                         GError     **error)
{
  g_return_val_if_fail (XFSM_IS_UPOWER (upower), FALSE);

  return xfsm_upower_can_method (upower,
                                 "CanSuspend",
                                 can_suspend,
                                 "SuspendAllowed",
                                 auth_suspend,
                                 error);
}



gboolean
xfsm_upower_can_hibernate (XfsmUPower  *upower,
                           gboolean    *can_hibernate,
                           gboolean    *auth_hibernate,
                           GError     **error)
{
  g_return_val_if_fail (XFSM_IS_UPOWER (upower), FALSE);

  return xfsm_upower_can_method (upower,
                                 "CanHibernate",
                                 can_hibernate,
                                 "HibernateAllowed",
                                 auth_hibernate,
                                 error);
}
