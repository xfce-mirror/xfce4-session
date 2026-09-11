/*
 * Copyright (c) 2026 Brian Tarricone <brian@terricone.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <libxfce4util/libxfce4util.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xfce-session-client-dbus.h"
#include "xfce-session-client-private.h"
#include "xfsm-client-dbus-client.h"
#include "xfsm-manager-dbus-client.h"

#define GET_PRIV(obj) ((XfceSessionClientDBusPrivate *) xfce_session_client_dbus_get_instance_private (XFCE_SESSION_CLIENT_DBUS (obj)))

typedef struct _XfceSessionClientDBusPrivate
{
  GDBusConnection *connection;
  XfsmDbusManager *manager_proxy;
  gchar *client_object_path;
  XfsmDbusClient *client_proxy;

  gboolean sent_quit_requested;
  gboolean shutdown_cancelled;
} XfceSessionClientDBusPrivate;

typedef enum
{
  XFSM_END_SESSION_MODE_FORCE = 0,
  XFSM_END_SESSION_MODE_NORMAL = 1,
} XfsmEndSessionMode;

static void
xfce_session_client_dbus_finalize (GObject *obj);

static gboolean
xfce_session_client_dbus_connect (XfceSessionClient *session_client,
                                  GError **error);
static void
xfce_session_client_dbus_disconnect (XfceSessionClient *session_client);
static void
xfce_session_client_dbus_request_shutdown (XfceSessionClient *session_client,
                                           XfceSessionClientShutdownHint shutdown_hint);
static gboolean
xfce_session_client_dbus_is_connected (XfceSessionClient *session_client);
static void
xfce_session_client_dbus_set_desktop_file (XfceSessionClient *session_client,
                                           const gchar *desktop_file);
static void
xfce_session_client_dbus_set_restart_style (XfceSessionClient *session_client,
                                            XfceSessionClientRestartStyle restart_style);
static void
xfce_session_client_dbus_set_priority (XfceSessionClient *session_client,
                                       guint8 priority);
static void
xfce_session_client_dbus_set_current_directory (XfceSessionClient *session_client,
                                                const gchar *current_directory);
static void
xfce_session_client_dbus_sync_commands (XfceSessionClient *session_client);

static void
dbussm_request_save_state (XfceSessionClientDBus *dsession_client,
                           XfsmDbusClient *client_proxy);
static void
dbussm_query_end_session (XfceSessionClientDBus *dsession_client,
                          guint32 end_session_mode,
                          XfsmDbusClient *client_proxy);
static void
dbussm_end_session (XfceSessionClientDBus *dsession_client,
                    guint32 end_session_mode,
                    XfsmDbusClient *client_proxy);
static void
dbussm_cancel_end_session (XfceSessionClientDBus *dsession_client);
static void
dbussm_stop (XfceSessionClientDBus *dsession_client);

static void
xfce_session_client_dbus_set_initial_properties (XfceSessionClientDBus *dsession_client);

G_DEFINE_TYPE_WITH_PRIVATE (XfceSessionClientDBus, xfce_session_client_dbus, XFCE_TYPE_SESSION_CLIENT)

static void
xfce_session_client_dbus_class_init (XfceSessionClientDBusClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = xfce_session_client_dbus_finalize;

  XfceSessionClientClass *session_client_class = XFCE_SESSION_CLIENT_CLASS (klass);
  session_client_class->connect = xfce_session_client_dbus_connect;
  session_client_class->disconnect = xfce_session_client_dbus_disconnect;
  session_client_class->request_shutdown = xfce_session_client_dbus_request_shutdown;
  session_client_class->is_connected = xfce_session_client_dbus_is_connected;
  session_client_class->set_desktop_file = xfce_session_client_dbus_set_desktop_file;
  session_client_class->set_restart_style = xfce_session_client_dbus_set_restart_style;
  session_client_class->set_priority = xfce_session_client_dbus_set_priority;
  session_client_class->set_current_directory = xfce_session_client_dbus_set_current_directory;
  session_client_class->sync_commands = xfce_session_client_dbus_sync_commands;
}

static void
xfce_session_client_dbus_init (XfceSessionClientDBus *session_client)
{
}

static void
xfce_session_client_dbus_finalize (GObject *obj)
{
  XfceSessionClientDBus *dsession_client = XFCE_SESSION_CLIENT_DBUS (obj);

  xfce_session_client_disconnect (XFCE_SESSION_CLIENT (dsession_client));

  G_OBJECT_CLASS (xfce_session_client_dbus_parent_class)->finalize (obj);
}


static gboolean
xfce_session_client_dbus_register (XfceSessionClientDBus *dsession_client,
                                   GError **error)
{
  XfceSessionClientDBusPrivate *priv = GET_PRIV (dsession_client);

  gchar *app_id;
  const gchar *desktop_file = _xfce_session_client_peek_desktop_file (XFCE_SESSION_CLIENT (dsession_client));
  if (desktop_file != NULL)
    {
      app_id = g_path_get_basename (desktop_file);
      gchar *ext = g_strrstr (app_id, ".desktop");
      if (ext != NULL)
        *ext = '\0';
    }
  else
    {
      app_id = g_strdup ("");
    }

  const gchar *client_id = xfce_session_client_get_client_id (XFCE_SESSION_CLIENT (dsession_client));
  gboolean ret = xfsm_dbus_manager_call_register_client_sync (priv->manager_proxy,
                                                              app_id,
                                                              client_id != NULL ? client_id : "",
                                                              &priv->client_object_path,
                                                              NULL,
                                                              error);

  g_free (app_id);
  return ret;
}


static gboolean
xfce_session_client_dbus_connect (XfceSessionClient *session_client,
                                  GError **error)
{
  XfceSessionClientDBus *dsession_client = XFCE_SESSION_CLIENT_DBUS (session_client);
  return _xfce_session_client_dbus_do_connect (dsession_client, xfce_session_client_dbus_register, error);
}

static void
xfce_session_client_dbus_disconnect (XfceSessionClient *session_client)
{
  XfceSessionClientDBus *dsession_client = XFCE_SESSION_CLIENT_DBUS (session_client);
  XfceSessionClientDBusPrivate *priv = GET_PRIV (dsession_client);

  if (priv->manager_proxy != NULL && priv->client_object_path != NULL)
    {
      GError *error = NULL;
      if (!xfsm_dbus_manager_call_unregister_client_sync (priv->manager_proxy,
                                                          priv->client_object_path,
                                                          NULL,
                                                          &error))
        {
          g_message ("Failed to unregister client from session manager: %s", error->message);
          g_error_free (error);
        }
    }

  g_clear_pointer (&priv->client_object_path, g_free);
  g_clear_object (&priv->client_proxy);
  g_clear_object (&priv->manager_proxy);
  g_clear_object (&priv->connection);
}

static void
xfce_session_client_dbus_request_shutdown (XfceSessionClient *session_client,
                                           XfceSessionClientShutdownHint shutdown_hint)
{
  XfceSessionClientDBus *dsession_client = XFCE_SESSION_CLIENT_DBUS (session_client);
  XfceSessionClientDBusPrivate *priv = GET_PRIV (dsession_client);

  if (priv->manager_proxy != NULL)
    {
      gboolean ret = TRUE;
      GError *error = NULL;

      switch (shutdown_hint)
        {
        case XFCE_SESSION_CLIENT_SHUTDOWN_HINT_ASK:
          ret = xfsm_dbus_manager_call_logout_sync (priv->manager_proxy,
                                                    TRUE,
                                                    TRUE,
                                                    NULL,
                                                    &error);
          break;

        case XFCE_SESSION_CLIENT_SHUTDOWN_HINT_LOGOUT:
          ret = xfsm_dbus_manager_call_logout_sync (priv->manager_proxy,
                                                    FALSE,
                                                    TRUE,
                                                    NULL,
                                                    &error);
          break;

        case XFCE_SESSION_CLIENT_SHUTDOWN_HINT_HALT:
          ret = xfsm_dbus_manager_call_shutdown_sync (priv->manager_proxy,
                                                      TRUE,
                                                      NULL,
                                                      &error);
          break;

        case XFCE_SESSION_CLIENT_SHUTDOWN_HINT_REBOOT:
          ret = xfsm_dbus_manager_call_restart_sync (priv->manager_proxy,
                                                     TRUE,
                                                     NULL,
                                                     &error);
          break;
        }

      if (!ret)
        {
          g_message ("Failed to request shutdown (hint=%d): %s", shutdown_hint, error->message);
          g_error_free (error);
        }
    }
}

static gboolean
xfce_session_client_dbus_is_connected (XfceSessionClient *session_client)
{
  XfceSessionClientDBusPrivate *priv = GET_PRIV (session_client);

  return priv->manager_proxy != NULL
         && priv->client_proxy != NULL
         && priv->client_object_path != NULL;
}

static void
xfce_session_client_dbus_set_property (XfceSessionClientDBus *dsession_client,
                                       const gchar *name,
                                       GVariant *value)
{
  XfceSessionClientDBusPrivate *priv = GET_PRIV (dsession_client);

  g_return_if_fail (XFSM_DBUS_IS_CLIENT (priv->client_proxy));

  GVariantDict dict;
  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert_value (&dict, name, value);
  GVariant *properties = g_variant_dict_end (&dict);

  GError *error = NULL;
  if (!xfsm_dbus_client_call_set_sm_properties_sync (priv->client_proxy,
                                                     properties,
                                                     NULL,
                                                     &error))
    {
      g_message ("Failed to set session manager property '%s': %s", name, error->message);
      g_error_free (error);
    }
}

static void
xfce_session_client_dbus_delete_property (XfceSessionClientDBus *dsession_client,
                                          const gchar *name)
{
  XfceSessionClientDBusPrivate *priv = GET_PRIV (dsession_client);

  g_return_if_fail (XFSM_DBUS_IS_CLIENT (priv->client_proxy));

  const gchar *const names[2] = {
    name,
    NULL,
  };

  GError *error = NULL;
  if (!xfsm_dbus_client_call_delete_sm_properties_sync (priv->client_proxy,
                                                        names,
                                                        NULL,
                                                        &error))
    {
      g_message ("Failed to delete session manager property '%s': %s", name, error->message);
      g_error_free (error);
    }
}

static void
xfce_session_client_dbus_set_desktop_file (XfceSessionClient *session_client,
                                           const gchar *desktop_file)
{
  XfceSessionClientDBus *dsession_client = XFCE_SESSION_CLIENT_DBUS (session_client);
  XfceSessionClientDBusPrivate *priv = GET_PRIV (dsession_client);

  if (priv->client_proxy != NULL)
    {
      if (desktop_file != NULL)
        {
          GVariant *value = g_variant_new_string (desktop_file);
          xfce_session_client_dbus_set_property (dsession_client, GsmDesktopFile, value);
        }
      else
        {
          xfce_session_client_dbus_delete_property (dsession_client, GsmDesktopFile);
        }
    }
}

static void
xfce_session_client_dbus_set_restart_style (XfceSessionClient *session_client,
                                            XfceSessionClientRestartStyle restart_style)
{
  XfceSessionClientDBus *dsession_client = XFCE_SESSION_CLIENT_DBUS (session_client);
  XfceSessionClientDBusPrivate *priv = GET_PRIV (dsession_client);

  if (priv->client_proxy != NULL)
    {
      guint8 hint = _xfce_session_client_restart_style_to_xsmp (restart_style);
      GVariant *value = g_variant_new_byte (hint);
      xfce_session_client_dbus_set_property (dsession_client, SmRestartStyleHint, value);
    }
}

static void
xfce_session_client_dbus_set_priority (XfceSessionClient *session_client,
                                       guint8 priority)
{
  XfceSessionClientDBus *dsession_client = XFCE_SESSION_CLIENT_DBUS (session_client);
  XfceSessionClientDBusPrivate *priv = GET_PRIV (dsession_client);

  if (priv->client_proxy != NULL)
    {
      GVariant *value = g_variant_new_byte (priority);
      xfce_session_client_dbus_set_property (dsession_client, GsmPriority, value);
    }
}

static void
xfce_session_client_dbus_set_current_directory (XfceSessionClient *session_client,
                                                const gchar *current_directory)
{
  XfceSessionClientDBus *dsession_client = XFCE_SESSION_CLIENT_DBUS (session_client);
  XfceSessionClientDBusPrivate *priv = GET_PRIV (dsession_client);

  if (priv->client_proxy != NULL)
    {
      if (current_directory != NULL)
        {
          GVariant *value = g_variant_new_string (current_directory);
          xfce_session_client_dbus_set_property (dsession_client, SmCurrentDirectory, value);
        }
      else
        {
          xfce_session_client_dbus_delete_property (dsession_client, SmCurrentDirectory);
        }
    }
}

static void
xfce_session_client_dbus_sync_commands (XfceSessionClient *session_client)
{
  XfceSessionClientDBus *dsession_client = XFCE_SESSION_CLIENT_DBUS (session_client);
  XfceSessionClientDBusPrivate *priv = GET_PRIV (dsession_client);

  if (priv->client_proxy != NULL)
    {
      GVariantDict to_set;
      g_variant_dict_init (&to_set, NULL);

      GStrvBuilder *to_delete = g_strv_builder_new ();

      gchar **clone_command = _xfce_session_client_dup_effective_clone_command (session_client);
      if (clone_command != NULL && clone_command[0] != NULL)
        {
          GVariant *value = g_variant_new_strv ((const gchar **) clone_command, g_strv_length (clone_command));
          g_variant_dict_insert_value (&to_set, SmCloneCommand, value);
        }
      else
        {
          g_strv_builder_add (to_delete, SmCloneCommand);
        }

      gchar **restart_command = _xfce_session_client_dup_effective_restart_command (session_client);
      if (restart_command != NULL && restart_command[0] != NULL)
        {
          GVariant *value = g_variant_new_strv ((const gchar **) restart_command, g_strv_length (restart_command));
          g_variant_dict_insert_value (&to_set, SmRestartCommand, value);
        }
      else
        {
          g_strv_builder_add (to_delete, SmRestartCommand);
        }

      gchar **discard_command = _xfce_session_client_dup_effective_discard_command (session_client);
      if (discard_command != NULL && discard_command[0] != NULL)
        {
          GVariant *value = g_variant_new_strv ((const gchar **) discard_command, g_strv_length (discard_command));
          g_variant_dict_insert_value (&to_set, SmDiscardCommand, value);
        }
      else
        {
          g_strv_builder_add (to_delete, SmDiscardCommand);
        }

      gchar **delete_names = g_strv_builder_end (to_delete);
      if (delete_names[0] != NULL)
        {
          GError *error = NULL;
          if (!xfsm_dbus_client_call_delete_sm_properties_sync (priv->client_proxy,
                                                                (const gchar **) delete_names,
                                                                NULL,
                                                                &error))
            {
              g_message ("Failed to delete session manager command properties: %s", error->message);
              g_error_free (error);
            }
        }

      GVariant *properties = g_variant_dict_end (&to_set);
      if (g_variant_n_children (properties) > 0)
        {
          GError *error = NULL;
          if (!xfsm_dbus_client_call_set_sm_properties_sync (priv->client_proxy,
                                                             properties,
                                                             NULL,
                                                             &error))
            {
              g_message ("Failed to set session manager command properties: %s", error->message);
              g_error_free (error);
            }
        }
      else
        {
          g_variant_unref (properties);
        }

      g_strfreev (delete_names);
      g_strv_builder_unref (to_delete);
      g_strfreev (clone_command);
      g_strfreev (restart_command);
      g_strfreev (discard_command);
    }
}

static void
dbussm_request_save_state (XfceSessionClientDBus *dsession_client,
                           XfsmDbusClient *client_proxy)
{
  _xfce_session_client_set_state (XFCE_SESSION_CLIENT (dsession_client), XFCE_SESSION_CLIENT_STATE_SAVING_PHASE_1);
  g_signal_emit_by_name (dsession_client, "save-state");

  GError *error = NULL;
  if (!xfsm_dbus_client_call_state_saved_sync (client_proxy, TRUE, NULL, &error))
    {
      g_message ("Failed to inform session manager of completed save: %s", error->message);
      g_error_free (error);
    }

  _xfce_session_client_set_state (XFCE_SESSION_CLIENT (dsession_client), XFCE_SESSION_CLIENT_STATE_IDLE);
}

static void
dbussm_query_end_session (XfceSessionClientDBus *dsession_client,
                          guint32 end_session_mode,
                          XfsmDbusClient *client_proxy)
{
  XfceSessionClientDBusPrivate *priv = GET_PRIV (dsession_client);

  gboolean cancel = FALSE;
  priv->shutdown_cancelled = FALSE;

  priv->sent_quit_requested = end_session_mode == XFSM_END_SESSION_MODE_NORMAL;
  if (priv->sent_quit_requested)
    {
      g_signal_emit_by_name (dsession_client, "quit-requested", &cancel);
    }

  gchar *reason = NULL;
  if (cancel)
    {
      GDesktopAppInfo *app_info = NULL;
      const gchar *app_name = NULL;

      const gchar *desktop_file = _xfce_session_client_peek_desktop_file (XFCE_SESSION_CLIENT (dsession_client));
      if (desktop_file != NULL)
        {
          app_info = g_desktop_app_info_new_from_filename (desktop_file);
          if (app_info != NULL)
            {
              app_name = g_app_info_get_display_name (G_APP_INFO (app_info));
            }
        }

      if (app_name == NULL)
        {
          app_name = g_get_prgname ();
        }

      if (app_name == NULL)
        {
          app_name = _("(unknown)");
        }

      reason = g_strdup_printf (_("Application '%s' cancelled logout"), app_name);

      if (app_info != NULL)
        {
          g_object_unref (app_info);
        }
    }

  if (!priv->shutdown_cancelled)
    {
      GError *error = NULL;
      if (!xfsm_dbus_client_call_end_session_response_sync (client_proxy,
                                                            !cancel,
                                                            reason != NULL ? reason : "",
                                                            NULL,
                                                            &error))
        {
          g_message ("Failed to respond to end session query: %s", error->message);
          g_error_free (error);
        }
    }

  g_free (reason);
  priv->shutdown_cancelled = FALSE;
}

static void
dbussm_end_session (XfceSessionClientDBus *dsession_client,
                    guint32 end_session_mode,
                    XfsmDbusClient *client_proxy)
{
  _xfce_session_client_set_state (XFCE_SESSION_CLIENT (dsession_client), XFCE_SESSION_CLIENT_STATE_SAVING_PHASE_1);
  g_signal_emit_by_name (dsession_client, "save-state");
  _xfce_session_client_set_state (XFCE_SESSION_CLIENT (dsession_client), XFCE_SESSION_CLIENT_STATE_FROZEN);

  GError *error = NULL;
  if (!xfsm_dbus_client_call_end_session_response_sync (client_proxy,
                                                        TRUE,
                                                        "",
                                                        NULL,
                                                        &error))
    {
      g_message ("Failed to respond to end session: %s", error->message);
      g_error_free (error);
    }
}

static void
dbussm_cancel_end_session (XfceSessionClientDBus *dsession_client)
{
  XfceSessionClientDBusPrivate *priv = GET_PRIV (dsession_client);

  priv->shutdown_cancelled = TRUE;
  _xfce_session_client_set_state (XFCE_SESSION_CLIENT (dsession_client), XFCE_SESSION_CLIENT_STATE_IDLE);

  if (priv->sent_quit_requested)
    {
      g_signal_emit_by_name (dsession_client, "quit-cancelled");
      priv->sent_quit_requested = FALSE;
    }
}

static void
dbussm_stop (XfceSessionClientDBus *dsession_client)
{
  _xfce_session_client_set_state (XFCE_SESSION_CLIENT (dsession_client), XFCE_SESSION_CLIENT_STATE_FROZEN);

  if (g_signal_has_handler_pending (dsession_client,
                                    _xfce_session_client_class_quit_signal_id (),
                                    0,
                                    FALSE))
    {
      g_signal_emit_by_name (dsession_client, "quit");
    }
  else
    {
      DBG ("XfceSessionClient will now call exit(0) which will abort your "
           "application. If you want to handle this yourself, you can "
           "implement the \"quit\"-signal.");

      exit (0);
    }
}

static void
xfce_session_client_dbus_set_initial_properties (XfceSessionClientDBus *dsession_client)
{
  XfceSessionClientDBusPrivate *priv = GET_PRIV (dsession_client);

  g_return_if_fail (XFSM_DBUS_IS_CLIENT (priv->client_proxy));
  XfceSessionClient *session_client = XFCE_SESSION_CLIENT (dsession_client);

  GVariantDict dict;
  g_variant_dict_init (&dict, NULL);

  g_variant_dict_insert_value (&dict, SmUserID, g_variant_new_string (g_get_user_name ()));
  g_variant_dict_insert_value (&dict, SmRestartStyleHint, g_variant_new_byte (_xfce_session_client_restart_style_to_xsmp (xfce_session_client_get_restart_style (session_client))));
  g_variant_dict_insert_value (&dict, SmProcessID, g_variant_new_take_string (g_strdup_printf ("%d", getpid ())));
  g_variant_dict_insert_value (&dict, SmCurrentDirectory, g_variant_new_string (xfce_session_client_get_current_directory (session_client)));
  g_variant_dict_insert_value (&dict, GsmPriority, g_variant_new_byte (xfce_session_client_get_priority (session_client)));

  const gchar *prgname = g_get_prgname ();
  if (prgname != NULL)
    {
      g_variant_dict_insert_value (&dict, SmProgram, g_variant_new_string (prgname));
    }

  const gchar *desktop_file = _xfce_session_client_peek_desktop_file (session_client);
  if (desktop_file != NULL)
    {
      g_variant_dict_insert_value (&dict, GsmDesktopFile, g_variant_new_string (desktop_file));
    }

  gchar **clone_command = _xfce_session_client_dup_effective_clone_command (session_client);
  if (clone_command != NULL && clone_command[0] != NULL)
    {
      GVariant *value = g_variant_new_strv ((const gchar **) clone_command, g_strv_length (clone_command));
      g_variant_dict_insert_value (&dict, SmCloneCommand, value);
    }
  g_strfreev (clone_command);

  gchar **restart_command = _xfce_session_client_dup_effective_restart_command (session_client);
  if (restart_command != NULL && restart_command[0] != NULL)
    {
      GVariant *value = g_variant_new_strv ((const gchar **) restart_command, g_strv_length (restart_command));
      g_variant_dict_insert_value (&dict, SmRestartCommand, value);
    }
  g_strfreev (restart_command);

  gchar **discard_command = _xfce_session_client_dup_effective_discard_command (session_client);
  if (discard_command != NULL && discard_command[0] != NULL)
    {
      GVariant *value = g_variant_new_strv ((const gchar **) discard_command, g_strv_length (discard_command));
      g_variant_dict_insert_value (&dict, SmDiscardCommand, value);
    }
  g_strfreev (discard_command);

  GVariant *properties = g_variant_dict_end (&dict);
  GError *error = NULL;
  if (!xfsm_dbus_client_call_set_sm_properties_sync (priv->client_proxy,
                                                     properties,
                                                     NULL,
                                                     &error))
    {
      g_message ("Failed to set initial session manager properties %s", error->message);
      g_error_free (error);
    }
}

gboolean
_xfce_session_client_dbus_do_connect (XfceSessionClientDBus *dsession_client,
                                      XfceSessionClientDBusRegisterFunc register_func,
                                      GError **error)
{
  XfceSessionClientDBusPrivate *priv = GET_PRIV (dsession_client);
  XfceSessionClient *session_client = XFCE_SESSION_CLIENT (dsession_client);

  _xfce_session_client_set_state (session_client, XFCE_SESSION_CLIENT_STATE_REGISTERING);

  if (priv->connection == NULL)
    {
      priv->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);
      if (priv->connection == NULL)
        {
          xfce_session_client_dbus_disconnect (session_client);
          _xfce_session_client_set_state (session_client, XFCE_SESSION_CLIENT_STATE_DISCONNECTED);
          return FALSE;
        }
    }

  if (priv->manager_proxy == NULL)
    {
      priv->manager_proxy = xfsm_dbus_manager_proxy_new_sync (priv->connection,
                                                              G_DBUS_PROXY_FLAGS_NONE,
                                                              "org.xfce.SessionManager",
                                                              "/org/xfce/SessionManager",
                                                              NULL,
                                                              error);
      if (priv->manager_proxy == NULL)
        {
          xfce_session_client_dbus_disconnect (session_client);
          _xfce_session_client_set_state (session_client, XFCE_SESSION_CLIENT_STATE_DISCONNECTED);
          return FALSE;
        }
    }

  if (priv->client_object_path == NULL)
    {
      if (!register_func (dsession_client, error))
        {
          xfce_session_client_dbus_disconnect (session_client);
          _xfce_session_client_set_state (session_client, XFCE_SESSION_CLIENT_STATE_DISCONNECTED);
          return FALSE;
        }

      g_clear_object (&priv->client_proxy);
    }

  if (priv->client_proxy == NULL)
    {
      priv->client_proxy = xfsm_dbus_client_proxy_new_sync (priv->connection,
                                                            G_DBUS_PROXY_FLAGS_NONE,
                                                            "org.xfce.SessionManager",
                                                            priv->client_object_path,
                                                            NULL,
                                                            error);
      if (priv->client_proxy == NULL)
        {
          xfce_session_client_dbus_disconnect (session_client);
          _xfce_session_client_set_state (session_client, XFCE_SESSION_CLIENT_STATE_DISCONNECTED);
          return FALSE;
        }

      gchar *final_client_id = NULL;
      if (!xfsm_dbus_client_call_get_id_sync (priv->client_proxy,
                                              &final_client_id,
                                              NULL,
                                              error))
        {
          xfce_session_client_dbus_disconnect (session_client);
          _xfce_session_client_set_state (session_client, XFCE_SESSION_CLIENT_STATE_DISCONNECTED);
          return FALSE;
        }

      const gchar *client_id = xfce_session_client_get_client_id (session_client);
      gboolean is_resumed = client_id != NULL && g_strcmp0 (client_id, final_client_id) == 0;

      _xfce_session_client_set_client_id (session_client, final_client_id);
      _xfce_session_client_set_is_resumed (session_client, is_resumed);
      g_free (final_client_id);

      xfce_session_client_dbus_set_initial_properties (dsession_client);

      g_signal_connect_swapped (priv->client_proxy, "request-save-state", G_CALLBACK (dbussm_request_save_state), dsession_client);
      g_signal_connect_swapped (priv->client_proxy, "query-end-session", G_CALLBACK (dbussm_query_end_session), dsession_client);
      g_signal_connect_swapped (priv->client_proxy, "end-session", G_CALLBACK (dbussm_end_session), dsession_client);
      g_signal_connect_swapped (priv->client_proxy, "cancel-end-session", G_CALLBACK (dbussm_cancel_end_session), dsession_client);
      g_signal_connect_swapped (priv->client_proxy, "stop", G_CALLBACK (dbussm_stop), dsession_client);

      _xfce_session_client_set_state (session_client, XFCE_SESSION_CLIENT_STATE_IDLE);
    }

  return TRUE;
}

XfsmDbusManager *
_xfce_session_client_dbus_get_xfsm_manager_proxy (XfceSessionClientDBus *dsession_client)
{
  return GET_PRIV (dsession_client)->manager_proxy;
}

void
_xfce_session_client_dbus_set_client_object_path (XfceSessionClientDBus *dsession_client,
                                                  const gchar *object_path)
{
  XfceSessionClientDBusPrivate *priv = GET_PRIV (dsession_client);
  g_free (priv->client_object_path);
  priv->client_object_path = g_strdup (object_path);
}
