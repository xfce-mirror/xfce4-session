/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
 * Copyright (c) 2010 Jannis Pohlmann <jannis@xfce.org>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gio/gio.h>

#include <X11/ICE/ICElib.h>
#include <X11/SM/SMlib.h>

#include <gdk-pixbuf/gdk-pixdata.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <libwnck/libwnck.h>

#include <libxfce4ui/libxfce4ui.h>

#include <libxfsm/xfsm-splash-engine.h>
#include <libxfsm/xfsm-util.h>

#include <xfce4-session/xfsm-manager-dbus.h>
#include <xfce4-session/xfsm-manager.h>
#include <xfce4-session/xfsm-chooser-icon.h>
#include <xfce4-session/xfsm-chooser.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-legacy.h>
#include <xfce4-session/xfsm-startup.h>
#include <xfce4-session/xfsm-marshal.h>
#include <xfce4-session/xfsm-error.h>
#include <xfce4-session/xfsm-logout-dialog.h>


#define DEFAULT_SESSION_NAME "Default"


struct _XfsmManager
{
  XfsmDbusManagerSkeleton parent;

  XfsmManagerState state;

  XfsmShutdownType  shutdown_type;
  XfsmShutdown     *shutdown_helper;
  gboolean          save_session;

  gboolean         session_chooser;
  gchar           *session_name;
  gchar           *session_file;
  gchar           *checkpoint_session_name;

  gboolean         start_at;

  gboolean         compat_gnome;
  gboolean         compat_kde;

  GQueue          *starting_properties;
  GQueue          *pending_properties;
  GQueue          *restart_properties;
  GQueue          *running_clients;

  gboolean         failsafe_mode;
  GQueue          *failsafe_clients;

  guint            die_timeout_id;
  guint            name_owner_id;

  GDBusConnection *connection;
};

typedef struct _XfsmManagerClass
{
  XfsmDbusManagerSkeletonClass parent;

  /*< signals >*/
  void (*quit)(XfsmManager *manager);
} XfsmManagerClass;

typedef struct
{
  XfsmManager *manager;
  XfsmClient  *client;
  guint        timeout_id;
} XfsmSaveTimeoutData;

typedef struct
{
  XfsmManager     *manager;
  XfsmShutdownType type;
  gboolean         allow_save;
} ShutdownIdleData;

enum
{
    MANAGER_QUIT,
    LAST_SIGNAL,
};

static guint manager_signals[LAST_SIGNAL] = { 0, };

static void       xfsm_manager_finalize (GObject *obj);

static gboolean   xfsm_manager_startup (XfsmManager *manager);
static void       xfsm_manager_start_client_save_timeout (XfsmManager *manager,
                                                          XfsmClient  *client);
static void       xfsm_manager_cancel_client_save_timeout (XfsmManager *manager,
                                                           XfsmClient  *client);
static gboolean   xfsm_manager_save_timeout (gpointer user_data);
static void       xfsm_manager_load_settings (XfsmManager   *manager,
                                              XfconfChannel *channel);
static gboolean   xfsm_manager_load_session (XfsmManager *manager);
static void       xfsm_manager_dbus_class_init (XfsmManagerClass *klass);
static void       xfsm_manager_dbus_init (XfsmManager *manager,
                                          GDBusConnection *connection);
static void       xfsm_manager_iface_init (XfsmDbusManagerIface *iface);
static void       xfsm_manager_dbus_cleanup (XfsmManager *manager);



G_DEFINE_TYPE_WITH_CODE (XfsmManager, xfsm_manager, XFSM_DBUS_TYPE_MANAGER_SKELETON, G_IMPLEMENT_INTERFACE (XFSM_DBUS_TYPE_MANAGER, xfsm_manager_iface_init));


static void
xfsm_manager_class_init (XfsmManagerClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;

  gobject_class->finalize = xfsm_manager_finalize;

  manager_signals[MANAGER_QUIT] = g_signal_new("manager-quit",
                                               G_OBJECT_CLASS_TYPE(gobject_class),
                                               G_SIGNAL_RUN_FIRST,
                                               G_STRUCT_OFFSET(XfsmManagerClass, quit),
                                               NULL, NULL,
                                               g_cclosure_marshal_VOID__VOID,
                                               G_TYPE_NONE, 0);

  xfsm_manager_dbus_class_init (klass);
}


static void
xfsm_manager_init (XfsmManager *manager)
{
  manager->state = XFSM_MANAGER_STARTUP;
  manager->session_chooser = FALSE;
  manager->failsafe_mode = TRUE;
  manager->shutdown_type = XFSM_SHUTDOWN_LOGOUT;
  manager->shutdown_helper = xfsm_shutdown_get ();
  manager->save_session = TRUE;

  manager->pending_properties = g_queue_new ();
  manager->starting_properties = g_queue_new ();
  manager->restart_properties = g_queue_new ();
  manager->running_clients = g_queue_new ();
  manager->failsafe_clients = g_queue_new ();
}

static void
xfsm_manager_finalize (GObject *obj)
{
  XfsmManager *manager = XFSM_MANAGER(obj);

  xfsm_manager_dbus_cleanup (manager);

  if (manager->die_timeout_id != 0)
    g_source_remove (manager->die_timeout_id);

  g_object_unref (manager->shutdown_helper);

  g_queue_foreach (manager->pending_properties, (GFunc) G_CALLBACK (xfsm_properties_free), NULL);
  g_queue_free (manager->pending_properties);

  g_queue_foreach (manager->starting_properties, (GFunc) G_CALLBACK (xfsm_properties_free), NULL);
  g_queue_free (manager->starting_properties);

  g_queue_foreach (manager->restart_properties, (GFunc) G_CALLBACK (xfsm_properties_free), NULL);
  g_queue_free (manager->restart_properties);

  g_queue_foreach (manager->running_clients, (GFunc) G_CALLBACK (g_object_unref), NULL);
  g_queue_free (manager->running_clients);

  g_queue_foreach (manager->failsafe_clients, (GFunc) G_CALLBACK (xfsm_failsafe_client_free), NULL);
  g_queue_free (manager->failsafe_clients);

  g_free (manager->session_name);
  g_free (manager->session_file);
  g_free (manager->checkpoint_session_name);

  G_OBJECT_CLASS (xfsm_manager_parent_class)->finalize (obj);
}


#ifdef G_CAN_INLINE
static inline void
#else
static void
#endif
xfsm_manager_set_state (XfsmManager     *manager,
                        XfsmManagerState state)
{
  XfsmManagerState old_state;

  /* idea here is to use this to set state always so we don't forget
   * to emit the signal */

  if (state == manager->state)
    return;

  old_state = manager->state;
  manager->state = state;

  xfsm_verbose ("\nstate is now %s\n",
                state == XFSM_MANAGER_STARTUP ? "XFSM_MANAGER_STARTUP" :
                state == XFSM_MANAGER_IDLE ?  "XFSM_MANAGER_IDLE" :
                state == XFSM_MANAGER_CHECKPOINT ? "XFSM_MANAGER_CHECKPOINT" :
                state == XFSM_MANAGER_SHUTDOWN ? "XFSM_MANAGER_SHUTDOWN" :
                state == XFSM_MANAGER_SHUTDOWNPHASE2 ? "XFSM_MANAGER_SHUTDOWNPHASE2" :
                "unknown");

  xfsm_dbus_manager_emit_state_changed (XFSM_DBUS_MANAGER (manager), old_state, state);
}


XfsmManager *
xfsm_manager_new (GDBusConnection *connection)
{
  XfsmManager *manager = XFSM_MANAGER (g_object_new (XFSM_TYPE_MANAGER, NULL));

  xfsm_manager_dbus_init (manager, connection);

  return manager;
}


static gboolean
xfsm_manager_startup (XfsmManager *manager)
{
  xfsm_startup_foreign (manager);
  g_queue_sort (manager->pending_properties, (GCompareDataFunc) G_CALLBACK (xfsm_properties_compare), NULL);
  xfsm_startup_begin (manager);
  return FALSE;
}


static void
xfsm_manager_restore_active_workspace (XfsmManager *manager,
                                       XfceRc      *rc)
{
  WnckWorkspace  *workspace;
  GdkDisplay     *display;
  WnckScreen     *screen;
  gchar           buffer[1024];
  gint            n, m;

  display = gdk_display_get_default ();
  for (n = 0; n < XScreenCount (gdk_x11_display_get_xdisplay (display)); ++n)
    {
      g_snprintf (buffer, 1024, "Screen%d_ActiveWorkspace", n);
      xfsm_verbose ("Attempting to restore %s\n", buffer);
      if (!xfce_rc_has_entry (rc, buffer))
        {
          xfsm_verbose ("no entry found\n");
          continue;
        }

      m = xfce_rc_read_int_entry (rc, buffer, 0);

      screen = wnck_screen_get (n);
      wnck_screen_force_update (screen);

      if (wnck_screen_get_workspace_count (screen) > m)
        {
          workspace = wnck_screen_get_workspace (screen, m);
          wnck_workspace_activate (workspace, GDK_CURRENT_TIME);
        }
    }
}


