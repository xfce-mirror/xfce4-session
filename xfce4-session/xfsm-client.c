/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gio/gio.h>

#include <libxfsm/xfsm-util.h>

#include <xfce4-session/xfsm-client.h>
#include <xfce4-session/xfsm-manager.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-marshal.h>
#include <xfce4-session/xfsm-error.h>
#include <xfce4-session/xfsm-client-dbus.h>

#define XFSM_CLIENT_OBJECT_PATH_PREFIX  "/org/xfce/SessionClients/"

struct _XfsmClient
{
  XfsmDbusClientSkeleton parent;

  XfsmManager     *manager;

  gchar           *id;
  gchar           *object_path;

  XfsmClientState  state;
  XfsmProperties  *properties;
  SmsConn          sms_conn;

  GDBusConnection *connection;
};

typedef struct _XfsmClientClass
{
  XfsmDbusClientSkeletonClass parent;
} XfsmClientClass;

typedef struct
{
  SmProp *props;
  gint    count;
} HtToPropsData;



static void xfsm_client_finalize (GObject *obj);

static void    xfsm_properties_discard_command_changed (XfsmProperties *properties,
                                                        gchar         **old_discard);
static void    xfsm_client_dbus_class_init (XfsmClientClass *klass);
static void    xfsm_client_dbus_init (XfsmClient *client);
static void    xfsm_client_iface_init (XfsmDbusClientIface *iface);
static void    xfsm_client_dbus_cleanup (XfsmClient *client);


G_DEFINE_TYPE_WITH_CODE (XfsmClient, xfsm_client, XFSM_DBUS_TYPE_CLIENT_SKELETON, G_IMPLEMENT_INTERFACE (XFSM_DBUS_TYPE_CLIENT, xfsm_client_iface_init));


static void
xfsm_client_class_init (XfsmClientClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = xfsm_client_finalize;

  xfsm_client_dbus_class_init (klass);
}


static void
xfsm_client_init (XfsmClient *client)
{

}

static void
xfsm_client_finalize (GObject *obj)
{
  XfsmClient *client = XFSM_CLIENT (obj);

  xfsm_client_dbus_cleanup (client);

  if (client->properties != NULL)
    xfsm_properties_free (client->properties);

  g_free (client->id);
  g_free (client->object_path);

  G_OBJECT_CLASS (xfsm_client_parent_class)->finalize (obj);
}




static void
xfsm_properties_discard_command_changed (XfsmProperties *properties,
                                         gchar         **old_discard)
{
  gchar **new_discard;

  g_return_if_fail (properties != NULL);
  g_return_if_fail (old_discard != NULL);

  new_discard = xfsm_properties_get_strv (properties, SmDiscardCommand);

  if (!xfsm_strv_equal (old_discard, new_discard))
    {
      xfsm_verbose ("Client Id = %s, running old discard command.\n\n",
                    properties->client_id);

      g_spawn_sync (xfsm_properties_get_string(properties, SmCurrentDirectory),
                    old_discard,
                    xfsm_properties_get_strv(properties, SmEnvironment),
                    G_SPAWN_SEARCH_PATH,
                    NULL, NULL,
                    NULL, NULL,
                    NULL, NULL);
    }
}


static void
xfsm_client_signal_prop_change (XfsmClient *client,
                                const gchar *name)
{
  const GValue   *value;
  GVariant       *variant = NULL;
  XfsmProperties *properties = client->properties;

  value = xfsm_properties_get (properties, name);
  if (value)
    {
      /* convert the gvalue to gvariant because gdbus requires it */
      if (G_VALUE_HOLDS_STRING (value))
        {
          variant = g_dbus_gvalue_to_gvariant(value, G_VARIANT_TYPE_STRING);
        }
      else if (G_VALUE_HOLDS_UCHAR (value))
        {
          variant = g_dbus_gvalue_to_gvariant(value, G_VARIANT_TYPE ("y"));
        }
      else if (G_VALUE_HOLDS (value, G_TYPE_STRV))
        {
          variant = g_dbus_gvalue_to_gvariant(value, G_VARIANT_TYPE_STRING_ARRAY);
        }
      else
        {
          g_warning ("xfsm_client.c:xfsm_client_signal_prop_change: Value type not supported");
	  return;
        }

//      xfsm_dbus_client_emit_sm_property_changed (XFSM_DBUS_CLIENT (client), name, variant);
      g_variant_unref (variant);
    }
}



