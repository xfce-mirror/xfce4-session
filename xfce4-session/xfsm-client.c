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

#include <dbus/dbus-glib.h>

#include <libxfsm/xfsm-util.h>

#include <xfce4-session/xfsm-client.h>
#include <xfce4-session/xfsm-manager.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-marshal.h>
#include <xfce4-session/xfsm-error.h>

#define XFSM_CLIENT_OBJECT_PATH_PREFIX  "/org/xfce/SessionClients/"

struct _XfsmClient
{
  GObject parent;

  XfsmManager     *manager;

  gchar           *id;
  gchar           *object_path;

  XfsmClientState  state;
  XfsmProperties  *properties;
  SmsConn          sms_conn;

  DBusGConnection *dbus_conn;
};

typedef struct _XfsmClientClass
{
  GObjectClass parent;

  /*< signals >*/
  void (*state_changed) (XfsmClient     *client,
                         XfsmClientState old_state,
                         XfsmClientState new_state);

  void (*sm_property_changed) (XfsmClient   *client,
                               const gchar  *name,
                               const GValue *value);

  void (*sm_property_deleted) (XfsmClient  *client,
                               const gchar *name);
} XfsmClientClass;

typedef struct
{
  SmProp *props;
  gint    count;
} HtToPropsData;

enum
{
  SIG_STATE_CHANGED = 0,
  SIG_SM_PROPERTY_CHANGED,
  SIG_SM_PROPERTY_DELETED,
  N_SIGS
};


static void xfsm_client_finalize (GObject *obj);

static void    xfsm_properties_discard_command_changed (XfsmProperties *properties,
                                                        gchar         **old_discard);
static void    xfsm_client_dbus_class_init (XfsmClientClass *klass);
static void    xfsm_client_dbus_init (XfsmClient *client);
static void    xfsm_client_dbus_cleanup (XfsmClient *client);


static guint signals[N_SIGS] = { 0, };


G_DEFINE_TYPE(XfsmClient, xfsm_client, G_TYPE_OBJECT)


static void
xfsm_client_class_init (XfsmClientClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = xfsm_client_finalize;

  signals[SIG_STATE_CHANGED] = g_signal_new ("state-changed",
                                             XFSM_TYPE_CLIENT,
                                             G_SIGNAL_RUN_LAST,
                                             G_STRUCT_OFFSET (XfsmClientClass,
                                                              state_changed),
                                             NULL, NULL,
                                             xfsm_marshal_VOID__UINT_UINT,
                                             G_TYPE_NONE, 2,
                                             G_TYPE_UINT, G_TYPE_UINT);

  signals[SIG_SM_PROPERTY_CHANGED] = g_signal_new ("sm-property-changed",
                                                   XFSM_TYPE_CLIENT,
                                                   G_SIGNAL_RUN_LAST,
                                                   G_STRUCT_OFFSET (XfsmClientClass,
                                                                    sm_property_changed),
                                                   NULL, NULL,
                                                   xfsm_marshal_VOID__STRING_BOXED,
                                                   G_TYPE_NONE, 2,
                                                   G_TYPE_STRING, G_TYPE_VALUE);

  signals[SIG_SM_PROPERTY_DELETED] = g_signal_new ("sm-property-deleted",
                                                   XFSM_TYPE_CLIENT,
                                                   G_SIGNAL_RUN_LAST,
                                                   G_STRUCT_OFFSET (XfsmClientClass,
                                                                    sm_property_deleted),
                                                   NULL, NULL,
                                                   g_cclosure_marshal_VOID__STRING,
                                                   G_TYPE_NONE, 1,
                                                   G_TYPE_STRING);

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
  XfsmProperties *properties = client->properties;

  value = xfsm_properties_get (properties, name);
  if (value)
    {
      g_signal_emit (client, signals[SIG_SM_PROPERTY_CHANGED], 0,
                     name, value);
    }
}



XfsmClient*
xfsm_client_new (XfsmManager *manager,
                 SmsConn      sms_conn)
{
  XfsmClient *client;

  g_return_val_if_fail (sms_conn, NULL);

  client = g_object_new (XFSM_TYPE_CLIENT, NULL);

  client->manager = manager;
  client->sms_conn = sms_conn;
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
      g_signal_emit (client, signals[SIG_STATE_CHANGED], 0, old_state, state);
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
          g_signal_emit (client, signals[SIG_SM_PROPERTY_DELETED], 0,
                         prop_names[n]);
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

static gboolean xfsm_client_dbus_get_id (XfsmClient *client,
                                         gchar     **OUT_id,
                                         GError    **error);
static gboolean xfsm_client_dbus_get_state (XfsmClient *client,
                                            guint      *OUT_state,
                                            GError    **error);
static gboolean xfsm_client_dbus_get_all_sm_properties (XfsmClient  *client,
                                                        GHashTable **OUT_properties,
                                                         GError    **error);
static gboolean xfsm_client_dbus_get_sm_properties (XfsmClient  *client,
                                                    gchar      **names,
                                                    GHashTable **OUT_values,
                                                    GError     **error);
static gboolean xfsm_client_dbus_set_sm_properties (XfsmClient *client,
                                                    GHashTable *properties,
                                                    GError    **error);
static gboolean xfsm_client_dbus_delete_sm_properties (XfsmClient *client,
                                                       gchar     **names,
                                                       GError    **error);
static gboolean xfsm_client_dbus_terminate (XfsmClient *client,
                                            GError    **error);


/* header needs the above fwd decls */
#include <xfce4-session/xfsm-client-dbus.h>


static void
xfsm_client_dbus_class_init (XfsmClientClass *klass)
{
  dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (klass),
                                   &dbus_glib_xfsm_client_object_info);
}


