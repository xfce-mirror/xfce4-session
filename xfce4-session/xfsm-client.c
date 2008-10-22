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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfsm/xfsm-util.h>

#include "xfsm-client.h"
#include "xfsm-manager.h"
#include "xfsm-global.h"
#include "xfsm-marshal.h"
#include "xfsm-error.h"

#define XFSM_CLIENT_OBJECT_PATH_PREFIX  "/org/xfce/SessionClients/"

struct _XfsmClient
{
  GObject parent;

  XfsmManager    *manager;

  gchar          *id;
  gchar          *object_path;

  XfsmClientState state;
  XfsmProperties *properties;
  SmsConn         sms_conn;
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


static void xfsm_client_class_init (XfsmClientClass *klass);
static void xfsm_client_init (XfsmClient *client);
static void xfsm_client_finalize (GObject *obj);

static void    xfsm_properties_replace_discard_command (XfsmProperties *properties,
                                                        gchar         **new_discard);
static void    xfsm_client_dbus_class_init (XfsmClientClass *klass);
static void    xfsm_client_dbus_init (XfsmClient *client);


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

  if (client->properties != NULL)
    xfsm_properties_free (client->properties);

  g_free (client->id);
  g_free (client->object_path);

  G_OBJECT_CLASS (xfsm_client_parent_class)->finalize (obj);
}





static void
xfsm_properties_replace_discard_command (XfsmProperties *properties,
                                         gchar         **new_discard)
{
  gchar **old_discard = properties->discard_command;

  if (old_discard != NULL)
    {
      if (!xfsm_strv_equal (old_discard, new_discard))
        {
          xfsm_verbose ("Client Id = %s, running old discard command.\n\n",
                        properties->client_id);

          g_spawn_sync (properties->current_directory,
                        old_discard,
                        properties->environment,
                        G_SPAWN_SEARCH_PATH,
                        NULL, NULL,
                        NULL, NULL,
                        NULL, NULL);
        }

      g_strfreev (old_discard);
    }

  properties->discard_command = new_discard;
}


