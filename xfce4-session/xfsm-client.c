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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>

#include "libxfsm/xfsm-util.h"

#include "xfsm-client-dbus.h"
#include "xfsm-client.h"
#include "xfsm-error.h"
#include "xfsm-global.h"
#include "xfsm-manager.h"
#include "xfsm-marshal.h"

#define XFSM_CLIENT_OBJECT_PATH_PREFIX "/org/xfce/SessionClients/"

struct _XfsmClient
{
  XfsmDbusClientSkeleton parent;

  XfsmManager *manager;

  gchar *id;
  gchar *app_id;
  gchar *object_path;
  gchar *service_name;
  guint quit_timeout;

  XfsmClientState state;
  XfsmProperties *properties;
  SmsConn sms_conn;
  GDBusConnection *connection;
};

typedef struct _XfsmClientClass
{
  XfsmDbusClientSkeletonClass parent;
} XfsmClientClass;



static void
xfsm_client_finalize (GObject *obj);

static void
xfsm_client_dbus_class_init (XfsmClientClass *klass);
static void
xfsm_client_dbus_init (XfsmClient *client);
static void
xfsm_client_iface_init (XfsmDbusClientIface *iface);
static void
xfsm_client_dbus_cleanup (XfsmClient *client);


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

  if (client->quit_timeout != 0)
    g_source_remove (client->quit_timeout);

  g_free (client->id);
  g_free (client->app_id);
  g_free (client->object_path);
  g_free (client->service_name);

  G_OBJECT_CLASS (xfsm_client_parent_class)->finalize (obj);
}



static const gchar *
get_state (XfsmClientState state)
{
  static const gchar *client_state[XFSM_CLIENT_STATE_COUNT] = {
    "XFSM_CLIENT_IDLE",
    "XFSM_CLIENT_INTERACTING",
    "XFSM_CLIENT_SAVEDONE",
    "XFSM_CLIENT_SAVING",
    "XFSM_CLIENT_SAVINGLOCAL",
    "XFSM_CLIENT_WAITFORINTERACT",
    "XFSM_CLIENT_WAITFORPHASE2",
    "XFSM_CLIENT_DISCONNECTED"
  };

  return client_state[state];
}