gboolean
xfsm_manager_handle_failed_properties (XfsmManager    *manager,
                                       XfsmProperties *properties)
{
  gint restart_style_hint;
  GError *error = NULL;

  /* Handle apps that failed to start, or died randomly, here */

  xfsm_properties_set_default_child_watch (properties);

  if (properties->restart_attempts_reset_id > 0)
    {
      g_source_remove (properties->restart_attempts_reset_id);
      properties->restart_attempts_reset_id = 0;
    }

  restart_style_hint = xfsm_properties_get_uchar (properties,
                                                  SmRestartStyleHint,
                                                  SmRestartIfRunning);

  if (restart_style_hint == SmRestartAnyway)
    {
      xfsm_verbose ("Client id %s died or failed to start, restarting anyway\n", properties->client_id);
      g_queue_push_tail (manager->restart_properties, properties);
    }
  else if (restart_style_hint == SmRestartImmediately)
    {
      if (++properties->restart_attempts > MAX_RESTART_ATTEMPTS)
        {
          xfsm_verbose ("Client Id = %s, reached maximum attempts [Restart attempts = %d]\n"
                        "   Will be re-scheduled for run on next startup\n",
                        properties->client_id, properties->restart_attempts);

          g_queue_push_tail (manager->restart_properties, properties);
        }
      else
        {
          xfsm_verbose ("Client Id = %s disconnected, restarting\n",
                        properties->client_id);

          if (G_UNLIKELY (!xfsm_startup_start_properties (properties, manager)))
            {
              /* this failure has nothing to do with the app itself, so
               * just add it to restart props */
              g_queue_push_tail (manager->restart_properties, properties);
            }
          else
            {
              /* put it back in the starting list */
              g_queue_push_tail (manager->starting_properties, properties);
            }
        }
    }
  else
    {
      gchar **discard_command;

      /* We get here if a SmRestartNever or SmRestartIfRunning client
       * has exited.  SmRestartNever clients shouldn't have discard
       * commands, but it can't hurt to run it if it has one for some
       * reason, and might clean up garbage we don't want. */
      xfsm_verbose ("Client Id %s exited, removing from session.\n",
                    properties->client_id);

      discard_command = xfsm_properties_get_strv (properties, SmDiscardCommand);
      if (discard_command != NULL && g_strv_length (discard_command) > 0)
        {
          /* Run the SmDiscardCommand after the client exited in any state,
           * but only if we don't expect the client to be restarted,
           * whether immediately or in the next session.
           *
           * NB: This used to also have the condition that the manager is
           * in the IDLE state, but this was removed because I can't see
           * why you'd treat a client that fails during startup any
           * differently, and this fixes a potential properties leak.
           *
           * Unfortunately the spec isn't clear about the usage of the
           * discard command. Have to check ksmserver/gnome-session, and
           * come up with consistent behaviour.
           *
           * But for now, this work-around fixes the problem of the
           * ever-growing number of xfwm4 session files when restarting
           * xfwm4 within a session.
           */
          xfsm_verbose ("Client Id = %s: running discard command %s:%d.\n\n",
                        properties->client_id, *discard_command,
                        g_strv_length (discard_command));

          if (!g_spawn_sync (xfsm_properties_get_string (properties, SmCurrentDirectory),
                             discard_command,
                             xfsm_properties_get_strv (properties, SmEnvironment),
                             G_SPAWN_SEARCH_PATH,
                             NULL, NULL,
                             NULL, NULL,
                             NULL, &error))
            {
              g_warning ("Failed to running discard command \"%s\": %s",
                         *discard_command, error->message);
              g_error_free (error);
            }
        }

      return FALSE;
    }

  return TRUE;
}


static gboolean
xfsm_manager_choose_session (XfsmManager *manager,
                             XfceRc      *rc)
{
  XfsmSessionInfo *session;
  GdkPixbuf       *preview_default = NULL;
  gboolean         load = FALSE;
  GList           *sessions = NULL;
  GList           *lp;
  gchar          **groups;
  gchar           *name;
  gint             result;
  gint             n;

  groups = xfce_rc_get_groups (rc);
  for (n = 0; groups[n] != NULL; ++n)
    {
      if (strncmp (groups[n], "Session: ", 9) == 0)
        {
          xfce_rc_set_group (rc, groups[n]);
          session = g_new0 (XfsmSessionInfo, 1);
          session->name = groups[n] + 9;
          session->atime = xfce_rc_read_int_entry (rc, "LastAccess", 0);
          session->preview = xfsm_load_session_preview (session->name);

          if (session->preview == NULL)
            {
              if (G_UNLIKELY (preview_default == NULL))
                {
                  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                  /* TODO: Turn this into a normal pixbuf? */
                  preview_default = gdk_pixbuf_new_from_inline (-1, xfsm_chooser_icon_data,
                                                                FALSE, NULL);
                  G_GNUC_END_IGNORE_DEPRECATIONS
                }

              session->preview = GDK_PIXBUF (g_object_ref (preview_default));
            }

          sessions = g_list_append (sessions, session);
        }
    }

  if (preview_default != NULL)
    g_object_unref (preview_default);

  if (sessions != NULL)
    {
      result = xfsm_splash_screen_choose (splash_screen, sessions,
                                          manager->session_name, &name);

      if (result == XFSM_CHOOSE_LOGOUT)
        {
          xfce_rc_close (rc);
          exit (EXIT_SUCCESS);
        }
      else if (result == XFSM_CHOOSE_LOAD)
        {
          load = TRUE;
        }

      if (manager->session_name != NULL)
        g_free (manager->session_name);
      manager->session_name = name;

      for (lp = sessions; lp != NULL; lp = lp->next)
        {
          session = (XfsmSessionInfo *) lp->data;
          g_object_unref (session->preview);
          g_free (session);
        }

      g_list_free (sessions);
    }

  g_strfreev (groups);

  return load;
}


static gboolean
xfsm_manager_load_session (XfsmManager *manager)
{
  XfsmProperties *properties;
  gchar           buffer[1024];
  XfceRc         *rc;
  gint            count;

  if (!g_file_test (manager->session_file, G_FILE_TEST_IS_REGULAR))
    {
      g_warning ("xfsm_manager_load_session: Something wrong with %s, Does it exist? Permissions issue?", manager->session_file);
      return FALSE;
    }

  rc = xfce_rc_simple_open (manager->session_file, FALSE);
  if (G_UNLIKELY (rc == NULL))
  {
    g_warning ("xfsm_manager_load_session: unable to open %s", manager->session_file);
    return FALSE;
  }

  if (manager->session_chooser && !xfsm_manager_choose_session (manager, rc))
    {
      g_warning ("xfsm_manager_load_session: failed to choose session");
      xfce_rc_close (rc);
      return FALSE;
    }

  g_snprintf (buffer, 1024, "Session: %s", manager->session_name);
  xfsm_verbose ("loading %s\n", buffer);

  xfce_rc_set_group (rc, buffer);
  count = xfce_rc_read_int_entry (rc, "Count", 0);
  if (G_UNLIKELY (count <= 0))
    {
      xfce_rc_close (rc);
      return FALSE;
    }

  while (count-- > 0)
    {
      g_snprintf (buffer, 1024, "Client%d_", count);
      properties = xfsm_properties_load (rc, buffer);
      if (G_UNLIKELY (properties == NULL))
        {
          xfsm_verbose ("%s has no properties. Skipping\n", buffer);
          continue;
        }
      if (xfsm_properties_check (properties))
        {
          g_queue_push_tail (manager->pending_properties, properties);
        }
      else
        {
          xfsm_verbose ("%s has invalid properties. Skipping\n", buffer);
          xfsm_properties_free (properties);
        }
    }

  xfsm_verbose ("Finished loading clients from rc file\n");

  /* load legacy applications */
  xfsm_legacy_load_session (rc);

  xfce_rc_close (rc);

  return g_queue_peek_head (manager->pending_properties) != NULL;
}


static gboolean
xfsm_manager_load_failsafe (XfsmManager   *manager,
                            XfconfChannel *channel,
                            gchar        **error)
{
  FailsafeClient *fclient;
  gchar          *failsafe_name;
  gchar           propbuf[4096];
  gchar         **command;
  gchar           command_entry[256];
  gchar           screen_entry[256];
  gint            count;
  gint            i;

  failsafe_name = xfconf_channel_get_string (channel, "/general/FailsafeSessionName", NULL);
  if (G_UNLIKELY (!failsafe_name))
    {
      if (error)
        *error = g_strdup_printf (_("Unable to determine failsafe session name.  Possible causes: xfconfd isn't running (D-Bus setup problem); environment variable $XDG_CONFIG_DIRS is set incorrectly (must include \"%s\"), or xfce4-session is installed incorrectly."),
                                  SYSCONFDIR);
      return FALSE;
    }

  g_snprintf (propbuf, sizeof (propbuf), "/sessions/%s/IsFailsafe",
              failsafe_name);
  if (!xfconf_channel_get_bool (channel, propbuf, FALSE))
    {
      if (error)
        {
          *error = g_strdup_printf (_("The specified failsafe session (\"%s\") is not marked as a failsafe session."),
                                    failsafe_name);
        }
      g_free (failsafe_name);
      return FALSE;
    }

  g_snprintf (propbuf, sizeof (propbuf), "/sessions/%s/Count", failsafe_name);
  count = xfconf_channel_get_int (channel, propbuf, 0);

  for (i = 0; i < count; ++i)
    {
      g_snprintf (command_entry, sizeof (command_entry),
                  "/sessions/%s/Client%d_Command", failsafe_name, i);
      command = xfconf_channel_get_string_list (channel, command_entry);
      if (G_UNLIKELY (command == NULL))
        continue;

      g_snprintf (screen_entry, sizeof (screen_entry),
                  "/sessions/%s/Client%d_PerScreen", failsafe_name, i);

      fclient = g_new0 (FailsafeClient, 1);
      fclient->command = command;
      fclient->screen = gdk_screen_get_default ();
      g_queue_push_tail (manager->failsafe_clients, fclient);
    }

  if (g_queue_peek_head (manager->failsafe_clients) == NULL)
    {
      if (error)
        *error = g_strdup (_("The list of applications in the failsafe session is empty."));
      return FALSE;
    }

  return TRUE;
}


static void
xfsm_manager_load_settings (XfsmManager   *manager,
                            XfconfChannel *channel)
{
  gboolean session_loaded = FALSE;

  manager->session_name = xfconf_channel_get_string (channel,
                                                     "/general/SessionName",
                                                     DEFAULT_SESSION_NAME);
  if (G_UNLIKELY (manager->session_name[0] == '\0'))
    {
      g_free (manager->session_name);
      manager->session_name = g_strdup (DEFAULT_SESSION_NAME);
    }

  manager->session_chooser = xfconf_channel_get_bool (channel, "/chooser/AlwaysDisplay", FALSE);

  session_loaded = xfsm_manager_load_session (manager);

  if (session_loaded)
    {
      xfsm_verbose ("Session \"%s\" loaded successfully.\n\n", manager->session_name);
      manager->failsafe_mode = FALSE;
    }
  else
    {
      gchar *errorstr = NULL;

      if (!xfsm_manager_load_failsafe (manager, channel, &errorstr))
        {
          if (G_LIKELY (splash_screen != NULL))
            {
              xfsm_splash_screen_free (splash_screen);
              splash_screen = NULL;
            }

          /* FIXME: migrate this into the splash screen somehow so the
           * window doesn't look ugly (right now no WM is running, so it
           * won't have window decorations). */
          xfce_message_dialog (NULL, _("Session Manager Error"),
                               "dialog-error",
                               _("Unable to load a failsafe session"),
                               errorstr,
                               XFCE_BUTTON_TYPE_MIXED, "application-exit", _("_Quit"), GTK_RESPONSE_ACCEPT, NULL);
          g_free (errorstr);
          exit (EXIT_FAILURE);
        }
      manager->failsafe_mode = TRUE;
    }
}