static void
xfsm_client_signal_prop_change (XfsmClient *client,
                                const gchar *name)
{
  GValue          val = { 0, };
  XfsmProperties *properties = client->properties;

  if (strcmp (name, SmCloneCommand) == 0)
    {
      g_value_init (&val, G_TYPE_STRV);
      g_value_set_boxed (&val, properties->clone_command);
    }
  else if (strcmp (name, SmCurrentDirectory) == 0)
    {
      g_value_init (&val, G_TYPE_STRING);
      g_value_set_string (&val, properties->current_directory);
    }
  else if (strcmp (name, SmDiscardCommand) == 0)
    {
      g_value_init (&val, G_TYPE_STRV);
      g_value_set_boxed (&val, properties->discard_command);
    }
  else if (strcmp (name, SmEnvironment) == 0)
    {
      g_value_init (&val, G_TYPE_STRV);
      g_value_set_boxed (&val, properties->environment);
    }
  else if (strcmp (name, SmProcessID) == 0)
    {
      g_value_init (&val, G_TYPE_STRING);
      g_value_set_string (&val, properties->process_id);
    }
  else if (strcmp (name, SmProgram) == 0)
    {
      g_value_init (&val, G_TYPE_STRING);
      g_value_set_string (&val, properties->program);
    }
  else if (strcmp (name, SmRestartCommand) == 0)
    {
      g_value_init (&val, G_TYPE_STRV);
      g_value_set_boxed (&val, properties->restart_command);
    }
  else if (strcmp (name, SmResignCommand) == 0)
    {
      g_value_init (&val, G_TYPE_STRV);
      g_value_set_boxed (&val, properties->resign_command);
    }
  else if (strcmp (name, SmRestartStyleHint) == 0)
    {
      g_value_init (&val, G_TYPE_UCHAR);
      g_value_set_uchar (&val, properties->restart_style_hint);
    }
  else if (strcmp (name, SmShutdownCommand) == 0)
    {
      g_value_init (&val, G_TYPE_STRV);
      g_value_set_boxed (&val, properties->shutdown_command);
    }
  else if (strcmp (name, SmUserID) == 0)
    {
      g_value_init (&val, G_TYPE_STRING);
      g_value_set_string (&val, properties->user_id);
    }
  else if (strcmp (name, GsmPriority) == 0)
    {
      g_value_init (&val, G_TYPE_UCHAR);
      g_value_set_uchar (&val, properties->priority);
    }
  else
    {
      xfsm_verbose ("Client Id = %s, unhandled property change %s\n",
                    client->id, name);
      return;
    }

    g_signal_emit (client, signals[SIG_SM_PROPERTY_CHANGED], 0, name, &val);
    g_value_unset (&val);
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


G_CONST_RETURN gchar *
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
  gchar         **strv;
  SmProp         *prop;
  guint           n;

  g_return_if_fail (XFSM_IS_CLIENT (client));
  g_return_if_fail (client->properties != NULL);

  properties = client->properties;
  
  for (n = 0; n < num_props; ++n)
    {
      prop = props[n];

      if (strcmp (prop->name, SmCloneCommand) == 0)
        {
          strv = xfsm_strv_from_smprop (prop);
          
          if (strv != NULL)
            {
              if (properties->clone_command != NULL)
                g_strfreev (properties->clone_command);
              properties->clone_command = strv;
              xfsm_client_signal_prop_change (client, SmCloneCommand);
            }
          else
            {
              g_warning ("Client %s specified property %s of invalid "
                         "type %s, ignoring.",
                         properties->client_id,
                         prop->name,
                         prop->type);
            }
        }
      else if (strcmp (prop->name, SmCurrentDirectory) == 0)
        {
          if (properties->current_directory != NULL)
            g_free (properties->current_directory);
          properties->current_directory = g_strdup ((const gchar *) prop->vals->value);
          xfsm_client_signal_prop_change (client, SmCurrentDirectory);
        }
      else if (strcmp (prop->name, SmDiscardCommand) == 0)
        {
          strv = xfsm_strv_from_smprop (prop);
          
          if (strv != NULL)
            {
              xfsm_properties_replace_discard_command (properties, strv);
              xfsm_client_signal_prop_change (client, SmDiscardCommand);
            }
          else
            {
              g_warning ("Client %s specified property %s of invalid "
                         "type %s, ignoring.",
                         properties->client_id,
                         prop->name,
                         prop->type);
            }
        }
      else if (strcmp (prop->name, SmEnvironment) == 0)
        {
          strv = xfsm_strv_from_smprop (prop);

          if (strv != NULL)
            {
              if (properties->environment != NULL)
                g_strfreev (properties->environment);
              properties->environment = strv;
              xfsm_client_signal_prop_change (client, SmEnvironment);
            }
          else
            {
              g_warning ("Client %s specified property %s of invalid "
                         "type %s, ignoring.",
                         properties->client_id,
                         prop->name,
                         prop->type);
            }
        }
      else if (strcmp (prop->name, GsmPriority) == 0)
        {
          if (strcmp (prop->type, SmCARD8) == 0)
            {
              properties->priority = *((gint8 *) prop->vals->value);
              xfsm_client_signal_prop_change (client, GsmPriority);
            }
          else
            {
              g_warning ("Client %s specified property %s of invalid "
                         "type %s, ignoring.",
                         properties->client_id,
                         prop->name,
                         prop->type);
            }
        }
      else if (strcmp (prop->name, SmProcessID) == 0)
        {
          if (strcmp (prop->type, SmARRAY8) == 0)
            {
              if (properties->process_id != NULL)
                g_free (properties->process_id);
              properties->process_id = g_strdup ((const gchar *) prop->vals->value);
              xfsm_client_signal_prop_change (client, SmProcessID);
            }
          else
            {
              g_warning ("Client %s specified property %s of invalid "
                         "type %s, ignoring.",
                         properties->client_id,
                         prop->name,
                         prop->type);
            }
        }
      else if (strcmp (prop->name, SmProgram) == 0)
        {
          if (strcmp (prop->type, SmARRAY8) == 0)
            {
              if (properties->program != NULL)
                g_free (properties->program);

              /* work-around damn f*cking xmms */
              if (properties->restart_command != NULL
                  && g_str_has_suffix (properties->restart_command[0], "xmms"))
                {
                  properties->program = g_strdup ("xmms");
                }
              else
                {
                  properties->program = g_strdup ((const gchar *) prop->vals->value);
                }

              xfsm_client_signal_prop_change (client, SmProgram);
            }
          else
            {
              g_warning ("Client %s specified property %s of invalid "
                         "type %s, ignoring.",
                         properties->client_id,
                         prop->name,
                         prop->type);
            }
        }
      else if (strcmp (prop->name, SmRestartCommand) == 0)
        {
          strv = xfsm_strv_from_smprop (prop);
          
          if (strv != NULL)
            {
              if (properties->restart_command != NULL)
                g_strfreev (properties->restart_command);
              properties->restart_command = strv;
              xfsm_client_signal_prop_change (client, SmRestartCommand);

              /* work-around damn f*cking xmms */
              if (g_str_has_suffix (strv[0], "xmms"))
                {
                  if (properties->program != NULL)
                    g_free (properties->program);
                  properties->program = g_strdup ("xmms");
                  xfsm_client_signal_prop_change (client, SmProgram);
                }
            }
          else
            {
              g_warning ("Client %s specified property %s of invalid "
                         "type %s, ignoring.",
                         properties->client_id,
                         prop->name,
                         prop->type);
            }
        }
      else if (strcmp (prop->name, SmRestartStyleHint) == 0)
        {
          if (strcmp (prop->type, SmCARD8) == 0)
            {
              properties->restart_style_hint = *((gint8 *) prop->vals->value);
              xfsm_client_signal_prop_change (client, SmRestartStyleHint);
            }
          else
            {
              g_warning ("Client %s specified property %s of invalid "
                         "type %s, ignoring.",
                         properties->client_id,
                         prop->name,
                         prop->type);
            }
        }
      else if (strcmp (prop->name, SmUserID) == 0)
        {
          if (strcmp (prop->type, SmARRAY8) == 0)
            {
              if (properties->user_id != NULL)
                g_free (properties->user_id);
              properties->user_id = g_strdup ((const gchar *) prop->vals->value);
              xfsm_client_signal_prop_change (client, SmUserID);
            }
          else
            {
              g_warning ("Client %s specified property %s of invalid "
                         "type %s, ignoring.",
                         properties->client_id,
                         prop->name,
                         prop->type);
            }
        }
    }
}