#ifdef ENABLE_X11
static void
xfsm_properties_discard_command_changed (XfsmProperties *properties,
                                         gchar **old_discard)
{
  gchar **new_discard;

  g_return_if_fail (properties != NULL);
  g_return_if_fail (old_discard != NULL);

  new_discard = xfsm_properties_get_strv (properties, SmDiscardCommand);

  if (!xfsm_strv_equal (old_discard, new_discard))
    {
      xfsm_verbose ("Client Id = %s, running old discard command.\n\n",
                    properties->client_id);

      g_spawn_sync (xfsm_properties_get_string (properties, SmCurrentDirectory),
                    old_discard,
                    xfsm_properties_get_strv (properties, SmEnvironment),
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
  const GValue *value;
  GVariant *variant = NULL;
  XfsmProperties *properties = client->properties;

  value = xfsm_properties_get (properties, name);
  if (value)
    {
      /* convert the gvalue to gvariant because gdbus requires it */
      if (G_VALUE_HOLDS_STRING (value))
        {
          variant = g_dbus_gvalue_to_gvariant (value, G_VARIANT_TYPE_STRING);
        }
      else if (G_VALUE_HOLDS_UCHAR (value))
        {
          variant = g_dbus_gvalue_to_gvariant (value, G_VARIANT_TYPE ("y"));
        }
      else if (G_VALUE_HOLDS (value, G_TYPE_STRV))
        {
          variant = g_dbus_gvalue_to_gvariant (value, G_VARIANT_TYPE_STRING_ARRAY);
        }
      else
        {
          g_warning ("xfsm_client.c:xfsm_client_signal_prop_change: Value type not supported");
          return;
        }

      g_variant_unref (variant);
    }
}
#endif



gchar *
xfsm_client_generate_id (SmsConn sms_conn)
{
  static char *addr = NULL;
  static int sequence = 0;
  char *id = NULL;

#ifdef ENABLE_X11
  if (sms_conn != NULL)
    {
      char *sms_id = SmsGenerateClientID (sms_conn);
      if (sms_id != NULL)
        {
          id = g_strdup (sms_id);
          g_free (sms_id);
        }
    }
#endif

  if (id == NULL)
    {
      if (addr == NULL)
        {
          /*
           * Faking our IP address, the 0 below is "unknown"
           * address format (1 would be IP, 2 would be DEC-NET
           * format). Stolen from KDE :-)
           */
          addr = g_strdup_printf ("0%.8x", g_random_int ());
        }

      id = (char *) g_malloc (50);
      g_snprintf (id, 50, "1%s%.13ld%.10d%.4d", addr,
                  (long) time (NULL), (int) getpid (), sequence);
      sequence = (sequence + 1) % 10000;
    }

  return id;
}


XfsmClient *
xfsm_client_new (XfsmManager *manager,
                 SmsConn sms_conn,
                 GDBusConnection *connection)
{
  XfsmClient *client;

  client = g_object_new (XFSM_TYPE_CLIENT, NULL);

  client->manager = manager;
  client->sms_conn = sms_conn;
  client->connection = g_object_ref (connection);
  client->state = XFSM_CLIENT_IDLE;

  return client;
}


void
xfsm_client_set_initial_properties (XfsmClient *client,
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



static const gchar *
get_client_id (XfsmClient *client)
{
  const gchar *client_id;

  if (client->app_id)
    client_id = client->app_id;
  else
    client_id = client->id;

  return client_id;
}



void
xfsm_client_set_state (XfsmClient *client,
                       XfsmClientState state)
{
  g_return_if_fail (XFSM_IS_CLIENT (client));

  if (G_LIKELY (client->state != state))
    {
      XfsmClientState old_state = client->state;
      client->state = state;
      xfsm_dbus_client_emit_state_changed (XFSM_DBUS_CLIENT (client), old_state, state);

      xfsm_verbose ("%s client state was %s and now is %s\n", get_client_id (client), get_state (old_state), get_state (state));

      /* During a save, we need to ask the client if it's ok to shutdown */
      if (state == XFSM_CLIENT_SAVING && xfsm_manager_get_state (client->manager) == XFSM_MANAGER_SHUTDOWN)
        {
          xfsm_dbus_client_emit_query_end_session (XFSM_DBUS_CLIENT (client), 1);
        }
      else if (state == XFSM_CLIENT_SAVING && xfsm_manager_get_state (client->manager) == XFSM_MANAGER_SHUTDOWNPHASE2)
        {
          xfsm_dbus_client_emit_end_session (XFSM_DBUS_CLIENT (client), 1);
        }
    }
}


const gchar *
xfsm_client_get_id (XfsmClient *client)
{
  g_return_val_if_fail (XFSM_IS_CLIENT (client), NULL);
  return client->id;
}


const gchar *
xfsm_client_get_app_id (XfsmClient *client)
{
  g_return_val_if_fail (XFSM_IS_CLIENT (client), NULL);
  return client->app_id;
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

  g_return_val_if_fail (XFSM_IS_CLIENT (client), NULL);

  properties = client->properties;
  client->properties = NULL;

  return properties;
}


#ifdef ENABLE_X11
void
xfsm_client_merge_properties (XfsmClient *client,
                              SmProp **props,
                              gint num_props)
{
  XfsmProperties *properties;
  SmProp *prop;
  gint n;

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
#endif


void
xfsm_client_delete_properties (XfsmClient *client,
                               gchar **prop_names,
                               gint num_props)
{
  XfsmProperties *properties;
  gint n;

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



void
xfsm_client_set_service_name (XfsmClient *client,
                              const gchar *service_name)
{
  g_free (client->service_name);
  client->service_name = g_strdup (service_name);
}


const gchar *
xfsm_client_get_service_name (XfsmClient *client)
{
  return client->service_name;
}



static void
xfsm_client_save_restart_command (XfsmClient *client)
{
  XfsmProperties *properties = client->properties;
  gchar *input;
  gchar *output = NULL;
  gint exit_status;
  GError *error = NULL;

  input = g_strdup_printf ("ps -p %u -o args=", properties->pid);

  if (g_spawn_command_line_sync (input, &output, NULL, &exit_status, &error))
    {
      gchar **strv = g_new0 (gchar *, 2);

      /* remove the newline at the end of the string */
      output[strcspn (output, "\n")] = 0;

      strv[0] = output;
      strv[1] = NULL;

      xfsm_verbose ("%s restart command %s\n", input, output);
      xfsm_properties_set_strv (properties, "RestartCommand", strv);
    }
  else
    {
      xfsm_verbose ("Failed to get the process command line using the command %s, error was %s\n", input, error->message);
    }

  g_free (input);
}



static void
xfsm_client_save_program_name (XfsmClient *client)
{
  XfsmProperties *properties = client->properties;
  gchar *input;
  gchar *output = NULL;
  gint exit_status;
  GError *error = NULL;

  input = g_strdup_printf ("ps -p %u -o comm=", properties->pid);

  if (g_spawn_command_line_sync (input, &output, NULL, &exit_status, &error))
    {
      /* remove the newline at the end of the string */
      output[strcspn (output, "\n")] = 0;

      xfsm_verbose ("%s program name %s\n", input, output);
      xfsm_properties_set_string (properties, "Program", output);
    }
  else
    {
      xfsm_verbose ("Failed to get the process command line using the command %s, error was %s\n", input, error->message);
    }

  g_free (input);
}



static void
xfsm_client_save_desktop_file (XfsmClient *client)
{
  XfsmProperties *properties = client->properties;
  GDesktopAppInfo *app_info = NULL;
  const gchar *app_id = client->app_id;
  gchar *desktop_file = NULL;

  if (app_id == NULL)
    return;

  /* First attempt to append .desktop to the filename since the desktop file
   * may match the application id. I.e. org.gnome.Devhelp.desktop matches
   * the GApplication org.gnome.Devhelp
   */
  desktop_file = g_strdup_printf ("%s.desktop", app_id);
  xfsm_verbose ("looking for desktop file %s\n", desktop_file);
  app_info = g_desktop_app_info_new (desktop_file);

  if (app_info == NULL || g_desktop_app_info_get_filename (app_info) == NULL)
    {
      gchar *begin;
      g_free (desktop_file);
      desktop_file = NULL;

      /* Find the last '.' and try to load that. This is because the app_id is
       * in the funky org.xfce.parole format and the desktop file may just be
       * parole.desktop */
      begin = g_strrstr (app_id, ".");

      /* maybe it doesn't have dots in the name? */
      if (begin == NULL || begin++ == NULL)
        return;

      desktop_file = g_strdup_printf ("%s.desktop", begin);
      xfsm_verbose ("looking for desktop file %s\n", desktop_file);
      app_info = g_desktop_app_info_new (desktop_file);

      if (app_info == NULL || g_desktop_app_info_get_filename (app_info) == NULL)
        {
          /* Failed to get a desktop file, maybe it doesn't have one */
          xfsm_verbose ("failed to get a desktop file for the client\n");
          g_free (desktop_file);
          return;
        }
    }

  /* if we got here we found a .desktop file, save it */
  xfsm_properties_set_string (properties, GsmDesktopFile, g_desktop_app_info_get_filename (app_info));

  g_free (desktop_file);
}



void
xfsm_client_set_pid (XfsmClient *client,
                     pid_t pid)
{
  XfsmProperties *properties;
  gchar *pid_str;

  g_return_if_fail (XFSM_IS_CLIENT (client));
  g_return_if_fail (client->properties != NULL);

  properties = client->properties;

  /* save the pid */
  properties->pid = pid;

  /* convert it to a string */
  pid_str = g_strdup_printf ("%d", pid);

  /* store the string as well (so we can export it over dbus */
  xfsm_properties_set_string (properties, "ProcessID", pid_str);

  /* save the command line for the process so we can restart it if needed */
  xfsm_client_save_restart_command (client);

  /* save the program name */
  xfsm_client_save_program_name (client);

  g_free (pid_str);
}


void
xfsm_client_set_app_id (XfsmClient *client,
                        const gchar *app_id)
{
  client->app_id = g_strdup (app_id);

  /* save the desktop file */
  xfsm_client_save_desktop_file (client);
}



static gboolean
kill_hung_client (gpointer user_data)
{
  XfsmClient *client = XFSM_CLIENT (user_data);

  client->quit_timeout = 0;

  if (!client->properties)
    return FALSE;

  if (client->properties->pid < 2)
    return FALSE;

  xfsm_verbose ("killing unresponsive client %s\n", get_client_id (client));
  kill (client->properties->pid, SIGKILL);

  return FALSE;
}



void
xfsm_client_terminate (XfsmClient *client)
{
  xfsm_verbose ("emitting stop signal for client %s\n", get_client_id (client));

  /* Ask the client to shutdown gracefully */
  xfsm_dbus_client_emit_stop (XFSM_DBUS_CLIENT (client));

  /* add a timeout so we can forcefully stop the client */
  client->quit_timeout = g_timeout_add_seconds (15, kill_hung_client, client);
}



void
xfsm_client_end_session (XfsmClient *client)
{
  xfsm_verbose ("emitting end session signal for client %s\n", get_client_id (client));

  /* Start the client shutdown */
  xfsm_dbus_client_emit_end_session (XFSM_DBUS_CLIENT (client), 1);
}


void
xfsm_client_cancel_shutdown (XfsmClient *client)
{
  xfsm_verbose ("emitting cancel session signal for client %s\n", get_client_id (client));

  /* Cancel the client shutdown */
  xfsm_dbus_client_emit_cancel_end_session (XFSM_DBUS_CLIENT (client));
}



/*
 * dbus server impl
 */

static gboolean
xfsm_client_dbus_get_id (XfsmDbusClient *object,
                         GDBusMethodInvocation *invocation);
static gboolean
xfsm_client_dbus_get_state (XfsmDbusClient *object,
                            GDBusMethodInvocation *invocation);
static gboolean
xfsm_client_dbus_get_all_sm_properties (XfsmDbusClient *object,
                                        GDBusMethodInvocation *invocation);
static gboolean
xfsm_client_dbus_get_sm_properties (XfsmDbusClient *object,
                                    GDBusMethodInvocation *invocation,
                                    const gchar *const *arg_names);
static gboolean
xfsm_client_dbus_set_sm_properties (XfsmDbusClient *object,
                                    GDBusMethodInvocation *invocation,
                                    GVariant *arg_properties);
static gboolean
xfsm_client_dbus_delete_sm_properties (XfsmDbusClient *object,
                                       GDBusMethodInvocation *invocation,
                                       const gchar *const *arg_names);
static gboolean
xfsm_client_dbus_terminate (XfsmDbusClient *object,
                            GDBusMethodInvocation *invocation);
static gboolean
xfsm_client_dbus_end_session_response (XfsmDbusClient *object,
                                       GDBusMethodInvocation *invocation,
                                       gboolean arg_is_ok,
                                       const gchar *arg_reason);



static void
xfsm_client_dbus_class_init (XfsmClientClass *klass)
{
}


static void
xfsm_client_dbus_init (XfsmClient *client)
{
  GError *error = NULL;

  if (G_UNLIKELY (!client->connection))
    {
      g_critical ("Unable to contact D-Bus session bus: %s", error ? error->message : "Unknown error");
      return;
    }

  xfsm_verbose ("exporting path %s\n", client->object_path);

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (XFSM_DBUS_CLIENT (client)),
                                         client->connection,
                                         client->object_path,
                                         &error))
    {
      if (error != NULL)
        {
          g_critical ("error exporting interface: %s", error->message);
          g_clear_error (&error);
          return;
        }
    }

  xfsm_verbose ("exported on %s\n", g_dbus_interface_skeleton_get_object_path (G_DBUS_INTERFACE_SKELETON (XFSM_DBUS_CLIENT (client))));
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
  iface->handle_end_session_response = xfsm_client_dbus_end_session_response;
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
  xfsm_dbus_client_complete_get_id (object, invocation, XFSM_CLIENT (object)->id);
  return TRUE;
}


static gboolean
xfsm_client_dbus_get_state (XfsmDbusClient *object,
                            GDBusMethodInvocation *invocation)
{
  xfsm_dbus_client_complete_get_state (object, invocation, XFSM_CLIENT (object)->state);
  return TRUE;
}


static void
builder_add_value (GVariantBuilder *builder,
                   const gchar *name,
                   const GValue *value)
{
  if (name == NULL)
    {
      g_warning ("xfsm_client.c:builder_add_value: name must not be NULL");
      return;
    }

  if (G_VALUE_HOLDS_STRING (value))
    {
      g_variant_builder_add (builder, "{sv}", name, g_dbus_gvalue_to_gvariant (value, G_VARIANT_TYPE_STRING));
    }
  else if (G_VALUE_HOLDS_UCHAR (value))
    {
      g_variant_builder_add (builder, "{sv}", name, g_dbus_gvalue_to_gvariant (value, G_VARIANT_TYPE ("y")));
    }
  else if (G_VALUE_HOLDS (value, G_TYPE_STRV))
    {
      g_variant_builder_add (builder, "{sv}", name, g_dbus_gvalue_to_gvariant (value, G_VARIANT_TYPE_STRING_ARRAY));
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
  gchar *prop_name = key;
  GValue *prop_value = value;
  GVariantBuilder *out_properties = data;

  builder_add_value (out_properties, prop_name, prop_value);
  return FALSE;
}

static gboolean
xfsm_client_dbus_get_all_sm_properties (XfsmDbusClient *object,
                                        GDBusMethodInvocation *invocation)
{
  XfsmProperties *properties = XFSM_CLIENT (object)->properties;
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
  XfsmProperties *properties = XFSM_CLIENT (object)->properties;
  GVariantBuilder out_properties;
  gint i;

  if (G_UNLIKELY (properties == NULL))
    {
      throw_error (invocation, XFSM_ERROR_BAD_VALUE, "The client doesn't have any properties set yet");
      return TRUE;
    }

  g_variant_builder_init (&out_properties, G_VARIANT_TYPE ("a{sv}"));

  for (i = 0; arg_names[i]; ++i)
    {
      GValue *value = g_tree_lookup (properties->sm_properties, arg_names[i]);

      if (value != NULL)
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
  XfsmProperties *properties = XFSM_CLIENT (object)->properties;
  GVariantIter *iter;
  gchar *prop_name;
  GVariant *variant;

  if (G_UNLIKELY (properties == NULL))
    {
      throw_error (invocation, XFSM_ERROR_BAD_VALUE, "The client doesn't have any properties set yet");
      return TRUE;
    }

  g_variant_get (arg_properties, "a{sv}", &iter);

  while (g_variant_iter_next (iter, "{sv}", &prop_name, &variant))
    {
      GValue value;

      g_dbus_gvariant_to_gvalue (variant, &value);
      xfsm_properties_set (properties, prop_name, &value);

      g_variant_unref (variant);
    }

  xfsm_dbus_client_complete_set_sm_properties (object, invocation);
  return TRUE;
}


static gboolean
xfsm_client_dbus_delete_sm_properties (XfsmDbusClient *object,
                                       GDBusMethodInvocation *invocation,
                                       const gchar *const *arg_names)
{
  XfsmProperties *properties = XFSM_CLIENT (object)->properties;
  gchar **names = g_strdupv ((gchar **) arg_names);

  if (G_UNLIKELY (properties == NULL))
    {
      throw_error (invocation, XFSM_ERROR_BAD_VALUE, "The client doesn't have any properties set yet");
      return TRUE;
    }

  xfsm_client_delete_properties (XFSM_CLIENT (object), names, g_strv_length (names));

  g_strfreev (names);
  xfsm_dbus_client_complete_delete_sm_properties (object, invocation);
  return TRUE;
}


static gboolean
xfsm_client_dbus_terminate (XfsmDbusClient *object,
                            GDBusMethodInvocation *invocation)
{
  GError *error = NULL;

  xfsm_manager_terminate_client (XFSM_CLIENT (object)->manager, XFSM_CLIENT (object), &error);
  if (error != NULL)
    {
      throw_error (invocation, XFSM_ERROR_BAD_STATE, "Unable to terminate client, error was: %s", error->message);
      g_clear_error (&error);
      return TRUE;
    }

  xfsm_dbus_client_complete_terminate (object, invocation);
  return TRUE;
}

static gboolean
xfsm_client_dbus_end_session_response (XfsmDbusClient *object,
                                       GDBusMethodInvocation *invocation,
                                       gboolean arg_is_ok,
                                       const gchar *arg_reason)
{
  XfsmClient *client = XFSM_CLIENT (object);

  xfsm_verbose ("got response for client %s, manager state is %s\n",
                get_client_id (client),
                xfsm_manager_get_state (client->manager) == XFSM_MANAGER_SHUTDOWN ? "XFSM_MANAGER_SHUTDOWN" : xfsm_manager_get_state (client->manager) == XFSM_MANAGER_SHUTDOWNPHASE2 ? "XFSM_MANAGER_SHUTDOWNPHASE2"
                                                                                                                                                                                      : "Invalid time to respond");

  if (xfsm_manager_get_state (client->manager) == XFSM_MANAGER_SHUTDOWN)
    {
      xfsm_manager_save_yourself_done (client->manager, client, arg_is_ok);
    }
  else if (xfsm_manager_get_state (client->manager) == XFSM_MANAGER_SHUTDOWNPHASE2)
    {
      xfsm_manager_close_connection (client->manager, client, TRUE);
    }
  else
    {
      throw_error (invocation, XFSM_ERROR_BAD_STATE,
                   "This method should be sent in response to a QueryEndSession or EndSession signal only");
      return TRUE;
    }

  xfsm_dbus_client_complete_end_session_response (object, invocation);
  return TRUE;
}