void
xfsm_manager_load (XfsmManager   *manager,
                   XfconfChannel *channel)
{
  gchar *display_name;
  gchar *resource_name;
#ifdef HAVE_OS_CYGWIN
  gchar *s;
#endif

  manager->compat_gnome = xfconf_channel_get_bool (channel, "/compat/LaunchGNOME", FALSE);
  manager->compat_kde = xfconf_channel_get_bool (channel, "/compat/LaunchKDE", FALSE);
  manager->start_at = xfconf_channel_get_bool (channel, "/general/StartAssistiveTechnologies", FALSE);

  display_name  = xfsm_gdk_display_get_fullname (gdk_display_get_default ());

#ifdef HAVE_OS_CYGWIN
  /* rename a colon (:) to a hash (#) under cygwin. windows doesn't like
   * filenames with a colon... */
  for (s = display_name; *s != '\0'; ++s)
    if (*s == ':')
      *s = '#';
#endif

  resource_name = g_strconcat ("sessions/xfce4-session-", display_name, NULL);
  manager->session_file  = xfce_resource_save_location (XFCE_RESOURCE_CACHE, resource_name, TRUE);
  g_free (resource_name);
  g_free (display_name);

  xfsm_manager_load_settings (manager, channel);
}


gboolean
xfsm_manager_restart (XfsmManager *manager)
{
  GdkPixbuf *preview;
  unsigned   steps;

  g_assert (manager->session_name != NULL);

  /* setup legacy application handling */
  xfsm_legacy_init ();

  /* tell splash screen that the session is starting now */
  preview = xfsm_load_session_preview (manager->session_name);
  if (preview == NULL)
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      /* TODO: Turn this into a normal pixbuf? */
      preview = gdk_pixbuf_new_from_inline (-1, xfsm_chooser_icon_data, FALSE, NULL);
      G_GNUC_END_IGNORE_DEPRECATIONS
    }
  steps = g_queue_get_length (manager->failsafe_mode ? manager->failsafe_clients : manager->pending_properties);
  xfsm_splash_screen_start (splash_screen, manager->session_name, preview, steps);
  g_object_unref (preview);

  g_idle_add ((GSourceFunc) xfsm_manager_startup, manager);

  return TRUE;
}


void
xfsm_manager_signal_startup_done (XfsmManager *manager)
{
  gchar buffer[1024];
  XfceRc *rc;

  xfsm_verbose ("Manager finished startup, entering IDLE mode now\n\n");
  xfsm_manager_set_state (manager, XFSM_MANAGER_IDLE);

  if (!manager->failsafe_mode)
    {
      /* restore active workspace, this has to be done after the
       * window manager is up, so we do it last.
       */
      g_snprintf (buffer, 1024, "Session: %s", manager->session_name);
      rc = xfce_rc_simple_open (manager->session_file, TRUE);
      xfce_rc_set_group (rc, buffer);
      xfsm_manager_restore_active_workspace (manager, rc);
      xfce_rc_close (rc);

      /* start legacy applications now */
      xfsm_legacy_startup ();
    }
}


XfsmClient*
xfsm_manager_new_client (XfsmManager *manager,
                         SmsConn      sms_conn,
                         gchar      **error)
{
  XfsmClient *client = NULL;

  if (G_UNLIKELY (manager->state != XFSM_MANAGER_IDLE)
      && G_UNLIKELY (manager->state != XFSM_MANAGER_STARTUP))
    {
      if (error != NULL)
        *error = "We don't accept clients while in CheckPoint/Shutdown state!";
      return NULL;
    }

  client = xfsm_client_new (manager, sms_conn, manager->connection);
  return client;
}


static gboolean
xfsm_manager_reset_restart_attempts (gpointer data)
{
  XfsmProperties *properties = data;

  properties->restart_attempts = 0;
  properties->restart_attempts_reset_id = 0;

  return FALSE;
}


static XfsmProperties*
xfsm_manager_get_pending_properties (XfsmManager *manager,
                                     const gchar *previous_id)
{
  XfsmProperties *properties = NULL;
  GList          *lp;

  lp = g_queue_find_custom (manager->starting_properties,
                            previous_id,
                            (GCompareFunc) xfsm_properties_compare_id);

  if (lp != NULL)
    {
      properties = XFSM_PROPERTIES (lp->data);
      g_queue_delete_link (manager->starting_properties, lp);
    }
  else
    {
      lp = g_queue_find_custom (manager->pending_properties,
                                previous_id,
                                (GCompareFunc) xfsm_properties_compare_id);
      if (lp != NULL)
        {
          properties = XFSM_PROPERTIES (lp->data);
          g_queue_delete_link (manager->pending_properties, lp);
        }
    }

  return properties;
}

static void
xfsm_manager_handle_old_client_reregister (XfsmManager    *manager,
                                           XfsmClient     *client,
                                           XfsmProperties *properties)
{
  /* cancel startup timer */
  if (properties->startup_timeout_id > 0)
    {
      g_source_remove (properties->startup_timeout_id);
      properties->startup_timeout_id = 0;
    }

  /* cancel the old child watch, and replace it with one that
   * doesn't really do anything but reap the child */
  xfsm_properties_set_default_child_watch (properties);

  xfsm_client_set_initial_properties (client, properties);

  /* if we've been restarted, we'll want to reset the restart
   * attempts counter if the client stays alive for a while */
  if (properties->restart_attempts > 0 && properties->restart_attempts_reset_id == 0)
    {
      properties->restart_attempts_reset_id = g_timeout_add (RESTART_RESET_TIMEOUT,
                                                             xfsm_manager_reset_restart_attempts,
                                                             properties);
    }
}

gboolean
xfsm_manager_register_client (XfsmManager *manager,
                              XfsmClient  *client,
                              const gchar *dbus_client_id,
                              const gchar *previous_id)
{
  XfsmProperties *properties = NULL;
  SmsConn         sms_conn;

  sms_conn = xfsm_client_get_sms_connection (client);

  /* This part is for sms based clients */
  if (previous_id != NULL)
    {
      properties = xfsm_manager_get_pending_properties (manager, previous_id);

      /* If previous_id is invalid, the SM will send a BadValue error message
       * to the client and reverts to register state waiting for another
       * RegisterClient message.
       */
      if (properties == NULL)
        {
          xfsm_verbose ("Client Id = %s registering, failed to find matching "
                        "properties\n", previous_id);
          return FALSE;
        }

      xfsm_manager_handle_old_client_reregister (manager, client, properties);
    }
  else
    {
      if (sms_conn != NULL)
        {
          /* new sms client */
          gchar *client_id = xfsm_generate_client_id (sms_conn);

          properties = xfsm_properties_new (client_id, SmsClientHostName (sms_conn));
          xfsm_client_set_initial_properties (client, properties);

          g_free (client_id);
        }
    }

  /* this part is for dbus clients */
  if (dbus_client_id != NULL)
    {
      properties = xfsm_manager_get_pending_properties (manager, dbus_client_id);

      if (properties != NULL)
        {
          xfsm_manager_handle_old_client_reregister (manager, client, properties);
          /* need this to continue session loading during XFSM_MANAGER_STARTUP */
          previous_id = dbus_client_id;
        }
      else
        {
          /* new dbus client */
          gchar *hostname = xfce_gethostname ();

          properties = xfsm_properties_new (dbus_client_id, hostname);
          xfsm_client_set_initial_properties (client, properties);

          g_free (hostname);
        }
    }


  g_queue_push_tail (manager->running_clients, client);

  if (sms_conn != NULL)
    {
      SmsRegisterClientReply (sms_conn, (char *) xfsm_client_get_id (client));
    }

  xfsm_dbus_manager_emit_client_registered (XFSM_DBUS_MANAGER (manager), xfsm_client_get_object_path (client));

  if (previous_id == NULL)
    {
      if (sms_conn != NULL)
        {
          SmsSaveYourself (sms_conn, SmSaveLocal, False, SmInteractStyleNone, False);
          xfsm_client_set_state (client, XFSM_CLIENT_SAVINGLOCAL);
          xfsm_manager_start_client_save_timeout (manager, client);
        }
    }

  if (previous_id != NULL && manager->state == XFSM_MANAGER_STARTUP)
    {
      /* Only continue the startup if the previous_id matched one of
       * the starting_properties. If there was no match above,
       * previous_id will be NULL here.  We don't need to continue when
       * in failsafe mode because in that case the failsafe session is
       * started all at once.
       */
      if (g_queue_peek_head (manager->starting_properties) == NULL)
        xfsm_startup_session_continue (manager);
    }

  return TRUE;
}


void
xfsm_manager_start_interact (XfsmManager *manager,
                             XfsmClient  *client)
{
  SmsConn sms = xfsm_client_get_sms_connection (client);

  /* notify client of interact */
  if (sms != NULL)
    {
      SmsInteract (sms);
    }
  xfsm_client_set_state (client, XFSM_CLIENT_INTERACTING);

  /* stop save yourself timeout */
  xfsm_manager_cancel_client_save_timeout (manager, client);
}


void
xfsm_manager_interact (XfsmManager *manager,
                       XfsmClient  *client,
                       int          dialog_type)
{
  GList *lp;

  if (G_UNLIKELY (xfsm_client_get_state (client) != XFSM_CLIENT_SAVING))
    {
      xfsm_verbose ("Client Id = %s, requested INTERACT, but client is not in SAVING mode\n"
                    "   Client will be disconnected now.\n\n",
                    xfsm_client_get_id (client));
      xfsm_manager_close_connection (manager, client, TRUE);
    }
  else if (G_UNLIKELY (manager->state != XFSM_MANAGER_CHECKPOINT)
        && G_UNLIKELY (manager->state != XFSM_MANAGER_SHUTDOWN))
    {
      xfsm_verbose ("Client Id = %s, requested INTERACT, but manager is not in CheckPoint/Shutdown mode\n"
                    "   Client will be disconnected now.\n\n",
                    xfsm_client_get_id (client));
      xfsm_manager_close_connection (manager, client, TRUE);
    }
  else
    {
      for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
           lp;
           lp = lp->next)
        {
          XfsmClient *cl = lp->data;
          if (xfsm_client_get_state (cl) == XFSM_CLIENT_INTERACTING)
            {
              /* a client is already interacting, so new client has to wait */
              xfsm_client_set_state (client, XFSM_CLIENT_WAITFORINTERACT);
              xfsm_manager_cancel_client_save_timeout(manager, client);
              return;
            }
        }

      xfsm_manager_start_interact (manager, client);
    }
}