XfsmClient*
xfsm_client_new (XfsmManager *manager,
                 SmsConn      sms_conn,
                 GDBusConnection *connection)
{
  XfsmClient *client;

  g_return_val_if_fail (sms_conn, NULL);

  client = g_object_new (XFSM_TYPE_CLIENT, NULL);

  client->manager = manager;
  client->sms_conn = sms_conn;
  client->connection = g_object_ref (connection);
  client->state = XFSM_CLIENT_IDLE;

  return client;
}


void
xfsm_client_set_initial_properties (XfsmClient     *client,
                                    XfsmProperties *properties)
{
  g_return_if_fail (XFSM_IS_CLIENT (client));
  g_return_if_fail (properties != NULL);

  if (client->properties != NULL)
    xfsm_properties_free (client->properties);
  client->properties = properties;

  client->id = g_strdup (properties->client_id);

  g_free (client->object_path);
  client->object_path = g_strconcat (XFSM_CLIENT_OBJECT_PATH_PREFIX,
                                     client->id, NULL);
  g_strcanon (client->object_path + strlen (XFSM_CLIENT_OBJECT_PATH_PREFIX),
              "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_",
              '_');

  xfsm_client_dbus_init (client);
}


XfsmClientState
xfsm_client_get_state (XfsmClient *client)
{
  g_return_val_if_fail (XFSM_IS_CLIENT (client), XFSM_CLIENT_DISCONNECTED);
  return client->state;
}


void
xfsm_client_set_state (XfsmClient     *client,
                       XfsmClientState state)
{
  g_return_if_fail (XFSM_IS_CLIENT (client));

  if (G_LIKELY (client->state != state))
    {
      XfsmClientState old_state = client->state;
      client->state = state;
      xfsm_dbus_client_emit_state_changed (XFSM_DBUS_CLIENT (client), old_state, state);
    }
}


const gchar *
xfsm_client_get_id (XfsmClient *client)
{
  g_return_val_if_fail (XFSM_IS_CLIENT (client), NULL);
  return client->id;
}


SmsConn
xfsm_client_get_sms_connection (XfsmClient *client)
{
  g_return_val_if_fail (XFSM_IS_CLIENT (client), NULL);
  return client->sms_conn;
}


XfsmProperties *
xfsm_client_get_properties (XfsmClient *client)
{
  g_return_val_if_fail (XFSM_IS_CLIENT (client), NULL);
  return client->properties;
}


XfsmProperties *
xfsm_client_steal_properties (XfsmClient *client)
{
  XfsmProperties *properties;

  g_return_val_if_fail(XFSM_IS_CLIENT (client), NULL);

  properties = client->properties;
  client->properties = NULL;

  return properties;
}


void
xfsm_client_merge_properties (XfsmClient *client,
                              SmProp    **props,
                              gint        num_props)
{
  XfsmProperties *properties;
  SmProp         *prop;
  gint            n;

  g_return_if_fail (XFSM_IS_CLIENT (client));
  g_return_if_fail (client->properties != NULL);

  properties = client->properties;

  for (n = 0; n < num_props; ++n)
    {
      gchar **old_discard = NULL;

      prop = props[n];

      if (!strcmp (prop->name, SmDiscardCommand))
        {
          old_discard = xfsm_properties_get_strv (properties, SmDiscardCommand);
          if (old_discard)
            old_discard = g_strdupv (old_discard);
        }

      if (xfsm_properties_set_from_smprop (properties, prop))
        {
          if (old_discard)
            xfsm_properties_discard_command_changed (properties, old_discard);

          xfsm_client_signal_prop_change (client, prop->name);
        }

      g_strfreev (old_discard);
    }
}


void
xfsm_client_delete_properties (XfsmClient *client,
                               gchar     **prop_names,
                               gint        num_props)
{
  XfsmProperties *properties;
  gint            n;

  g_return_if_fail (XFSM_IS_CLIENT (client));
  g_return_if_fail (client->properties != NULL);

  properties = client->properties;

  for (n = 0; n < num_props; ++n)
    {
      if (xfsm_properties_remove (properties, prop_names[n]))
        {
          xfsm_dbus_client_emit_sm_property_deleted (XFSM_DBUS_CLIENT (client), prop_names[n]);
        }
    }
}


const gchar *
xfsm_client_get_object_path (XfsmClient *client)
{
  g_return_val_if_fail (XFSM_IS_CLIENT (client), NULL);
  return client->object_path;
}



/*
 * dbus server impl
 */

static gboolean xfsm_client_dbus_get_id (XfsmDbusClient *object,
                                         GDBusMethodInvocation *invocation);