static void
xfsm_client_dbus_init (XfsmClient *client)
{
  GError *error = NULL;

  client->dbus_conn = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

  if (G_UNLIKELY(!client->dbus_conn))
    {
      g_critical ("Unable to contact D-Bus session bus: %s", error ? error->message : "Unknown error");
      if (error)
        g_error_free (error);
      return;
    }

  dbus_g_connection_register_g_object (client->dbus_conn, client->object_path,
                                       G_OBJECT (client));
}


static void
xfsm_client_dbus_cleanup (XfsmClient *client)
{
  if (G_LIKELY (client->dbus_conn))
    {
      dbus_g_connection_unref (client->dbus_conn);
      client->dbus_conn = NULL;
    }
}


static gboolean
xfsm_client_dbus_get_id (XfsmClient *client,
                         gchar     **OUT_id,
                         GError    **error)
{
  *OUT_id = g_strdup (client->id);
  return TRUE;
}


static gboolean
xfsm_client_dbus_get_state (XfsmClient *client,
                            guint      *OUT_state,
                            GError    **error)
{
  *OUT_state = client->state;
  return TRUE;
}


static gboolean
xfsm_client_properties_tree_foreach (gpointer key,
                                     gpointer value,
                                     gpointer data)
{
  gchar       *prop_name = key;
  GValue      *prop_value = value;
  GHashTable  *hash_table = data;

  g_hash_table_insert (hash_table, prop_name, prop_value);

  return FALSE;
}

static gboolean
xfsm_client_dbus_get_all_sm_properties (XfsmClient *client,
                                        GHashTable **OUT_properties,
                                        GError    **error)
{
  XfsmProperties *properties = client->properties;

  if (G_UNLIKELY (properties == NULL))
    {
      g_set_error (error, XFSM_ERROR, XFSM_ERROR_BAD_VALUE,
                   _("The client doesn't have any properties set yet"));
      return FALSE;
    }

  *OUT_properties = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           NULL, NULL);
  g_tree_foreach (properties->sm_properties,
                  xfsm_client_properties_tree_foreach,
                  *OUT_properties);

  return TRUE;
}


static gboolean
xfsm_client_dbus_get_sm_properties (XfsmClient  *client,
                                    gchar      **names,
                                    GHashTable **OUT_properties,
                                    GError     **error)
{
  XfsmProperties *properties = client->properties;
  gint            i;

  if (G_UNLIKELY (properties == NULL))
    {
      g_set_error (error, XFSM_ERROR, XFSM_ERROR_BAD_VALUE,
                   _("The client doesn't have any properties set yet"));
      return FALSE;
    }

  *OUT_properties = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           NULL, NULL);

  for (i = 0; names[i]; ++i)
    {
      GValue *value = g_tree_lookup (properties->sm_properties, names[i]);
      if (G_LIKELY (value))
        g_hash_table_insert (*OUT_properties, names[i], value);
    }

  return TRUE;
}


static void
xfsm_client_dbus_merge_properties_ht (gpointer key,
                                      gpointer value,
                                      gpointer user_data)
{
  gchar          *prop_name = key;
  GValue         *prop_value = value;
  XfsmProperties *properties = user_data;

  xfsm_properties_set (properties, prop_name, prop_value);
}


static gboolean
xfsm_client_dbus_set_sm_properties (XfsmClient *client,
                                    GHashTable *properties,
                                    GError    **error)
{
  if (G_UNLIKELY (client->properties == NULL))
    {
      g_set_error (error, XFSM_ERROR, XFSM_ERROR_BAD_VALUE,
                   _("The client doesn't have any properties set yet"));
      return FALSE;
    }

  g_hash_table_foreach (properties, xfsm_client_dbus_merge_properties_ht,
                        client->properties);

  return TRUE;
}


static gboolean
xfsm_client_dbus_delete_sm_properties (XfsmClient *client,
                                       gchar     **names,
                                       GError    **error)
{
  if (G_UNLIKELY (client->properties == NULL))
    return TRUE;

  xfsm_client_delete_properties (client, names, g_strv_length (names));

  return TRUE;
}


static gboolean
xfsm_client_dbus_terminate (XfsmClient *client,
                            GError    **error)
{
  return xfsm_manager_terminate_client (client->manager, client, error);
}