void
xfsm_manager_interact_done (XfsmManager *manager,
                            XfsmClient  *client,
                            gboolean     cancel_shutdown)
{
  GList *lp;

  if (G_UNLIKELY (xfsm_client_get_state (client) != XFSM_CLIENT_INTERACTING))
    {
      xfsm_verbose ("Client Id = %s, send INTERACT DONE, but client is not in INTERACTING state\n"
                    "   Client will be disconnected now.\n\n",
                    xfsm_client_get_id (client));
      xfsm_manager_close_connection (manager, client, TRUE);
      return;
    }
  else if (G_UNLIKELY (manager->state != XFSM_MANAGER_CHECKPOINT)
        && G_UNLIKELY (manager->state != XFSM_MANAGER_SHUTDOWN))
    {
      xfsm_verbose ("Client Id = %s, send INTERACT DONE, but manager is not in CheckPoint/Shutdown state\n"
                    "   Client will be disconnected now.\n\n",
                    xfsm_client_get_id (client));
      xfsm_manager_close_connection (manager, client, TRUE);
      return;
    }

  xfsm_client_set_state (client, XFSM_CLIENT_SAVING);

  /* Setting the cancel-shutdown field to True indicates that the user
   * has requested that the entire shutdown be cancelled. Cancel-
   * shutdown may only be True if the corresponding SaveYourself
   * message specified True for the shutdown field and Any or Error
   * for the interact-style field. Otherwise, cancel-shutdown must be
   * False.
   */
  if (cancel_shutdown && manager->state == XFSM_MANAGER_SHUTDOWN)
    {
      /* we go into checkpoint state from here... */
      xfsm_manager_set_state (manager, XFSM_MANAGER_CHECKPOINT);

      /* If a shutdown is in progress, the user may have the option
       * of cancelling the shutdown. If the shutdown is cancelled
       * (specified in the "Interact Done" message), the session
       * manager should send a "Shutdown Cancelled" message to each
       * client that requested to interact.
       */
      for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
           lp;
           lp = lp->next)
        {
          XfsmClient *cl = lp->data;
          SmsConn sms = xfsm_client_get_sms_connection (cl);

          /* only sms clients do the interact stuff */
          if (sms && xfsm_client_get_state (cl) != XFSM_CLIENT_WAITFORINTERACT)
            continue;

          /* reset all clients that are waiting for interact */
          xfsm_client_set_state (client, XFSM_CLIENT_SAVING);
          if (sms != NULL)
            {
              SmsShutdownCancelled (sms);
            }
          else
            {
              xfsm_client_cancel_shutdown (client);
            }
        }

        xfsm_dbus_manager_emit_shutdown_cancelled (XFSM_DBUS_MANAGER (manager));
    }
  else
    {
      /* let next client interact */
      for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
           lp;
           lp = lp->next)
        {
          XfsmClient *cl = lp->data;
          if (xfsm_client_get_state (cl) == XFSM_CLIENT_WAITFORINTERACT)
            {
              xfsm_manager_start_interact (manager, cl);
              break;
            }
        }
    }

  /* restart save yourself timeout for client */
  xfsm_manager_start_client_save_timeout (manager, client);
}


static void
xfsm_manager_save_yourself_global (XfsmManager     *manager,
                                   gint             save_type,
                                   gboolean         shutdown,
                                   gint             interact_style,
                                   gboolean         fast,
                                   XfsmShutdownType shutdown_type,
                                   gboolean         allow_shutdown_save)
{
  gboolean  shutdown_save = allow_shutdown_save;
  GList    *lp;
  GError   *error = NULL;

  xfsm_verbose ("entering");

  if (shutdown)
    {
      if (!fast && shutdown_type == XFSM_SHUTDOWN_ASK)
        {
          /* if we're not specifying fast shutdown, and we're ok with
           * prompting then ask the user what to do */
          if (!xfsm_logout_dialog (manager->session_name,
                                   &manager->shutdown_type,
                                   &shutdown_save,
                                   manager->start_at))
            {
              return;
            }

          /* |allow_shutdown_save| is ignored if we prompt the user.  i think
           * this is the right thing to do. */
        }

      /* update shutdown type if we didn't prompt the user */
      if (shutdown_type != XFSM_SHUTDOWN_ASK)
        manager->shutdown_type = shutdown_type;

      xfsm_launch_desktop_files_on_shutdown (FALSE, manager->shutdown_type);

      /* we only save the session and quit if we're actually shutting down;
       * suspend, hibernate, hybrid sleep and switch user will (if successful) return us to
       * exactly the same state, so there's no need to save session */
      if (manager->shutdown_type == XFSM_SHUTDOWN_SUSPEND
          || manager->shutdown_type == XFSM_SHUTDOWN_HIBERNATE
          || manager->shutdown_type == XFSM_SHUTDOWN_HYBRID_SLEEP
          || manager->shutdown_type == XFSM_SHUTDOWN_SWITCH_USER)
        {
          if (!xfsm_shutdown_try_type (manager->shutdown_helper,
                                       manager->shutdown_type,
                                       &error))
            {
              xfsm_verbose ("failed calling shutdown, error message was %s", error->message);
              xfce_message_dialog (NULL, _("Shutdown Failed"),
                                   "dialog-error",
                                   manager->shutdown_type == XFSM_SHUTDOWN_SUSPEND
                                   ? _("Failed to suspend session")
                                   : manager->shutdown_type == XFSM_SHUTDOWN_HIBERNATE
                                   ? _("Failed to hibernate session")
                                   : manager->shutdown_type == XFSM_SHUTDOWN_HYBRID_SLEEP
                                   ? _("Failed to hybrid sleep session")
                                   : _("Failed to switch user"),
                                   error->message,
                                   XFCE_BUTTON_TYPE_MIXED, "window-close", _("_Close"), GTK_RESPONSE_ACCEPT,
                                   NULL);
              g_error_free (error);
            }

          /* at this point, either we failed to suspend/hibernate/hybrid sleep/switch user,
           * or we successfully did and we've been woken back
           * up or returned to the session, so return control to the user */
          return;
        }
    }

  /* don't save the session if shutting down without save */
  manager->save_session = !shutdown || shutdown_save;

  if (save_type == SmSaveBoth && !manager->save_session)
    {
      /* saving the session, so clients should
       * (prompt to) save the user data only */
      save_type = SmSaveGlobal;
    }

  xfsm_manager_set_state (manager,
                          shutdown
                          ? XFSM_MANAGER_SHUTDOWN
                          : XFSM_MANAGER_CHECKPOINT);

  /* handle legacy applications first! */
  if (manager->save_session)
      xfsm_legacy_perform_session_save ();

  for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
       lp;
       lp = lp->next)
    {
      XfsmClient *client = lp->data;
      XfsmProperties *properties = xfsm_client_get_properties (client);
      const gchar *program;

      /* xterm's session management is broken, so we won't
       * send a SAVE YOURSELF to xterms */
      program = xfsm_properties_get_string (properties, SmProgram);
      if (program != NULL && strcasecmp (program, "xterm") == 0)
        continue;

      if (xfsm_client_get_state (client) != XFSM_CLIENT_SAVINGLOCAL)
        {
          SmsConn sms = xfsm_client_get_sms_connection (client);
          if (sms != NULL)
            {
              SmsSaveYourself (sms, save_type, shutdown, interact_style, fast);
            }
        }

      xfsm_client_set_state (client, XFSM_CLIENT_SAVING);
      xfsm_manager_start_client_save_timeout (manager, client);
    }
}


void
xfsm_manager_save_yourself (XfsmManager *manager,
                            XfsmClient  *client,
                            gint         save_type,
                            gboolean     shutdown,
                            gint         interact_style,
                            gboolean     fast,
                            gboolean     global)
{
  xfsm_verbose ("entering");

  if (G_UNLIKELY (xfsm_client_get_state (client) != XFSM_CLIENT_IDLE))
    {


      xfsm_verbose ("Client Id = %s, requested SAVE YOURSELF, but client is not in IDLE mode.\n"
                    "   Client will be nuked now.\n\n",
                    xfsm_client_get_id (client));
      xfsm_manager_close_connection (manager, client, TRUE);
      return;
    }
  else if (G_UNLIKELY (manager->state != XFSM_MANAGER_IDLE))
    {
      xfsm_verbose ("Client Id = %s, requested SAVE YOURSELF, but manager is not in IDLE mode.\n"
                    "   Client will be nuked now.\n\n",
                    xfsm_client_get_id (client));
      xfsm_manager_close_connection (manager, client, TRUE);
      return;
    }

  if (!global)
    {
      SmsConn sms = xfsm_client_get_sms_connection (client);
      /* client requests a local checkpoint. We slightly ignore
       * shutdown here, since it does not make sense for a local
       * checkpoint.
       */
      if (sms != NULL)
        {
          SmsSaveYourself (sms, save_type, FALSE, interact_style, fast);
        }
      xfsm_client_set_state (client, XFSM_CLIENT_SAVINGLOCAL);
      xfsm_manager_start_client_save_timeout (manager, client);
    }
  else
    xfsm_manager_save_yourself_global (manager, save_type, shutdown, interact_style, fast, XFSM_SHUTDOWN_ASK, TRUE);
}


void
xfsm_manager_save_yourself_phase2 (XfsmManager *manager,
                                   XfsmClient *client)
{
  xfsm_verbose ("entering");

  if (manager->state != XFSM_MANAGER_CHECKPOINT && manager->state != XFSM_MANAGER_SHUTDOWN)
    {
      SmsConn sms = xfsm_client_get_sms_connection (client);
      if (sms != NULL)
        {
          SmsSaveYourselfPhase2 (sms);
        }
      xfsm_client_set_state (client, XFSM_CLIENT_SAVINGLOCAL);
      xfsm_manager_start_client_save_timeout (manager, client);
    }
  else
    {
      xfsm_client_set_state (client, XFSM_CLIENT_WAITFORPHASE2);
      xfsm_manager_cancel_client_save_timeout (manager, client);

      if (!xfsm_manager_check_clients_saving (manager))
        xfsm_manager_maybe_enter_phase2 (manager);
    }
}