static gboolean xfsm_client_dbus_get_state (XfsmDbusClient *object,
                                            GDBusMethodInvocation *invocation);
static gboolean xfsm_client_dbus_get_all_sm_properties (XfsmDbusClient *object,
                                                        GDBusMethodInvocation *invocation);
static gboolean xfsm_client_dbus_get_sm_properties (XfsmDbusClient *object,
                                                    GDBusMethodInvocation *invocation,
                                                    const gchar *const *arg_names);
static gboolean xfsm_client_dbus_set_sm_properties (XfsmDbusClient *object,
                                                    GDBusMethodInvocation *invocation,
                                                    GVariant *arg_properties);
static gboolean xfsm_client_dbus_delete_sm_properties (XfsmDbusClient *object,
                                                       GDBusMethodInvocation *invocation,
                                                       const gchar *const *arg_names);
static gboolean xfsm_client_dbus_terminate (XfsmDbusClient *object,
                                            GDBusMethodInvocation *invocation);


/* header needs the above fwd decls */
#include <xfce4-session/xfsm-client-dbus.h>


static void
xfsm_client_dbus_class_init (XfsmClientClass *klass)
{
}


static void
xfsm_client_dbus_init (XfsmClient *client)
{
  GError *error = NULL;

  if (G_UNLIKELY(!client->connection))
    {
      g_critical ("Unable to contact D-Bus session bus: %s", error ? error->message : "Unknown error");
      if (error)
        g_clear_error (&error);
      return;
    }

  g_debug ("exporting path %s", client->object_path);

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (XFSM_DBUS_CLIENT (client)),
                                         client->connection,
                                         client->object_path,
                                         &error)) {
    if (error != NULL) {
            g_critical ("error exporting interface: %s", error->message);
            g_clear_error (&error);
            return;
    }
  }

  g_debug ("exported on %s", g_dbus_interface_skeleton_get_object_path (G_DBUS_INTERFACE_SKELETON (XFSM_DBUS_CLIENT (client))));
}

static void
xfsm_client_iface_init (XfsmDbusClientIface *iface)
{
        iface->handle_delete_sm_properties = xfsm_client_dbus_delete_sm_properties;
        iface->handle_get_all_sm_properties = xfsm_client_dbus_get_all_sm_properties;
        iface->handle_get_id = xfsm_client_dbus_get_id;
        iface->handle_get_sm_properties = xfsm_client_dbus_get_sm_properties;
        iface->handle_get_state = xfsm_client_dbus_get_state;
        iface->handle_set_sm_properties = xfsm_client_dbus_set_sm_properties;
        iface->handle_terminate = xfsm_client_dbus_terminate;
}

static void
xfsm_client_dbus_cleanup (XfsmClient *client)
{
  if (G_LIKELY (client->connection))
    {
      g_object_unref (client->connection);
      client->connection = NULL;
    }
}


static gboolean
xfsm_client_dbus_get_id (XfsmDbusClient *object,
                         GDBusMethodInvocation *invocation)
{
  xfsm_dbus_client_complete_get_id (object, invocation, XFSM_CLIENT(object)->id);
  return TRUE;
}


static gboolean
xfsm_client_dbus_get_state (XfsmDbusClient *object,
                            GDBusMethodInvocation *invocation)
{
  xfsm_dbus_client_complete_get_state (object, invocation, XFSM_CLIENT(object)->state);
  return TRUE;
}


static void
builder_add_value (GVariantBuilder *builder,
                   const gchar  *name,
                   const GValue *value)
{
  if (name == NULL)
    {
      g_warning ("xfsm_client.c:builder_add_value: name must not be NULL");
      return;
    }

  if (G_VALUE_HOLDS_STRING (value))
    {
      g_variant_builder_add (builder, "{sv}", name, g_dbus_gvalue_to_gvariant(value, G_VARIANT_TYPE_STRING));
    }
  else if (G_VALUE_HOLDS_UCHAR (value))
    {
      g_variant_builder_add (builder, "{sv}", name, g_dbus_gvalue_to_gvariant(value, G_VARIANT_TYPE ("y")));
    }
  else if (G_VALUE_HOLDS (value, G_TYPE_STRV))
    {
      g_variant_builder_add (builder, "{sv}", name, g_dbus_gvalue_to_gvariant(value, G_VARIANT_TYPE_STRING_ARRAY));
    }
  else
    {
      g_warning ("xfsm_client.c:builder_add_value: Value type not supported");
    }
}