void
xfsm_client_delete_properties (XfsmClient *client,
                               gchar     **prop_names,
                               gint        num_props)
{
  XfsmProperties *properties;
  gint            n;
  const gchar    *name_signal = NULL;

  g_return_if_fail (XFSM_IS_CLIENT (client));
  g_return_if_fail (client->properties != NULL);

  properties = client->properties;
  
  for (n = 0; n < num_props; ++n)
    {
      if (strcmp (prop_names[n], SmCloneCommand) == 0)
        {
          if (properties->clone_command != NULL)
            {
              g_strfreev (properties->clone_command);
              properties->clone_command = NULL;
              name_signal = prop_names[n];
            }
        }
      else if (strcmp (prop_names[n], SmCurrentDirectory) == 0)
        {
          if (properties->current_directory != NULL)
            {
              g_free (properties->current_directory);
              properties->current_directory = NULL;
              name_signal = prop_names[n];
            }
        }
      else if (strcmp (prop_names[n], SmDiscardCommand) == 0)
        {
          if (properties->discard_command != NULL)
            {
              g_strfreev (properties->discard_command);
              properties->discard_command = NULL;
              name_signal = prop_names[n];
            }
        }
      else if (strcmp (prop_names[n], SmEnvironment) == 0)
        {
          if (properties->environment != NULL)
            {
              g_strfreev (properties->environment);
              properties->environment = NULL;
              name_signal = prop_names[n];
            }
        }
      else if (strcmp (prop_names[n], GsmPriority) == 0)
        {
          if (properties->priority != 50)
            {
              properties->priority = 50;
              xfsm_client_signal_prop_change (client, GsmPriority);
            }
        }
      else if (strcmp (prop_names[n], SmRestartStyleHint) == 0)
        {
          if (properties->restart_style_hint != SmRestartIfRunning)
            {
              properties->restart_style_hint = SmRestartIfRunning;
              xfsm_client_signal_prop_change (client, SmRestartStyleHint);
            }
        }
      else if (strcmp (prop_names[n], SmUserID) == 0)
        {
          if (properties->user_id != NULL)
            {
              g_free (properties->user_id);
              properties->user_id = NULL;
              name_signal = prop_names[n];
            }
        }
    }

    if (name_signal != NULL)
      g_signal_emit (client, signals[SIG_SM_PROPERTY_DELETED], 0, name_signal);
}