void
xfsm_manager_save_yourself_done (XfsmManager *manager,
                                 XfsmClient  *client,
                                 gboolean     success)
{
  xfsm_verbose ("entering");

  /* In xfsm_manager_interact_done we send SmsShutdownCancelled to clients in
     XFSM_CLIENT_WAITFORINTERACT state. They respond with SmcSaveYourselfDone
     (xsmp_shutdown_cancelled in libxfce4ui library) so we allow it here. */
  if (xfsm_client_get_state (client) != XFSM_CLIENT_SAVING &&
      xfsm_client_get_state (client) != XFSM_CLIENT_SAVINGLOCAL &&
      xfsm_client_get_state (client) != XFSM_CLIENT_WAITFORINTERACT)
    {
      xfsm_verbose ("Client Id = %s send SAVE YOURSELF DONE, while not being "
                    "in save mode. Prepare to be nuked!\n",
                    xfsm_client_get_id (client));

      xfsm_manager_close_connection (manager, client, TRUE);
    }

  /* remove client save timeout, as client responded in time */
  xfsm_manager_cancel_client_save_timeout (manager, client);

  if (xfsm_client_get_state (client) == XFSM_CLIENT_SAVINGLOCAL)
    {
      SmsConn sms = xfsm_client_get_sms_connection (client);
      /* client completed local SaveYourself */
      xfsm_client_set_state (client, XFSM_CLIENT_IDLE);
      if (sms != NULL)
        {
          SmsSaveComplete (sms);
        }
    }
  else if (manager->state != XFSM_MANAGER_CHECKPOINT && manager->state != XFSM_MANAGER_SHUTDOWN)
    {
      xfsm_verbose ("Client Id = %s, send SAVE YOURSELF DONE, but manager is not in CheckPoint/Shutdown mode.\n"
                    "   Client will be nuked now.\n\n",
                    xfsm_client_get_id (client));
      xfsm_manager_close_connection (manager, client, TRUE);
    }
  else
    {
      xfsm_client_set_state (client, XFSM_CLIENT_SAVEDONE);
      xfsm_manager_complete_saveyourself (manager);
    }
}


void
xfsm_manager_close_connection (XfsmManager *manager,
                               XfsmClient  *client,
                               gboolean     cleanup)
{
  IceConn ice_conn;
  GList *lp;

  xfsm_client_set_state (client, XFSM_CLIENT_DISCONNECTED);
  xfsm_manager_cancel_client_save_timeout (manager, client);

  if (cleanup)
    {
      SmsConn sms_conn = xfsm_client_get_sms_connection (client);
      if (sms_conn != NULL)
        {
          ice_conn = SmsGetIceConnection (sms_conn);
          SmsCleanUp (sms_conn);
          IceSetShutdownNegotiation (ice_conn, False);
          IceCloseConnection (ice_conn);
        }
      else
        {
          xfsm_client_terminate (client);
        }
    }

  if (manager->state == XFSM_MANAGER_SHUTDOWNPHASE2)
    {
      for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
           lp;
           lp = lp->next)
        {
          XfsmClient *cl = lp->data;
          if (xfsm_client_get_state (cl) != XFSM_CLIENT_DISCONNECTED)
            return;
        }

      /* all clients finished the DIE phase in time */
      if (manager->die_timeout_id)
        {
          g_source_remove (manager->die_timeout_id);
          manager->die_timeout_id = 0;
        }
      g_signal_emit(G_OBJECT(manager), manager_signals[MANAGER_QUIT], 0);
    }
  else if (manager->state == XFSM_MANAGER_SHUTDOWN || manager->state == XFSM_MANAGER_CHECKPOINT)
    {
      xfsm_verbose ("Client Id = %s, closed connection in checkpoint state\n"
                    "   Session manager will show NO MERCY\n\n",
                    xfsm_client_get_id (client));

      /* stupid client disconnected in CheckPoint state, prepare to be nuked! */
      g_queue_remove (manager->running_clients, client);
      g_object_unref (client);
      xfsm_manager_complete_saveyourself (manager);
    }
  else
    {
      XfsmProperties *properties = xfsm_client_steal_properties (client);

      if (properties != NULL)
        {
          if (xfsm_properties_check (properties))
            {
              if (xfsm_manager_handle_failed_properties (manager, properties) == FALSE)
                xfsm_properties_free (properties);
            }
          else
            xfsm_properties_free (properties);
        }

      /* regardless of the restart style hint, the current instance of
       * the client is gone, so remove it from the client list and free it. */
      g_queue_remove (manager->running_clients, client);
      g_object_unref (client);
    }
}


void
xfsm_manager_close_connection_by_ice_conn (XfsmManager *manager,
                                           IceConn      ice_conn)
{
  GList *lp;

  for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
       lp;
       lp = lp->next)
    {
      XfsmClient *client = lp->data;
      SmsConn sms = xfsm_client_get_sms_connection (client);

      if (sms != NULL && SmsGetIceConnection (sms) == ice_conn)
        {
          xfsm_manager_close_connection (manager, client, FALSE);
          break;
        }
    }

  /* be sure to close the Ice connection in any case */
  IceSetShutdownNegotiation (ice_conn, False);
  IceCloseConnection (ice_conn);
}


gboolean
xfsm_manager_terminate_client (XfsmManager *manager,
                               XfsmClient  *client,
                               GError **error)
{
  SmsConn sms = xfsm_client_get_sms_connection (client);

  if (manager->state != XFSM_MANAGER_IDLE
      || xfsm_client_get_state (client) != XFSM_CLIENT_IDLE)
    {
      if (error)
        {
          g_set_error (error, XFSM_ERROR, XFSM_ERROR_BAD_STATE,
                       _("Can only terminate clients when in the idle state"));
        }
      return FALSE;
    }

  if (sms != NULL)
    {
      SmsDie (sms);
    }
  else
    {
      xfsm_client_terminate (client);
    }

  return TRUE;
}


static gboolean
manager_quit_signal (XfsmManager *manager)
{
  g_signal_emit(G_OBJECT(manager), manager_signals[MANAGER_QUIT], 0);
  return FALSE;
}


void
xfsm_manager_perform_shutdown (XfsmManager *manager)
{
  GList *lp;

  xfsm_verbose ("entering\n");

  /* send SmDie message to all clients */
  xfsm_manager_set_state (manager, XFSM_MANAGER_SHUTDOWNPHASE2);
  for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
       lp;
       lp = lp->next)
    {
      XfsmClient *client = lp->data;
      SmsConn sms = xfsm_client_get_sms_connection (client);
      if (sms != NULL)
        {
          SmsDie (sms);
        }
      else
        {
          xfsm_client_end_session (client);
        }
    }

  /* check for SmRestartAnyway clients that have already quit and
   * set a ShutdownCommand */
  for (lp = g_queue_peek_nth_link (manager->restart_properties, 0);
       lp;
       lp = lp->next)
    {
      XfsmProperties *properties = lp->data;
      gint            restart_style_hint;
      gchar         **shutdown_command;

      restart_style_hint = xfsm_properties_get_uchar (properties,
                                                      SmRestartStyleHint,
                                                      SmRestartIfRunning);
      shutdown_command = xfsm_properties_get_strv (properties, SmShutdownCommand);

      if (restart_style_hint == SmRestartAnyway && shutdown_command != NULL)
        {
          xfsm_verbose ("Client Id = %s, quit already, running shutdown command.\n\n",
                        properties->client_id);

          g_spawn_sync (xfsm_properties_get_string (properties, SmCurrentDirectory),
                        shutdown_command,
                        xfsm_properties_get_strv (properties, SmEnvironment),
                        G_SPAWN_SEARCH_PATH,
                        NULL, NULL,
                        NULL, NULL,
                        NULL, NULL);
        }
    }

  /* give all clients the chance to close the connection */
  manager->die_timeout_id = g_timeout_add (DIE_TIMEOUT,
                                           (GSourceFunc) manager_quit_signal,
                                           manager);
}


gboolean
xfsm_manager_check_clients_saving (XfsmManager *manager)
{
  GList *lp;

  for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
       lp;
       lp = lp->next)
    {
      XfsmClient *client = lp->data;
      XfsmClientState state = xfsm_client_get_state (client);
      switch (state)
        {
          case XFSM_CLIENT_SAVING:
          case XFSM_CLIENT_WAITFORINTERACT:
          case XFSM_CLIENT_INTERACTING:
            return TRUE;
          default:
            break;
        }
    }

  return FALSE;
}


gboolean
xfsm_manager_maybe_enter_phase2 (XfsmManager *manager)
{
  gboolean entered_phase2 = FALSE;
  GList *lp;

  for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
       lp;
       lp = lp->next)
    {
      XfsmClient *client = lp->data;

      if (xfsm_client_get_state (client) == XFSM_CLIENT_WAITFORPHASE2)
        {
          SmsConn sms = xfsm_client_get_sms_connection (client);
          entered_phase2 = TRUE;

          if (sms != NULL)
            {
              SmsSaveYourselfPhase2 (sms);
            }

          xfsm_client_set_state (client, XFSM_CLIENT_SAVING);
          xfsm_manager_start_client_save_timeout (manager, client);

          xfsm_verbose ("Client Id = %s enters SAVE YOURSELF PHASE2.\n\n",
                        xfsm_client_get_id (client));
        }
    }

  return entered_phase2;
}


void
xfsm_manager_complete_saveyourself (XfsmManager *manager)
{
  GList *lp;

  /* Check if still clients in SAVING state or if we have to enter PHASE2
   * now. In either case, SaveYourself cannot be completed in this run.
   */
  if (xfsm_manager_check_clients_saving (manager) || xfsm_manager_maybe_enter_phase2 (manager))
    return;

  xfsm_verbose ("Manager finished SAVE YOURSELF, session data will be stored now.\n\n");

  /* all clients done, store session data */
  if (manager->save_session)
    xfsm_manager_store_session (manager);

  if (manager->state == XFSM_MANAGER_CHECKPOINT)
    {
      /* reset all clients to idle state */
      xfsm_manager_set_state (manager, XFSM_MANAGER_IDLE);
      for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
           lp;
           lp = lp->next)
        {
          XfsmClient *client = lp->data;
          SmsConn sms = xfsm_client_get_sms_connection (client);

          xfsm_client_set_state (client, XFSM_CLIENT_IDLE);
          if (sms != NULL)
            {
              SmsSaveComplete (sms);
            }
        }
    }
  else
    {
      /* shutdown the session */
      xfsm_manager_perform_shutdown (manager);
    }
}


