/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
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
#include "xfsm-global.h"
#include "xfsm-marshal.h"

struct _XfsmClient
{
  GObject parent;

  XfsmClientState state;
  gchar          *id;
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

static gchar **property_to_strv (const SmProp *prop)  G_GNUC_PURE;
static void    xfsm_properties_replace_discard_command (XfsmProperties *properties,
                                                        gchar         **new_discard);


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

  G_OBJECT_CLASS (xfsm_client_parent_class)->finalize (obj);
}



static gchar **
property_to_strv (const SmProp *prop)
{
  gchar **strv = NULL;
  gint    strc;
  guint   n;
  
  if (strcmp (prop->type, SmARRAY8) == 0)
    {
      if (!g_shell_parse_argv ((const gchar *) prop->vals->value,
                               &strc, &strv, NULL))
        return NULL;
    }
  else if (strcmp (prop->type, SmLISTofARRAY8) == 0)
    {
      strv = g_new (gchar *, prop->num_vals + 1);
      for (n = 0; n < prop->num_vals; ++n)
        strv[n] = g_strdup ((const gchar *) prop->vals[n].value);
      strv[n] = NULL;
    }

  return strv;
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
#if 0  /* FIXME */
      g_value_init (&val, G_TYPE_STRING);
      g_value_set_string (&val, properties->process_id);
#else
      g_warning ("FIXME: handle SmProcessID prop change");
      return;
#endif
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
#if 0  /* FIXME */
      g_value_init (&val, G_TYPE_STRV);
      g_value_set_boxed (&val, properties->resign_command);
#else
      g_warning ("FIXME: handle SmResignCommand prop change");
      return;
#endif
    }
  else if (strcmp (name, SmRestartStyleHint) == 0)
    {
      g_value_init (&val, G_TYPE_UCHAR);
      g_value_set_uchar (&val, properties->restart_style_hint);
    }
  else if (strcmp (name, SmShutdownCommand) == 0)
    {
#if 0  /* FIXME */
      g_value_init (&val, G_TYPE_STRV);
      g_value_set_boxed (&val, properties->shutdown_command);
#else
      g_warning ("FIXME: handle SmShutdownCommand prop change");
      return;
#endif

    }
  else if (strcmp (name, SmUserID) == 0)
    {
      g_value_init (&val, G_TYPE_STRING);
      g_value_set_string (&val, properties->user_id);
    }
  else if (strcmp (name, GsmPriority) == 0)
    {
      g_value_init (&val, G_TYPE_INT);
      g_value_set_int (&val, properties->priority);
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
xfsm_client_new (SmsConn sms_conn)
{
  XfsmClient *client;

  g_return_val_if_fail (sms_conn, NULL);

  client = g_object_new (XFSM_TYPE_CLIENT, NULL);

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
  XfsmProperties *properties = client->properties;
  gchar         **strv;
  SmProp         *prop;
  guint           n;
  
  for (n = 0; n < num_props; ++n)
    {
      prop = props[n];

      if (strcmp (prop->name, SmCloneCommand) == 0)
        {
          strv = property_to_strv (prop);
          
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
          strv = property_to_strv (prop);
          
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
          strv = property_to_strv (prop);

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
          strv = property_to_strv (prop);
          
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
  XfsmProperties *properties  = client->properties;
  gint            n;
  const gchar    *name_signal = NULL;
  
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