G_CONST_RETURN gchar *
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
#include "xfsm-client-dbus.h"


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
  DBusGConnection *dbus_conn = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

  if (G_UNLIKELY(!dbus_conn))
    {
      g_critical ("Unable to contact D-Bus session bus: %s", error ? error->message : "Unknown error");
      if (error)
        g_error_free (error);
      return;
    }

  dbus_g_connection_register_g_object (dbus_conn, client->object_path,
                                       G_OBJECT (client));
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
                                           NULL,
                                           (GDestroyNotify) xfsm_g_value_free);

  g_hash_table_insert (*OUT_properties, SmCloneCommand,
                       xfsm_g_value_from_property (properties, SmCloneCommand));

  g_hash_table_insert (*OUT_properties, SmCurrentDirectory,
                       xfsm_g_value_from_property (properties, SmCurrentDirectory));

  g_hash_table_insert (*OUT_properties, SmDiscardCommand,
                       xfsm_g_value_from_property (properties, SmDiscardCommand));

  g_hash_table_insert (*OUT_properties, SmEnvironment,
                       xfsm_g_value_from_property (properties, SmEnvironment));

  g_hash_table_insert (*OUT_properties, SmProcessID,
                       xfsm_g_value_from_property (properties, SmProcessID));

  g_hash_table_insert (*OUT_properties, SmProgram,
                       xfsm_g_value_from_property (properties, SmProgram));

  g_hash_table_insert (*OUT_properties, SmRestartCommand,
                       xfsm_g_value_from_property (properties, SmRestartCommand));

  g_hash_table_insert (*OUT_properties, SmResignCommand,
                       xfsm_g_value_from_property (properties, SmResignCommand));
  
  g_hash_table_insert (*OUT_properties, SmRestartStyleHint,
                       xfsm_g_value_from_property (properties, SmRestartStyleHint));

  g_hash_table_insert (*OUT_properties, SmShutdownCommand,
                       xfsm_g_value_from_property (properties, SmShutdownCommand));

  g_hash_table_insert (*OUT_properties, SmUserID,
                       xfsm_g_value_from_property (properties, SmUserID));

  g_hash_table_insert (*OUT_properties, GsmPriority,
                       xfsm_g_value_from_property (properties, GsmPriority));

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
                                           NULL,
                                           (GDestroyNotify) xfsm_g_value_free);

  for (i = 0; names[i]; ++i)
    {
      GValue *value = xfsm_g_value_from_property (properties, names[i]);
      if (G_LIKELY (value))
        g_hash_table_insert (*OUT_properties, names[i], value);
    }

  return TRUE;
}