static gboolean
xfsm_manager_save_timeout (gpointer user_data)
{
  XfsmSaveTimeoutData *stdata = user_data;

  xfsm_verbose ("Client id = %s, received SAVE TIMEOUT\n"
                "   Client will be disconnected now.\n\n",
                xfsm_client_get_id (stdata->client));

  /* returning FALSE below will free the data */
  g_object_steal_data (G_OBJECT (stdata->client), "--save-timeout-id");

  xfsm_manager_close_connection (stdata->manager, stdata->client, TRUE);

  return FALSE;
}


static void
xfsm_manager_start_client_save_timeout (XfsmManager *manager,
                                        XfsmClient  *client)
{
  XfsmSaveTimeoutData *sdata = g_new(XfsmSaveTimeoutData, 1);

  sdata->manager = manager;
  sdata->client = client;
  /* |sdata| will get freed when the source gets removed */
  sdata->timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT, SAVE_TIMEOUT,
                                          xfsm_manager_save_timeout,
                                          sdata, (GDestroyNotify) g_free);
  /* ... or, if the object gets destroyed first, the source will get
   * removed and will free |sdata| for us.  also, if there's a pending
   * timer, this call will clear it. */
  g_object_set_data_full (G_OBJECT (client), "--save-timeout-id",
                          GUINT_TO_POINTER (sdata->timeout_id),
                          (GDestroyNotify) G_CALLBACK (g_source_remove));
}


static void
xfsm_manager_cancel_client_save_timeout (XfsmManager *manager,
                                         XfsmClient  *client)
{
  /* clearing out the data will call g_source_remove(), which will free it */
  g_object_set_data (G_OBJECT (client), "--save-timeout-id", NULL);
}


void
xfsm_manager_store_session (XfsmManager *manager)
{
  WnckWorkspace *workspace;
  GdkDisplay    *display;
  WnckScreen    *screen;
  XfceRc        *rc;
  GList         *lp;
  gchar          prefix[64];
  gchar         *backup;
  gchar         *group;
  gint           count = 0;
  gint           n, m;

  /* open file for writing, creates it if it doesn't exist */
  rc = xfce_rc_simple_open (manager->session_file, FALSE);
  if (G_UNLIKELY (rc == NULL))
    {
      fprintf (stderr,
               "xfce4-session: Unable to open session file %s for "
               "writing. Session data will not be stored. Please check "
               "your installation.\n",
               manager->session_file);
      return;
    }

  /* backup the old session file first */
  if (g_file_test (manager->session_file, G_FILE_TEST_IS_REGULAR))
    {
      backup = g_strconcat (manager->session_file, ".bak", NULL);
      unlink (backup);
      if (link (manager->session_file, backup))
          g_warning ("Failed to create session file backup");
      g_free (backup);
    }

  if (manager->state == XFSM_MANAGER_CHECKPOINT && manager->checkpoint_session_name != NULL)
    group = g_strconcat ("Session: ", manager->checkpoint_session_name, NULL);
  else
    group = g_strconcat ("Session: ", manager->session_name, NULL);
  xfce_rc_delete_group (rc, group, TRUE);
  xfce_rc_set_group (rc, group);
  g_free (group);

  for (lp = g_queue_peek_nth_link (manager->restart_properties, 0);
       lp;
       lp = lp->next)
    {
      XfsmProperties *properties = lp->data;
      g_snprintf (prefix, 64, "Client%d_", count);
      xfsm_properties_store (properties, rc, prefix);
      ++count;
    }

  for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
       lp;
       lp = lp->next)
    {
      XfsmClient     *client     = lp->data;
      XfsmProperties *properties = xfsm_client_get_properties (client);
      gint            restart_style_hint;

      if (properties == NULL || !xfsm_properties_check (xfsm_client_get_properties (client)))
        continue;
      restart_style_hint = xfsm_properties_get_uchar (properties,
                                                      SmRestartStyleHint,
                                                      SmRestartIfRunning);
      if (restart_style_hint == SmRestartNever)
        continue;

      g_snprintf (prefix, 64, "Client%d_", count);
      xfsm_properties_store (xfsm_client_get_properties (client), rc, prefix);
      ++count;
    }

  xfce_rc_write_int_entry (rc, "Count", count);

  /* store legacy applications state */
  xfsm_legacy_store_session (rc);

  /* store current workspace numbers */
  display = gdk_display_get_default ();
  for (n = 0; n < XScreenCount (gdk_x11_display_get_xdisplay (display)); ++n)
    {
      screen = wnck_screen_get (n);
      wnck_screen_force_update (screen);

      workspace = wnck_screen_get_active_workspace (screen);
      m = wnck_workspace_get_number (workspace);

      g_snprintf (prefix, 64, "Screen%d_ActiveWorkspace", n);
      xfce_rc_write_int_entry (rc, prefix, m);
    }

  /* remember time */
  xfce_rc_write_int_entry (rc, "LastAccess", time (NULL));

  xfce_rc_close (rc);

  g_free (manager->checkpoint_session_name);
  manager->checkpoint_session_name = NULL;
}


XfsmShutdownType
xfsm_manager_get_shutdown_type (XfsmManager *manager)
{
  return manager->shutdown_type;
}


XfsmManagerState
xfsm_manager_get_state (XfsmManager *manager)
{
  return manager->state;
}


GQueue *
xfsm_manager_get_queue (XfsmManager         *manager,
                        XfsmManagerQueueType q_type)
{
  switch(q_type)
    {
      case XFSM_MANAGER_QUEUE_PENDING_PROPS:
        return manager->pending_properties;
      case XFSM_MANAGER_QUEUE_STARTING_PROPS:
        return manager->starting_properties;
      case XFSM_MANAGER_QUEUE_RESTART_PROPS:
        return manager->restart_properties;
      case XFSM_MANAGER_QUEUE_RUNNING_CLIENTS:
        return manager->running_clients;
      case XFSM_MANAGER_QUEUE_FAILSAFE_CLIENTS:
        return manager->failsafe_clients;
      default:
        g_warning ("Requested invalid queue type %d", (gint)q_type);
        return NULL;
    }
}


gboolean
xfsm_manager_get_use_failsafe_mode (XfsmManager *manager)
{
  return manager->failsafe_mode;
}


gboolean
xfsm_manager_get_compat_startup (XfsmManager          *manager,
                                 XfsmManagerCompatType type)
{
  switch (type)
    {
      case XFSM_MANAGER_COMPAT_GNOME:
        return manager->compat_gnome;
      case XFSM_MANAGER_COMPAT_KDE:
        return manager->compat_kde;
      default:
        g_warning ("Invalid compat startup type %d", type);
        return FALSE;
    }
}


gboolean
xfsm_manager_get_start_at (XfsmManager *manager)
{
  return manager->start_at;
}


/*
 * dbus server impl
 */

static gboolean xfsm_manager_dbus_get_info (XfsmDbusManager *object,
                                            GDBusMethodInvocation *invocation);
static gboolean xfsm_manager_dbus_list_clients (XfsmDbusManager *object,
                                                GDBusMethodInvocation *invocation);
static gboolean xfsm_manager_dbus_get_state (XfsmDbusManager *object,
                                             GDBusMethodInvocation *invocation);
static gboolean xfsm_manager_dbus_checkpoint (XfsmDbusManager *object,
                                              GDBusMethodInvocation *invocation,
                                              const gchar *arg_session_name);
static gboolean xfsm_manager_dbus_logout (XfsmDbusManager *object,
                                          GDBusMethodInvocation *invocation,
                                          gboolean arg_show_dialog,
                                          gboolean arg_allow_save);
static gboolean xfsm_manager_dbus_shutdown (XfsmDbusManager *object,
                                            GDBusMethodInvocation *invocation,
                                            gboolean arg_allow_save);
static gboolean xfsm_manager_dbus_can_shutdown (XfsmDbusManager *object,
                                                GDBusMethodInvocation *invocation);
static gboolean xfsm_manager_dbus_restart (XfsmDbusManager *object,
                                           GDBusMethodInvocation *invocation,
                                           gboolean arg_allow_save);
static gboolean xfsm_manager_dbus_can_restart (XfsmDbusManager *object,
                                               GDBusMethodInvocation *invocation);
static gboolean xfsm_manager_dbus_suspend (XfsmDbusManager *object,
                                           GDBusMethodInvocation *invocation);
static gboolean xfsm_manager_dbus_can_suspend (XfsmDbusManager *object,
                                               GDBusMethodInvocation *invocation);
static gboolean xfsm_manager_dbus_hibernate (XfsmDbusManager *object,
                                             GDBusMethodInvocation *invocation);
static gboolean xfsm_manager_dbus_can_hibernate (XfsmDbusManager *object,
                                                 GDBusMethodInvocation *invocation);
static gboolean xfsm_manager_dbus_hybrid_sleep (XfsmDbusManager *object,
                                                GDBusMethodInvocation *invocation);
static gboolean xfsm_manager_dbus_can_hybrid_sleep (XfsmDbusManager *object,
                                                    GDBusMethodInvocation *invocation);
static gboolean xfsm_manager_dbus_switch_user (XfsmDbusManager *object,
                                               GDBusMethodInvocation *invocation);
static gboolean xfsm_manager_dbus_register_client (XfsmDbusManager *object,
                                                   GDBusMethodInvocation *invocation,
                                                   const gchar *arg_app_id,
                                                   const gchar *arg_client_startup_id);
static gboolean xfsm_manager_dbus_unregister_client (XfsmDbusManager *object,
                                                     GDBusMethodInvocation *invocation,
                                                     const gchar *arg_client_id);


/* eader needs the above fwd decls */
#include <xfce4-session/xfsm-manager-dbus.h>