static gboolean
xfsm_client_properties_tree_foreach (gpointer key,
                                     gpointer value,
                                     gpointer data)
{
  gchar  *prop_name = key;
  GValue *prop_value = value;
  GVariantBuilder *out_properties = data;

  builder_add_value (out_properties, prop_name, prop_value);
  return FALSE;
}

static gboolean
xfsm_client_dbus_get_all_sm_properties (XfsmDbusClient *object,
                                        GDBusMethodInvocation *invocation)
{
  XfsmProperties *properties = XFSM_CLIENT(object)->properties;
  GVariantBuilder out_properties;

  if (G_UNLIKELY (properties == NULL))
    {
      throw_error (invocation, XFSM_ERROR_BAD_VALUE, "The client doesn't have any properties set yet");
      return TRUE;
    }

  g_variant_builder_init (&out_properties, G_VARIANT_TYPE ("a{sv}"));

  g_tree_foreach (properties->sm_properties,
                  xfsm_client_properties_tree_foreach,
                  &out_properties);

  xfsm_dbus_client_complete_get_all_sm_properties (object, invocation, g_variant_builder_end (&out_properties));
  return TRUE;
}


static gboolean
xfsm_client_dbus_get_sm_properties (XfsmDbusClient *object,
                                    GDBusMethodInvocation *invocation,
                                    const gchar *const *arg_names)
{
  XfsmProperties *properties = XFSM_CLIENT(object)->properties;
  GVariantBuilder out_properties;
  gint            i;

  if (G_UNLIKELY (properties == NULL))
    {
      throw_error (invocation, XFSM_ERROR_BAD_VALUE, "The client doesn't have any properties set yet");
      return TRUE;
    }

  g_variant_builder_init (&out_properties, G_VARIANT_TYPE ("a{sv}"));

  for (i = 0; arg_names[i]; ++i)
    {
      GValue *value = g_tree_lookup (properties->sm_properties, arg_names[i]);

      builder_add_value (&out_properties, arg_names[i], value);
    }

  xfsm_dbus_client_complete_get_all_sm_properties (object, invocation, g_variant_builder_end (&out_properties));
  return TRUE;
}


static gboolean
xfsm_client_dbus_set_sm_properties (XfsmDbusClient *object,
                                    GDBusMethodInvocation *invocation,
                                    GVariant *arg_properties)
{
  XfsmProperties *properties = XFSM_CLIENT(object)->properties;
  GVariantIter   *iter;
  gchar          *prop_name;
  GVariant       *variant;

  if (G_UNLIKELY (properties == NULL))
    {
      throw_error (invocation, XFSM_ERROR_BAD_VALUE, "The client doesn't have any properties set yet");
      return TRUE;
    }

  g_variant_get (arg_properties, "a(sv)", &iter);

  while (g_variant_iter_next (iter, "(sv)", &prop_name, &variant))
    {
      GValue value;

      g_dbus_gvariant_to_gvalue (variant, &value);
      xfsm_properties_set (properties, prop_name, &value);

      g_variant_unref (variant);
    }

  g_variant_iter_free (iter);
  xfsm_dbus_client_complete_set_sm_properties (object, invocation);
  return TRUE;
}


static gboolean
xfsm_client_dbus_delete_sm_properties (XfsmDbusClient *object,
                                       GDBusMethodInvocation *invocation,
                                       const gchar *const *arg_names)
{
  XfsmProperties *properties = XFSM_CLIENT(object)->properties;
  gchar **names = g_strdupv((gchar**)arg_names);

  if (G_UNLIKELY (properties == NULL))
    {
      throw_error (invocation, XFSM_ERROR_BAD_VALUE, "The client doesn't have any properties set yet");
      return TRUE;
    }

  xfsm_client_delete_properties (XFSM_CLIENT(object), names, g_strv_length (names));

  g_strfreev (names);
  xfsm_dbus_client_complete_delete_sm_properties (object, invocation);
  return TRUE;
}


static gboolean
xfsm_client_dbus_terminate (XfsmDbusClient *object,
                            GDBusMethodInvocation *invocation)
{
  GError *error = NULL;

  xfsm_manager_terminate_client (XFSM_CLIENT(object)->manager, XFSM_CLIENT(object), &error);
  if (error != NULL)
    {
      throw_error (invocation, XFSM_ERROR_BAD_STATE, "Unable to terminate client, error was: %s", error->message);
      g_clear_error (&error);
      return TRUE;
    }

  return TRUE;
}