/* this is a "lightweight" version of xfsm_properties_extract().  it
 * uses glib functions to allocate memory, and doesn't allocate where
 * it doesn't need to (it assumes all strings will last a while.  for
 * these reasons, you can't use SmFreeProperty() on the results.
 */
static void
xfsm_convert_sm_properties_ht (gpointer key,
                               gpointer value,
                               gpointer user_data)
{
  HtToPropsData *pdata = user_data;
  gchar         *name  = key;
  GValue        *val   = value;
  gint           n     = pdata->count;

  if (strcmp (name, SmCloneCommand) == 0
      || strcmp (name, SmDiscardCommand) == 0
      || strcmp (name, SmEnvironment) == 0
      || strcmp (name, SmRestartCommand) == 0
      || strcmp (name, SmResignCommand) == 0
      || strcmp (name, SmShutdownCommand) == 0)
    {
      gchar **val_strv = g_value_get_boxed (val);
      gint i;

      if (G_UNLIKELY (val_strv == NULL))
        return;

      pdata->props[n].name = name;
      pdata->props[n].type = SmLISTofARRAY8;
      pdata->props[n].num_vals = g_strv_length (val_strv);
      pdata->props[n].vals = g_new0 (SmPropValue, pdata->props[n].num_vals);
      for (i = 0; i < pdata->props[n].num_vals; ++i)
        {
          pdata->props[n].vals[i].length = strlen (val_strv[i]);
          pdata->props[n].vals[i].value = val_strv[i];
        }
    }
  else if (strcmp (name, SmCurrentDirectory) == 0
           || strcmp (name, SmProcessID) == 0
           || strcmp (name, SmProgram) == 0
           || strcmp (name, SmUserID) == 0)
    {
      gchar *val_str = (gchar *) g_value_get_string (val);

      if (G_UNLIKELY (val_str == NULL))
        return;

      pdata->props[n].name = name;
      pdata->props[n].type = SmARRAY8;
      pdata->props[n].num_vals = 1;
      pdata->props[n].vals = g_new0 (SmPropValue, 1);
      pdata->props[n].vals[0].length = strlen (val_str);
      pdata->props[n].vals[0].value = val_str;
    }
  else if (strcmp (name, SmRestartStyleHint) == 0
           || strcmp (name, GsmPriority) == 0)
    {
      guint val_uchar = g_value_get_uchar (val);

      pdata->props[n].name = name;
      pdata->props[n].type = SmCARD8;
      pdata->props[n].num_vals = 1;
      pdata->props[n].vals = g_new0 (SmPropValue, 1);
      pdata->props[n].vals[0].length = 1;
      pdata->props[n].vals[0].value = g_new0 (guchar, 1);
      *(guchar *)(pdata->props[n].vals[0].value) = val_uchar;
    }
  else
    {
      g_warning ("Unhandled property \"%s\"", name);
      return;
    }

  ++pdata->count;
}


static gboolean
xfsm_client_dbus_set_sm_properties (XfsmClient *client,
                                    GHashTable *properties,
                                    GError    **error)
{
  HtToPropsData   pdata;
  gint            n_props, i;

  if (G_UNLIKELY (client->properties == NULL))
    {
      g_set_error (error, XFSM_ERROR, XFSM_ERROR_BAD_VALUE,
                   _("The client doesn't have any properties set yet"));
      return FALSE;
    }

  n_props = g_hash_table_size (properties);
  pdata.props = g_new0 (SmProp, n_props);
  pdata.count = 0;

  g_hash_table_foreach (properties, xfsm_convert_sm_properties_ht, &pdata);
  xfsm_client_merge_properties (client, &pdata.props, pdata.count);

  for (i = 0; i < pdata.count; ++i)
    {
      if (strcmp (pdata.props[i].type, SmCARD8) == 0)
        g_free (pdata.props[i].vals[0].value);
      g_free (pdata.props[i].vals);
    }
  g_free (pdata.props);

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