static void
remove_clients_for_connection (XfsmManager *manager,
                               const gchar *service_name)
{
  GList       *lp;

  for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
       lp;
       lp = lp->next)
    {
      XfsmClient *client = XFSM_CLIENT (lp->data);
      if (g_strcmp0 (xfsm_client_get_service_name (client), service_name) == 0)
        {
          xfsm_manager_close_connection (manager, client, FALSE);
        }
    }
}

static void
on_name_owner_notify (GDBusConnection *connection,
                      const gchar     *sender_name,
                      const gchar     *object_path,
                      const gchar     *interface_name,
                      const gchar     *signal_name,
                      GVariant        *parameters,
                      gpointer         user_data)
{
        XfsmManager *manager = XFSM_MANAGER (user_data);
        gchar       *service_name,
                    *old_service_name,
                    *new_service_name;

        g_variant_get (parameters, "(sss)", &service_name, &old_service_name, &new_service_name);

        if (strlen (new_service_name) == 0) {
                remove_clients_for_connection (manager, old_service_name);
        }
}

static void
xfsm_manager_dbus_class_init (XfsmManagerClass *klass)
{
}


static void
xfsm_manager_dbus_init (XfsmManager *manager, GDBusConnection *connection)
{
  GError *error = NULL;

  g_return_if_fail (XFSM_IS_MANAGER (manager));

  manager->connection = g_object_ref (connection);

  g_debug ("exporting path /org/xfce/SessionManager");

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (XFSM_DBUS_MANAGER (manager)),
                                         manager->connection,
                                         "/org/xfce/SessionManager",
                                         &error)) {
    if (error != NULL) {
            g_critical ("error exporting interface: %s", error->message);
            g_clear_error (&error);
            return;
    }
  }

  g_debug ("exported on %s", g_dbus_interface_skeleton_get_object_path (G_DBUS_INTERFACE_SKELETON (XFSM_DBUS_MANAGER (manager))));

  manager->name_owner_id = g_dbus_connection_signal_subscribe (manager->connection,
                                                               "org.freedesktop.DBus",
                                                               "org.freedesktop.DBus",
                                                               "NameOwnerChanged",
                                                               "/org/freedesktop/DBus",
                                                               NULL,
                                                               G_DBUS_SIGNAL_FLAGS_NONE,
                                                               on_name_owner_notify,
                                                               manager,
                                                               NULL);
}


static void
xfsm_manager_iface_init (XfsmDbusManagerIface *iface)
{
  iface->handle_can_hibernate = xfsm_manager_dbus_can_hibernate;
  iface->handle_can_hybrid_sleep = xfsm_manager_dbus_can_hybrid_sleep;
  iface->handle_can_restart = xfsm_manager_dbus_can_restart;
  iface->handle_can_shutdown = xfsm_manager_dbus_can_shutdown;
  iface->handle_can_suspend = xfsm_manager_dbus_can_suspend;
  iface->handle_checkpoint = xfsm_manager_dbus_checkpoint;
  iface->handle_get_info = xfsm_manager_dbus_get_info;
  iface->handle_get_state = xfsm_manager_dbus_get_state;
  iface->handle_hibernate = xfsm_manager_dbus_hibernate;
  iface->handle_hybrid_sleep = xfsm_manager_dbus_hybrid_sleep;
  iface->handle_switch_user = xfsm_manager_dbus_switch_user;
  iface->handle_list_clients = xfsm_manager_dbus_list_clients;
  iface->handle_logout = xfsm_manager_dbus_logout;
  iface->handle_restart = xfsm_manager_dbus_restart;
  iface->handle_shutdown = xfsm_manager_dbus_shutdown;
  iface->handle_suspend = xfsm_manager_dbus_suspend;
  iface->handle_register_client = xfsm_manager_dbus_register_client;
  iface->handle_unregister_client = xfsm_manager_dbus_unregister_client;
}

static void
xfsm_manager_dbus_cleanup (XfsmManager *manager)
{
  if (manager->name_owner_id > 0 && manager->connection)
    {
      g_dbus_connection_signal_unsubscribe (manager->connection, manager->name_owner_id);
      manager->name_owner_id = 0;
    }

  if (G_LIKELY (manager->connection))
    {
      g_object_unref (manager->connection);
      manager->connection = NULL;
    }
}


static gboolean
xfsm_manager_dbus_get_info (XfsmDbusManager *object,
                            GDBusMethodInvocation *invocation)
{
  xfsm_dbus_manager_complete_get_info (object, invocation, PACKAGE, VERSION, "Xfce");
  return TRUE;
}


static gboolean
xfsm_manager_dbus_list_clients (XfsmDbusManager *object,
                                GDBusMethodInvocation *invocation)
{
  XfsmManager *manager = XFSM_MANAGER(object);
  GList  *lp;
  gint    i = 0;
  gint    num_clients;
  gchar **clients;

  num_clients = g_queue_get_length (manager->running_clients);
  clients = g_new0 (gchar*, num_clients + 1);
  clients[num_clients] = NULL;

  for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
       lp;
       lp = lp->next)
    {
      XfsmClient *client = XFSM_CLIENT (lp->data);
      gchar *object_path = g_strdup (xfsm_client_get_object_path (client));
      clients[i] = object_path;
      i++;
    }

  xfsm_dbus_manager_complete_list_clients (object, invocation, (const gchar * const*)clients);
  g_strfreev (clients);
  return TRUE;
}


static gboolean
xfsm_manager_dbus_get_state (XfsmDbusManager *object,
                             GDBusMethodInvocation *invocation)
{
  xfsm_dbus_manager_complete_get_state (object, invocation, XFSM_MANAGER(object)->state);
  return TRUE;
}


static gboolean
xfsm_manager_dbus_checkpoint_idled (gpointer data)
{
  XfsmManager *manager = XFSM_MANAGER (data);

  xfsm_manager_save_yourself_global (manager, SmSaveBoth, FALSE,
                                     SmInteractStyleNone, FALSE,
                                     XFSM_SHUTDOWN_ASK, TRUE);

  return FALSE;
}


static gboolean
xfsm_manager_dbus_checkpoint (XfsmDbusManager *object,
                              GDBusMethodInvocation *invocation,
                              const gchar *arg_session_name)
{
  XfsmManager *manager = XFSM_MANAGER(object);

  if (manager->state != XFSM_MANAGER_IDLE)
    {
      throw_error (invocation, XFSM_ERROR_BAD_STATE, _("Session manager must be in idle state when requesting a checkpoint"));
      return TRUE;
    }

  g_free (manager->checkpoint_session_name);
  if (arg_session_name[0] != '\0')
    manager->checkpoint_session_name = g_strdup (arg_session_name);
  else
    manager->checkpoint_session_name = NULL;

  /* idle so the dbus call returns in the client */
  g_idle_add (xfsm_manager_dbus_checkpoint_idled, manager);

  xfsm_dbus_manager_complete_checkpoint (object, invocation);
  return TRUE;
}


static gboolean
xfsm_manager_dbus_shutdown_idled (gpointer data)
{
  ShutdownIdleData *idata = data;

  xfsm_manager_save_yourself_global (idata->manager, SmSaveBoth, TRUE,
                                     SmInteractStyleAny, FALSE,
                                     idata->type, idata->allow_save);

  return FALSE;
}


static gboolean
xfsm_manager_save_yourself_dbus (XfsmManager       *manager,
                                 XfsmShutdownType   type,
                                 gboolean           allow_save)
{
  ShutdownIdleData *idata;

  if (manager->state != XFSM_MANAGER_IDLE)
    {
      return FALSE;
    }

  idata = g_new (ShutdownIdleData, 1);
  idata->manager = manager;
  idata->type = type;
  idata->allow_save = allow_save;
  g_idle_add_full (G_PRIORITY_DEFAULT, xfsm_manager_dbus_shutdown_idled,
                   idata, (GDestroyNotify) g_free);

  return TRUE;
}


static gboolean
xfsm_manager_dbus_logout (XfsmDbusManager *object,
                          GDBusMethodInvocation *invocation,
                          gboolean arg_show_dialog,
                          gboolean arg_allow_save)
{
  XfsmShutdownType type;

  xfsm_verbose ("entering\n");

  g_return_val_if_fail (XFSM_IS_MANAGER (object), FALSE);

  type = arg_show_dialog ? XFSM_SHUTDOWN_ASK : XFSM_SHUTDOWN_LOGOUT;
  if (xfsm_manager_save_yourself_dbus (XFSM_MANAGER (object), type, arg_allow_save) == FALSE)
    {
      throw_error (invocation, XFSM_ERROR_BAD_STATE,
                   _("Session manager must be in idle state when requesting a shutdown"));
      return TRUE;
    }

  xfsm_dbus_manager_complete_logout (object, invocation);
  return TRUE;
}


static gboolean
xfsm_manager_dbus_shutdown (XfsmDbusManager *object,
                            GDBusMethodInvocation *invocation,
                            gboolean arg_allow_save)
{
  xfsm_verbose ("entering\n");

  g_return_val_if_fail (XFSM_IS_MANAGER (object), FALSE);
  if (xfsm_manager_save_yourself_dbus (XFSM_MANAGER (object), XFSM_SHUTDOWN_SHUTDOWN, arg_allow_save) == FALSE)
    {
      throw_error (invocation, XFSM_ERROR_BAD_STATE,
                   _("Session manager must be in idle state when requesting a shutdown"));
      return TRUE;
    }

  xfsm_dbus_manager_complete_shutdown (object, invocation);
  return TRUE;
}


static gboolean
xfsm_manager_dbus_can_shutdown (XfsmDbusManager *object,
                                GDBusMethodInvocation *invocation)
{
  gboolean can_shutdown = FALSE;
  GError *error = NULL;

  xfsm_verbose ("entering\n");

  g_return_val_if_fail (XFSM_IS_MANAGER (object), FALSE);

  xfsm_shutdown_can_shutdown (XFSM_MANAGER (object)->shutdown_helper, &can_shutdown, &error);

  if (error)
    {
      throw_error (invocation, XFSM_ERROR_BAD_STATE, error->message);
      g_clear_error(&error);
      return TRUE;
    }

  xfsm_dbus_manager_complete_can_shutdown (object, invocation, can_shutdown);
  return TRUE;
}


static gboolean
xfsm_manager_dbus_restart (XfsmDbusManager *object,
                           GDBusMethodInvocation *invocation,
                           gboolean arg_allow_save)
{
  xfsm_verbose ("entering\n");

  g_return_val_if_fail (XFSM_IS_MANAGER (object), FALSE);
  if (xfsm_manager_save_yourself_dbus (XFSM_MANAGER (object), XFSM_SHUTDOWN_RESTART, arg_allow_save) == FALSE)
    {
      throw_error (invocation, XFSM_ERROR_BAD_STATE,
                   _("Session manager must be in idle state when requesting a restart"));
      return TRUE;
    }

  xfsm_dbus_manager_complete_restart (object, invocation);
  return TRUE;
}


static gboolean
xfsm_manager_dbus_can_restart (XfsmDbusManager *object,
                               GDBusMethodInvocation *invocation)
{
  gboolean can_restart = FALSE;
  GError *error = NULL;

  xfsm_verbose ("entering\n");

  g_return_val_if_fail (XFSM_IS_MANAGER (object), FALSE);

  xfsm_shutdown_can_restart (XFSM_MANAGER (object)->shutdown_helper, &can_restart, &error);

  if (error)
    {
      throw_error (invocation, XFSM_ERROR_BAD_STATE, error->message);
      g_clear_error(&error);
      return TRUE;
    }

  xfsm_dbus_manager_complete_can_restart (object, invocation, can_restart);
  return TRUE;
}


static gboolean
xfsm_manager_dbus_suspend (XfsmDbusManager *object,
                           GDBusMethodInvocation *invocation)
{
  GError *error = NULL;

  xfsm_verbose ("entering\n");

  g_return_val_if_fail (XFSM_IS_MANAGER (object), FALSE);
  if (xfsm_shutdown_try_suspend (XFSM_MANAGER (object)->shutdown_helper, &error) == FALSE)
    {
      throw_error (invocation, XFSM_ERROR_BAD_STATE, error->message);
      g_clear_error (&error);
      return TRUE;
    }

  xfsm_dbus_manager_complete_suspend (object, invocation);
  return TRUE;
}


static gboolean
xfsm_manager_dbus_can_suspend (XfsmDbusManager *object,
                               GDBusMethodInvocation *invocation)
{
  gboolean auth_suspend = FALSE;
  gboolean can_suspend = FALSE;
  GError *error = NULL;

  xfsm_verbose ("entering\n");

  g_return_val_if_fail (XFSM_IS_MANAGER (object), FALSE);

  xfsm_shutdown_can_suspend (XFSM_MANAGER (object)->shutdown_helper, &can_suspend, &auth_suspend, &error);

  if (error)
    {
      throw_error (invocation, XFSM_ERROR_BAD_STATE, error->message);
      g_clear_error(&error);
      return TRUE;
    }

  if (!auth_suspend)
    can_suspend = FALSE;

  xfsm_dbus_manager_complete_can_suspend (object, invocation, can_suspend);
  return TRUE;
}

static gboolean
xfsm_manager_dbus_hibernate (XfsmDbusManager *object,
                             GDBusMethodInvocation *invocation)
{
  GError *error = NULL;

  xfsm_verbose ("entering\n");

  g_return_val_if_fail (XFSM_IS_MANAGER (object), FALSE);
  if (xfsm_shutdown_try_hibernate (XFSM_MANAGER (object)->shutdown_helper, &error) == FALSE)
    {
      throw_error (invocation, XFSM_ERROR_BAD_STATE, error->message);
      g_clear_error (&error);
      return TRUE;
    }

  xfsm_dbus_manager_complete_hibernate (object, invocation);
  return TRUE;
}

static gboolean
xfsm_manager_dbus_can_hibernate (XfsmDbusManager *object,
                                 GDBusMethodInvocation *invocation)
{
  gboolean auth_hibernate = FALSE;
  gboolean can_hibernate = FALSE;
  GError *error = NULL;

  xfsm_verbose ("entering\n");

  g_return_val_if_fail (XFSM_IS_MANAGER (object), FALSE);

  xfsm_shutdown_can_hibernate (XFSM_MANAGER (object)->shutdown_helper, &can_hibernate, &auth_hibernate, &error);

  if (error)
    {
      throw_error (invocation, XFSM_ERROR_BAD_STATE, error->message);
      g_clear_error(&error);
      return TRUE;
    }

  if (!auth_hibernate)
    can_hibernate = FALSE;

  xfsm_dbus_manager_complete_can_hibernate (object, invocation, can_hibernate);
  return TRUE;
}

static gboolean
xfsm_manager_dbus_hybrid_sleep (XfsmDbusManager *object,
                                GDBusMethodInvocation *invocation)
{
  GError *error = NULL;

  xfsm_verbose ("entering\n");

  g_return_val_if_fail (XFSM_IS_MANAGER (object), FALSE);
  if (xfsm_shutdown_try_hybrid_sleep (XFSM_MANAGER (object)->shutdown_helper, &error) == FALSE)
    {
      throw_error (invocation, XFSM_ERROR_BAD_STATE, error->message);
      g_clear_error (&error);
      return TRUE;
    }

  xfsm_dbus_manager_complete_hybrid_sleep (object, invocation);
  return TRUE;
}

static gboolean
xfsm_manager_dbus_can_hybrid_sleep (XfsmDbusManager *object,
                                    GDBusMethodInvocation *invocation)
{
  gboolean auth_hybrid_sleep = FALSE;
  gboolean can_hybrid_sleep = FALSE;
  GError *error = NULL;

  xfsm_verbose ("entering\n");

  g_return_val_if_fail (XFSM_IS_MANAGER (object), FALSE);

  xfsm_shutdown_can_hybrid_sleep (XFSM_MANAGER (object)->shutdown_helper, &can_hybrid_sleep, &auth_hybrid_sleep, &error);

  if (error)
    {
      throw_error (invocation, XFSM_ERROR_BAD_STATE, error->message);
      g_clear_error(&error);
      return TRUE;
    }

  if (!auth_hybrid_sleep)
    can_hybrid_sleep = FALSE;

  xfsm_dbus_manager_complete_can_hybrid_sleep (object, invocation, can_hybrid_sleep);
  return TRUE;
}

static gboolean
xfsm_manager_dbus_switch_user (XfsmDbusManager *object,
                               GDBusMethodInvocation *invocation)
{
  GError *error = NULL;

  xfsm_verbose ("entering\n");

  g_return_val_if_fail (XFSM_IS_MANAGER (object), FALSE);
  if (xfsm_shutdown_try_switch_user (XFSM_MANAGER (object)->shutdown_helper, &error) == FALSE)
    {
      throw_error (invocation, XFSM_ERROR_BAD_STATE, error->message);
      g_clear_error (&error);
      return TRUE;
    }

  xfsm_dbus_manager_complete_switch_user (object, invocation);
  return TRUE;
}



/* adapted from ConsoleKit2 whch was adapted from PolicyKit */
static gboolean
get_caller_info (XfsmManager *manager,
                 const char  *sender,
                 pid_t       *calling_pid)
{
        gboolean  res   = FALSE;
        GVariant *value = NULL;
        GError   *error = NULL;

        if (sender == NULL) {
                xfsm_verbose ("sender == NULL");
                goto out;
        }

        if (manager->connection == NULL) {
                xfsm_verbose ("manager->connection == NULL");
                goto out;
        }

        value = g_dbus_connection_call_sync (manager->connection,
                                             "org.freedesktop.DBus",
                                             "/org/freedesktop/DBus",
                                             "org.freedesktop.DBus",
                                             "GetConnectionUnixProcessID",
                                             g_variant_new ("(s)", sender),
                                             G_VARIANT_TYPE ("(u)"),
                                             G_DBUS_CALL_FLAGS_NONE,
                                             -1,
                                             NULL,
                                             &error);

        if (value == NULL) {
                xfsm_verbose ("GetConnectionUnixProcessID() failed: %s", error->message);
                g_error_free (error);
                goto out;
        }
        g_variant_get (value, "(u)", calling_pid);
        g_variant_unref (value);

        res = TRUE;

out:
        return res;
}



static gboolean
xfsm_manager_dbus_register_client (XfsmDbusManager *object,
                                   GDBusMethodInvocation *invocation,
                                   const gchar *arg_app_id,
                                   const gchar *arg_client_startup_id)
{
  XfsmManager *manager;
  XfsmClient  *client;
  gchar       *client_id;
  pid_t        pid = 0;

  manager = XFSM_MANAGER (object);

  if (arg_client_startup_id != NULL || (g_strcmp0 (arg_client_startup_id, "") == 0))
    {
      client_id = g_strdup_printf ("%s%s", arg_app_id, arg_client_startup_id);
    }
  else
    {
      client_id = g_strdup (arg_app_id);
    }

  /* create a new dbus-based client */
  client = xfsm_client_new (manager, NULL, manager->connection);

  /* register it so that it exports the dbus name */
  xfsm_manager_register_client (manager, client, client_id, NULL);

  /* save the app-id */
  xfsm_client_set_app_id (client, arg_app_id);

  /* attempt to get the caller'd pid */
  if (!get_caller_info (manager, g_dbus_method_invocation_get_sender (invocation), &pid))
    {
      pid = 0;
    }

  xfsm_client_set_pid (client, pid);

  /* we use the dbus service name to track clients so we know when they exit
   * or crash */
  xfsm_client_set_service_name (client, g_dbus_method_invocation_get_sender (invocation));

  xfsm_dbus_manager_complete_register_client (object, invocation, xfsm_client_get_object_path (client));
  g_free (client_id);
  return TRUE;
}



static gboolean
xfsm_manager_dbus_unregister_client (XfsmDbusManager *object,
                                     GDBusMethodInvocation *invocation,
                                     const gchar *arg_client_id)
{
  XfsmManager *manager;
  GList       *lp;

  manager = XFSM_MANAGER (object);


  for (lp = g_queue_peek_nth_link (manager->running_clients, 0);
       lp;
       lp = lp->next)
    {
      XfsmClient *client = XFSM_CLIENT (lp->data);
      if (g_strcmp0 (xfsm_client_get_object_path (client), arg_client_id) == 0)
        {
          xfsm_manager_close_connection (manager, client, FALSE);
          xfsm_dbus_manager_complete_unregister_client (object, invocation);
          return TRUE;
        }
    }


  throw_error (invocation, XFSM_ERROR_BAD_VALUE, "Client with id of '%s' was not found", arg_client_id);
  return TRUE;
}
