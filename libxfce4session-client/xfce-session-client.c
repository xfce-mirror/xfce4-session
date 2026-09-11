/*
 * Copyright (c) 2009 Brian Tarricone <brian@terricone.org>
 * Copyright (C) 1999 Olivier Fourdan <fourdan@xfce.org>
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

/**
 * SECTION:xfce-session-client
 * @title: XfceSessionClient
 * @short_description: Session management client
 * @stability: Stable
 * @include: libxfce4session-client/libxfce4session-client.h
 *
 * #XfceSessionClient is a session management client that supports both X11 and Wayland.
 *
 * On X11 it speaks the X Session Management Protocol (XSMP), and on Wayland it
 * uses a hybrid of xdg-session-management-v1 and xfce4-session's D-Bus
 * protocol.  It's designed to be easy to use and hide some of the more
 * esoteric features of the session management protocols from the API user.
 **/

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#endif

#ifdef ENABLE_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

#include "libxfce4session-client-enum-types.h"
#include "libxfce4session-client-marshal.h"
#include "xfce-session-client-dbus.h"
#include "xfce-session-client-private.h"
#include "xfce-session-client.h"
#include "libxfce4session-client-visibility.h"

#ifdef ENABLE_X11
#include "xfce-session-client-xsmp.h"
#endif

#ifdef ENABLE_WAYLAND
#include "xfce-session-client-wl-xdg.h"
#endif

#define SM_ID_ARG "--sm-client-id"
#ifdef ENABLE_X11
#define DPY_ARG "--display"
#endif

#define GET_PRIV(obj) ((XfceSessionClientPrivate *) xfce_session_client_get_instance_private (XFCE_SESSION_CLIENT (obj)))

/**
 * XfceSessionClientPriority:
 * @XFCE_SESSION_CLIENT_PRIORITY_HIGHEST: A high priority value.  You probably
 *                                   don't want to use this.
 * @XFCE_SESSION_CLIENT_PRIORITY_WM: A priority value for use by the window manager.
 * @XFCE_SESSION_CLIENT_PRIORITY_CORE: A priority value for use by applications that
 *                                place windows on the screen and possibly set
 *                                window manager struts.
 * @XFCE_SESSION_CLIENT_PRIORITY_DESKTOP: A priority value for use by applications
 *                                   that draw on the desktop.
 * @XFCE_SESSION_CLIENT_PRIORITY_DEFAULT: A priority value for regular applications.
 * @XFCE_SESSION_CLIENT_PRIORITY_LOWEST: The lowest possible priority value.
 *
 * Some sample priority values for use with xfce_session_client_set_priority().
 **/

/**
 * XfceSessionClientRestartStyle:
 * @XFCE_SESSION_CLIENT_RESTART_NORMAL: Only restart the application if it is
 *                                 still running when the session is next
 *                                 saved.
 * @XFCE_SESSION_CLIENT_RESTART_IMMEDIATELY: Immediately restart the application
 *                                      if it ever quits.
 *
 * An enumeration describing how the session manager should restart
 * the application.
 **/

/**
 * XfceSessionClientShutdownHint:
 * @XFCE_SESSION_CLIENT_SHUTDOWN_HINT_ASK: Prompt the user for a choice,
 * @XFCE_SESSION_CLIENT_SHUTDOWN_HINT_LOGOUT: End the current session,
 * @XFCE_SESSION_CLIENT_SHUTDOWN_HINT_HALT: Shut down the computer.
 * @XFCE_SESSION_CLIENT_SHUTDOWN_HINT_REBOOT: Restart the computer.
 *
 * Hints to the session manager what kind of shutdown the session manager
 * should perform.
 **/


typedef struct
{
  gint argc;
  gchar **argv;
  gchar *client_id;
  gboolean sm_disable;
} XfceSessionClientStartupOptions;

typedef struct _XfceSessionClientPrivate
{
  XfceSessionClientState state;
  XfceSessionClientRestartStyle restart_style;

  guint8 priority;

  gchar *client_id;

  gchar *current_directory;
  gchar **clone_command;
  gchar **restart_command;
  gchar **discard_command;

  guint32 resumed : 1;

  gchar *state_file;
  gchar *desktop_file;
} XfceSessionClientPrivate;

enum
{
  SIG_SAVE_STATE = 0,
  SIG_SAVE_STATE_EXTENDED,
  SIG_QUIT_REQUESTED,
  SIG_QUIT,
  SIG_QUIT_CANCELLED,
  N_SIGS
};

enum
{
  PROP_0 = 0,
  PROP_RESUMED,
  PROP_RESTART_STYLE,
  PROP_PRIORITY,
  PROP_CLIENT_ID,
  PROP_CURRENT_DIRECTORY,
  PROP_RESTART_COMMAND,
  PROP_CLONE_COMMAND,
  PROP_DESKTOP_FILE,
};

static void
xfce_session_client_get_property (GObject *obj,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec);
static void
xfce_session_client_set_property (GObject *obj,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec);
static GObject *
xfce_session_client_constructor (GType type,
                                 guint n_construct_params,
                                 GObjectConstructParam *construct_params);
static void
xfce_session_client_constructed (GObject *obj);

static void
xfce_session_client_finalize (GObject *obj);

static void
xfce_session_client_parse_argv (gint argc,
                                const gchar **argv,
                                gchar **client_id,
                                gchar ***restart_command,
                                gchar ***clone_command);


static guint signals[N_SIGS] = { 0 };
static XfceSessionClientStartupOptions startup_options = { 0, NULL, NULL, FALSE };
static XfceSessionClient *session_client_singleton = NULL;


G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (XfceSessionClient, xfce_session_client, G_TYPE_OBJECT)


static void
xfce_session_client_class_init (XfceSessionClientClass *klass)
{
  /* make sure to use the translations from libxfce4session-client */
  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->get_property = xfce_session_client_get_property;
  gobject_class->set_property = xfce_session_client_set_property;
  gobject_class->constructor = xfce_session_client_constructor;
  gobject_class->constructed = xfce_session_client_constructed;
  gobject_class->finalize = xfce_session_client_finalize;

  /**
   * XfceSessionClient::save-state:
   * @session_client: An #XfceSessionClient
   *
   * Signals the client that it should save a copy of its current state
   * such that it could be restarted later in exactly the same state as
   * it is at the time of signal emission.
   *
   * If the state is simple enough to be encoded in the application's
   * command line, xfce_session_client_set_restart_command() can be used
   * to set that command line.  For more complex state data,
   * xfce_session_client_get_state_file() should be used.
   *
   * The application should attempt to save its state as quickly as
   * possible, and MUST NOT interact with the user as a part of saving
   * state.
   **/
  signals[SIG_SAVE_STATE] = g_signal_new (I_ ("save-state"),
                                          G_TYPE_FROM_CLASS (klass),
                                          G_SIGNAL_RUN_LAST,
                                          0,
                                          NULL, NULL,
                                          g_cclosure_marshal_VOID__VOID,
                                          G_TYPE_NONE, 0);

  /**
   * XfceSessionClient::save-state-extended:
   * @session_client: An #XfceSessionClient
   *
   * Allows the application to save extra state information after all
   * other applications in the session have had a chance to save their
   * state.  This is mainly used by the window manager to save window
   * positions.  Most applications should not need to connect to this
   * signal.
   **/
  signals[SIG_SAVE_STATE_EXTENDED] = g_signal_new (I_ ("save-state-extended"),
                                                   G_TYPE_FROM_CLASS (klass),
                                                   G_SIGNAL_RUN_LAST,
                                                   0,
                                                   NULL, NULL,
                                                   g_cclosure_marshal_VOID__VOID,
                                                   G_TYPE_NONE, 0);

  /**
   * XfceSessionClient::quit-requested:
   * @session_client: An #XfceSessionClient
   *
   * Signals the client that the session manager will soon want the
   * application to quit, perhaps as a part of ending the session
   * (but this should not be assumed).  The application can take
   * this opportunity to prompt the user to save any unsaved work
   * to disk.
   *
   * This signal also expects a return value from the handler.  If the
   * application wishes to cancel the quit request (perhaps because the
   * user selected "Cancel" in prompts to save unsaved work), it should
   * return %TRUE from the handler.  If the application is satisfied
   * with possibly needing to quit soon, the handler should return %FALSE.
   **/
  signals[SIG_QUIT_REQUESTED] = g_signal_new (I_ ("quit-requested"),
                                              G_TYPE_FROM_CLASS (klass),
                                              G_SIGNAL_RUN_LAST,
                                              0,
                                              g_signal_accumulator_true_handled,
                                              NULL,
                                              _libxfce4session_client_marshal_BOOLEAN__VOID,
                                              G_TYPE_BOOLEAN, 0);

  /**
   * XfceSessionClient::quit:
   * @session_client: An #XfceSessionClient
   *
   * Emitted when the application is required to quit.  This is not
   * optional: if the client does not quit a short time after receiving
   * this signal, it will likely be terminated in some other way.  While
   * not required, the application will usually receive quit-requested
   * before receiving quit.  If the application does not connect to this
   * signal, #XfceSessionClient will call <function>exit(3)</function> with
   * an exit code of zero on behalf of the application.
   **/
  signals[SIG_QUIT] = g_signal_new (I_ ("quit"),
                                    G_TYPE_FROM_CLASS (klass),
                                    G_SIGNAL_RUN_LAST,
                                    0,
                                    NULL, NULL,
                                    g_cclosure_marshal_VOID__VOID,
                                    G_TYPE_NONE, 0);

  /**
   * XfceSessionClient::quit-cancelled:
   * @session_client: An #XfceSessionClient
   *
   * Informs the application that it will not need to quit.  In most cases,
   * quit-cancelled will be emitted a short time after quit-requested.
   **/
  signals[SIG_QUIT_CANCELLED] = g_signal_new (I_ ("quit-cancelled"),
                                              G_TYPE_FROM_CLASS (klass),
                                              G_SIGNAL_RUN_LAST,
                                              0,
                                              NULL, NULL,
                                              g_cclosure_marshal_VOID__VOID,
                                              G_TYPE_NONE, 0);

  g_object_class_install_property (gobject_class, PROP_RESUMED,
                                   g_param_spec_boolean ("resumed",
                                                         "Resumed",
                                                         "Whether or not the client was resumed with previous state",
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_RESTART_STYLE,
                                   g_param_spec_enum ("restart-style",
                                                      "Restart style",
                                                      "Specifies how the client should be restarted by the session manager",
                                                      XFCE_TYPE_SESSION_CLIENT_RESTART_STYLE,
                                                      XFCE_SESSION_CLIENT_RESTART_NORMAL,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PRIORITY,
                                   g_param_spec_uchar ("priority",
                                                       "Priority",
                                                       "Determines the ordering in which this client is restarted",
                                                       0, G_MAXUINT8,
                                                       XFCE_SESSION_CLIENT_PRIORITY_DEFAULT,
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CLIENT_ID,
                                   g_param_spec_string ("client-id",
                                                        "Client ID",
                                                        "A string uniquely identifying the current instance of this client",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CURRENT_DIRECTORY,
                                   g_param_spec_string ("current-directory",
                                                        "Current working directory",
                                                        "The directory that should be used as the working directory the next time this client is restarted",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_RESTART_COMMAND,
                                   g_param_spec_boxed ("restart-command",
                                                       "Restart command",
                                                       "A command used to restart this application, preserving the current state",
                                                       G_TYPE_STRV,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CLONE_COMMAND,
                                   g_param_spec_boxed ("clone-command",
                                                       "Clone command",
                                                       "A command used to start a new instance of this application",
                                                       G_TYPE_STRV,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DESKTOP_FILE,
                                   g_param_spec_string ("desktop-file",
                                                        "Desktop file",
                                                        "The application's .desktop file",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
xfce_session_client_init (XfceSessionClient *session_client)
{
  GET_PRIV (session_client)->current_directory = g_strdup (xfce_get_homedir ());
}

static void
xfce_session_client_get_property (GObject *obj,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
  XfceSessionClient *session_client = XFCE_SESSION_CLIENT (obj);
  XfceSessionClientPrivate *priv = GET_PRIV (session_client);

  switch (property_id)
    {
    case PROP_RESUMED:
      g_value_set_boolean (value, priv->resumed);
      break;

    case PROP_RESTART_STYLE:
      g_value_set_enum (value, priv->restart_style);
      break;

    case PROP_PRIORITY:
      g_value_set_uchar (value, priv->priority);
      break;

    case PROP_CLIENT_ID:
      g_value_set_string (value, priv->client_id);
      break;

    case PROP_CURRENT_DIRECTORY:
      g_value_set_string (value, priv->current_directory);
      break;

    case PROP_RESTART_COMMAND:
      g_value_set_boxed (value, priv->restart_command);
      break;

    case PROP_CLONE_COMMAND:
      g_value_set_boxed (value, priv->clone_command);
      break;

    case PROP_DESKTOP_FILE:
      g_value_set_string (value, priv->desktop_file);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
    }
}

static void
xfce_session_client_set_property (GObject *obj,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
  XfceSessionClient *session_client = XFCE_SESSION_CLIENT (obj);

  switch (property_id)
    {
    case PROP_RESTART_STYLE:
      xfce_session_client_set_restart_style (session_client,
                                             g_value_get_enum (value));
      break;

    case PROP_PRIORITY:
      xfce_session_client_set_priority (session_client, g_value_get_uchar (value));
      break;

    case PROP_CLIENT_ID:
      _xfce_session_client_set_client_id (session_client, g_value_get_string (value));
      break;

    case PROP_CURRENT_DIRECTORY:
      xfce_session_client_set_current_directory (session_client,
                                                 g_value_get_string (value));
      break;

    case PROP_RESTART_COMMAND:
      xfce_session_client_set_restart_command (session_client,
                                               g_value_get_boxed (value));
      break;

    case PROP_CLONE_COMMAND:
      xfce_session_client_set_clone_command (session_client,
                                             g_value_get_boxed (value));
      break;

    case PROP_DESKTOP_FILE:
      xfce_session_client_set_desktop_file (session_client,
                                            g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
    }
}

static GObject *
xfce_session_client_constructor (GType type,
                                 guint n_construct_params,
                                 GObjectConstructParam *construct_params)
{
  if (session_client_singleton != NULL)
    {
      return G_OBJECT (g_object_ref (session_client_singleton));
    }
  else
    {
      GObject *session_client = G_OBJECT_CLASS (xfce_session_client_parent_class)->constructor (type, n_construct_params, construct_params);
      session_client_singleton = XFCE_SESSION_CLIENT (session_client);
      return session_client;
    }
}

static void
xfce_session_client_constructed (GObject *obj)
{
  G_OBJECT_CLASS (xfce_session_client_parent_class)->constructed (obj);

#ifdef ENABLE_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    {
      const gchar *desktop_autostart_id = g_getenv ("DESKTOP_AUTOSTART_ID");
      if (desktop_autostart_id != NULL)
        {
          XfceSessionClient *session_client = XFCE_SESSION_CLIENT (obj);
          const gchar *client_id = xfce_session_client_get_client_id (session_client);

          if (client_id != NULL && strcmp (client_id, desktop_autostart_id) != 0)
            {
              g_warning ("SM client ID specified on command line (%s) is "
                         "different from ID specified by $DESKTOP_AUTOSTART_ID "
                         "(%s); using env var",
                         client_id,
                         desktop_autostart_id);
            }

          _xfce_session_client_set_client_id (session_client, desktop_autostart_id);

          g_unsetenv ("DESKTOP_AUTOSTART_ID");
        }
    }
#endif
}

static void
xfce_session_client_finalize (GObject *obj)
{
  XfceSessionClient *session_client = XFCE_SESSION_CLIENT (obj);
  XfceSessionClientPrivate *priv = GET_PRIV (session_client);

  g_assert (session_client == session_client_singleton);
  session_client_singleton = NULL;

  startup_options.argc = 0;
  g_clear_pointer (&startup_options.argv, g_strfreev);
  g_clear_pointer (&startup_options.client_id, g_free);
  startup_options.sm_disable = FALSE;

  g_free (priv->state_file);
  g_free (priv->desktop_file);

  g_free (priv->client_id);
  g_free (priv->current_directory);
  g_strfreev (priv->clone_command);
  g_strfreev (priv->restart_command);
  g_strfreev (priv->discard_command);

  G_OBJECT_CLASS (xfce_session_client_parent_class)->finalize (obj);
}



const gchar *
_xfce_session_client_state_to_string (XfceSessionClientState state)
{
  switch (state)
    {
    case XFCE_SESSION_CLIENT_STATE_DISCONNECTED:
      return "DISCONNECTED";
    case XFCE_SESSION_CLIENT_STATE_REGISTERING:
      return "REGISTERING";
    case XFCE_SESSION_CLIENT_STATE_IDLE:
      return "IDLE";
    case XFCE_SESSION_CLIENT_STATE_SAVING_PHASE_1:
      return "SAVING_PHASE_1";
    case XFCE_SESSION_CLIENT_STATE_WAITING_FOR_INTERACT:
      return "WAITING_FOR_INTERACT";
    case XFCE_SESSION_CLIENT_STATE_INTERACTING:
      return "INTERACTING";
    case XFCE_SESSION_CLIENT_STATE_WAITING_FOR_PHASE_2:
      return "WAITING_FOR_PHASE_2";
    case XFCE_SESSION_CLIENT_STATE_SAVING_PHASE_2:
      return "SAVING_PHASE_2";
    case XFCE_SESSION_CLIENT_STATE_FROZEN:
      return "FROZEN";
    default:
      return "(unknown)";
    }
}

guint
_xfce_session_client_class_quit_requested_signal_id (void)
{
  return signals[SIG_QUIT_REQUESTED];
}

guint
_xfce_session_client_class_save_state_extended_signal_id (void)
{
  return signals[SIG_SAVE_STATE_EXTENDED];
}

guint
_xfce_session_client_class_quit_signal_id (void)
{
  return signals[SIG_QUIT];
}

static void
sync_commands (XfceSessionClient *session_client)
{
  XfceSessionClientClass *klass = XFCE_SESSION_CLIENT_GET_CLASS (session_client);

  if (klass->sync_commands != NULL)
    klass->sync_commands (session_client);
}

/* If @command[@i] is the session ID argument, sets @n_args to the number of
 * vector entries it occupies and returns the ID it carries, which is %NULL if
 * the argument is present but has no value.  Otherwise sets @n_args to zero
 * and returns %NULL.  Both the "--sm-client-id=ID" and "--sm-client-id ID"
 * forms are accepted. */
static const gchar *
command_peek_client_id (const gchar *const *command,
                        gint i,
                        gint *n_args)
{
  if (g_str_has_prefix (command[i], SM_ID_ARG "="))
    {
      *n_args = 1;
      return command[i] + strlen (SM_ID_ARG "=");
    }
  else if (strcmp (command[i], SM_ID_ARG) == 0)
    {
      *n_args = command[i + 1] != NULL ? 2 : 1;
      return command[i + 1];
    }
  else
    {
      *n_args = 0;
      return NULL;
    }
}

/* Returns a copy of @command carrying @client_id as its session ID argument,
 * or, if @client_id is %NULL, with that argument removed.  The session manager
 * hands us back the ID in the restart command when it restarts us, and must
 * not find one in the clone command, which starts an unrelated instance. */
static gchar **
command_with_client_id (const gchar *const *command,
                        const gchar *client_id)
{
  GPtrArray *new_command = g_ptr_array_new ();

  for (gint i = 0; command[i] != NULL; ++i)
    {
      gint n_args;

      command_peek_client_id (command, i, &n_args);

      if (n_args > 0)
        i += n_args - 1;
      else
        g_ptr_array_add (new_command, g_strdup (command[i]));
    }

  if (client_id != NULL)
    {
      g_ptr_array_add (new_command, g_strdup (SM_ID_ARG));
      g_ptr_array_add (new_command, g_strdup (client_id));
    }

  g_ptr_array_add (new_command, NULL);

  return (gchar **) g_ptr_array_free (new_command, FALSE);
}

void
_xfce_session_client_set_client_id (XfceSessionClient *session_client,
                                    const gchar *client_id)
{
  XfceSessionClientPrivate *priv = GET_PRIV (session_client);

  if (g_strcmp0 (priv->client_id, client_id) == 0)
    return;

  g_free (priv->client_id);
  priv->client_id = g_strdup (client_id);

#ifdef ENABLE_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    gdk_x11_set_sm_client_id (priv->client_id);
#endif

  /* the restart command embeds the ID, so it has to go back out */
  sync_commands (session_client);

  g_object_notify (G_OBJECT (session_client), "client-id");
}

static void
xfce_session_client_parse_argv (gint argc,
                                const gchar **argv,
                                gchar **client_id,
                                gchar ***restart_command,
                                gchar ***clone_command)
{
#ifdef ENABLE_X11
  gboolean display_is_x11 = GDK_IS_X11_DISPLAY (gdk_display_get_default ());
  gboolean got_x11_display = FALSE;
#endif

  if (argc == 0 || argv == NULL)
    return;

  GStrvBuilder *clone_builder = g_strv_builder_new ();

  for (gint i = 0; i < argc; ++i)
    {
      gint n_args;
      const gchar *arg_client_id = command_peek_client_id ((const gchar *const *) argv, i, &n_args);

      if (n_args > 0)
        {
          if (arg_client_id != NULL && client_id != NULL)
            *client_id = g_strdup (arg_client_id);
          i += n_args - 1;
        }
#ifdef ENABLE_X11
      else if (display_is_x11 && (g_strcmp0 (argv[i], DPY_ARG) == 0 || g_str_has_prefix (argv[i], DPY_ARG "=")))
        {
          got_x11_display = TRUE;
          if (argv[i][strlen (DPY_ARG)] != '=')
            i++;
        }
#endif
      else
        {
          if (strcmp (argv[i], "--sm-client-disable") == 0)
            startup_options.sm_disable = TRUE;

          g_strv_builder_add (clone_builder, argv[i]);
        }
    }

  GStrvBuilder *restart_builder = g_strv_builder_new ();
  for (gint i = 0; i < argc; ++i)
    {
      g_strv_builder_add (restart_builder, argv[i]);
    }

#ifdef ENABLE_X11
  if (display_is_x11 && !got_x11_display)
    {
      GdkScreen *gscreen = gdk_display_get_default_screen (gdk_display_get_default ());
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gchar *display_name = gdk_screen_make_display_name (gscreen);
      G_GNUC_END_IGNORE_DEPRECATIONS

      g_strv_builder_add (restart_builder, DPY_ARG);
      g_strv_builder_add (restart_builder, display_name);

      g_free (display_name);
    }
#endif

  *restart_command = g_strv_builder_end (restart_builder);
  *clone_command = g_strv_builder_end (clone_builder);

  DBG ("setting restart and clone commands (%s, %s)",
       *restart_command && (*restart_command)[0] ? (*restart_command)[0] : "(null)",
       *clone_command && (*clone_command)[0] ? (*clone_command)[0] : "(null)");

  g_strv_builder_unref (restart_builder);
  g_strv_builder_unref (clone_builder);
}

void
_xfce_session_client_set_state (XfceSessionClient *session_client,
                                XfceSessionClientState new_state)
{
  XfceSessionClientPrivate *priv = GET_PRIV (session_client);
  XfceSessionClientState old_state = priv->state;

  if (G_UNLIKELY (old_state == new_state))
    return;

  priv->state = new_state;

  DBG ("state change: %s -> %s", _xfce_session_client_state_to_string (old_state),
       _xfce_session_client_state_to_string (new_state));
}

XfceSessionClientState
_xfce_session_client_get_state (XfceSessionClient *session_client)
{
  return GET_PRIV (session_client)->state;
}

void
_xfce_session_client_set_is_resumed (XfceSessionClient *session_client,
                                     gboolean is_resumed)
{
  XfceSessionClientPrivate *priv = GET_PRIV (session_client);
  if (priv->resumed != is_resumed)
    {
      priv->resumed = is_resumed;
      g_object_notify (G_OBJECT (session_client), "resumed");
    }
}

const gchar *
_xfce_session_client_peek_state_file (XfceSessionClient *session_client)
{
  return GET_PRIV (session_client)->state_file;
}

const gchar *
_xfce_session_client_peek_desktop_file (XfceSessionClient *session_client)
{
  return GET_PRIV (session_client)->desktop_file;
}

gchar **
_xfce_session_client_dup_effective_clone_command (XfceSessionClient *session_client)
{
  XfceSessionClientPrivate *priv = GET_PRIV (session_client);
  gchar **command = priv->clone_command != NULL ? priv->clone_command : priv->restart_command;

  if (command == NULL)
    return NULL;
  else
    return command_with_client_id ((const gchar *const *) command, NULL);
}

gchar **
_xfce_session_client_dup_effective_restart_command (XfceSessionClient *session_client)
{
  XfceSessionClientPrivate *priv = GET_PRIV (session_client);

  if (priv->restart_command == NULL)
    return NULL;
  else
    return command_with_client_id ((const gchar *const *) priv->restart_command,
                                   priv->client_id);
}

gchar **
_xfce_session_client_dup_effective_discard_command (XfceSessionClient *session_client)
{
  XfceSessionClientPrivate *priv = GET_PRIV (session_client);

  if (priv->discard_command == NULL)
    return NULL;
  else
    return g_strdupv (priv->discard_command);
}


/**
 * xfce_session_client_error_quark:
 *
 * Gets the XfceSessionClient Error Quark.
 *
 * Return value: a #GQuark.
 **/
GQuark
xfce_session_client_error_quark (void)
{
  static GQuark q;

  if G_UNLIKELY (q == 0)
    q = g_quark_from_static_string ("xfce-session-client-error-quark");

  return q;
}



/**
 * xfce_session_client_get_option_group:
 * @argc: The application's argument count
 * @argv: The application's argument vector
 *
 * Constructs a #GOptionGroup suitable for use with Glib's
 * command-line option parser.
 *
 * This function is a bit sneaky in that it will make a copy of
 * the program's argc and argv <emphasis>before</emphasis> GTK+ etc.
 * has a chance to mess around with it, so #XfceSessionClient can later
 * construct an accurate restart command.  Instead of calling
 * gtk_init() or gtk_init_with_args(), instead you'd do something
 * like:
 *
 * <informalexample><programlisting>
 * GOptionContext *context = g_option_context_new("");
 * g_option_context_add_group(context, gtk_get_option_group(TRUE));
 * g_option_context_add_group(context, xfce_session_client_get_option_group(argc, argv);
 * g_option_context_parse(context, &argc, &argv, NULL);
 * </programlisting></informalexample>
 *
 * Error checking is omitted here for brevity, and of course you could
 * add your app's own options with g_option_context_add_main_entries()
 * or similar.
 *
 * Returns: A new #GOptionGroup
 **/
GOptionGroup *
xfce_session_client_get_option_group (gint argc,
                                      gchar **argv)
{
  const GOptionEntry entries[] = {
    { "sm-client-id", 0, 0, G_OPTION_ARG_STRING, &startup_options.client_id, N_ ("Session management client ID"), N_ ("ID") },
    { "sm-client-disable", 0, 0, G_OPTION_ARG_NONE, &startup_options.sm_disable, N_ ("Disable session management"), NULL },
    { NULL },
  };
  GOptionGroup *group = NULL;

  /* make sure to use the translations from libxfce4session-client */
  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  startup_options.argc = argc;
  g_clear_pointer (&startup_options.argv, g_strfreev);
  if (argv)
    startup_options.argv = g_strdupv (argv);

  group = g_option_group_new ("sm-client", _("Session management options"), _("Show session management options"), NULL,
                              NULL);
  g_option_group_add_entries (group, entries);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);

  return group;
}

G_GNUC_NULL_TERMINATED
static XfceSessionClient *
xfce_session_client_new_internal (const gchar *first_property_name,
                                  ...)
{
  if (session_client_singleton != NULL)
    {
      return g_object_ref (session_client_singleton);
    }
  else
    {
      GType client_type = XFCE_TYPE_SESSION_CLIENT_DBUS;

#ifdef ENABLE_WAYLAND
      if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
        {
          client_type = XFCE_TYPE_SESSION_CLIENT_WL_XDG;
        }
#endif

#ifdef ENABLE_X11
      if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
        {
          client_type = XFCE_TYPE_SESSION_CLIENT_XSMP;
        }
#endif

      va_list args;
      va_start (args, first_property_name);
      GObject *session_client = g_object_new_valist (client_type, first_property_name, args);
      va_end (args);

      return XFCE_SESSION_CLIENT (session_client);
    }
}

/**
 * xfce_session_client_new:
 *
 * Creates the application's SM client instance.  This is best
 * used with xfce_session_client_get_option_group() above (and using
 * the returned #GOptionGroup with g_option_context_parse()), as the
 * command line parsing will figure out many of the SM client's
 * required property values for you.
 *
 * If you have already created an #XfceSessionClient instance using any of the
 * "new" functions, this will return a new reference to the same instance.
 *
 * Returns: (transfer full) (not nullable): A new #XfceSessionClient instance,
 * or the singleton with its reference count incremented.
 **/
XfceSessionClient *
xfce_session_client_new (void)
{
  gchar **restart_command = NULL;
  gchar **clone_command = NULL;

  xfce_session_client_parse_argv (startup_options.argc,
                                  (const gchar **) startup_options.argv,
                                  NULL,
                                  &restart_command,
                                  &clone_command);
  XfceSessionClient *client = xfce_session_client_new_internal ("client-id", startup_options.client_id,
                                                                "restart-command", restart_command,
                                                                "clone-command", clone_command,
                                                                NULL);

  g_strfreev (restart_command);
  g_strfreev (clone_command);

  return client;
}

/**
 * xfce_session_client_new_with_argv:
 * @argc: The number of arguments passed to main()
 * @argv: The argument vector passed to main()
 * @restart_style: An #XfceSessionClientRestartStyle
 * @priority: A restart priority
 *
 * Creates a new #XfceSessionClient instance.  It attempts to
 * set all required properties using the app's command line.
 * Note that this function does not actually connect to the session
 * manager, so other actions can be taken (such as setting custom
 * properties or connecting signals) before calling
 * xfce_session_client_connect().
 *
 * If you have already created an #XfceSessionClient instance using any of the
 * "new" functions, this will return a new reference to the same instance,
 * ignoring any arguments passed.
 *
 *
 * If you are using Gtk or Glib's command-line option parser,
 * it is recommended that you use xfce_session_client_get_option_group()
 * and xfce_session_client_new() instead.
 *
 * Returns: (transfer full) (not nullable): A new #XfceSessionClient instance,
 * or the singleton with its reference count incremented.
 **/
XfceSessionClient *
xfce_session_client_new_with_argv (gint argc,
                                   gchar **argv,
                                   XfceSessionClientRestartStyle restart_style,
                                   guchar priority)
{
  gchar *client_id = NULL;
  gchar **restart_command = NULL;
  gchar **clone_command = NULL;

  xfce_session_client_parse_argv (argc,
                                  (const gchar **) argv,
                                  &client_id,
                                  &restart_command,
                                  &clone_command);
  XfceSessionClient *client = xfce_session_client_new_internal ("restart-style", restart_style,
                                                                "priority", priority,
                                                                "client-id", client_id,
                                                                "restart-command", restart_command,
                                                                "clone-command", clone_command,
                                                                NULL);

  g_free (client_id);
  g_strfreev (restart_command);
  g_strfreev (clone_command);

  return client;
}

/**
 * xfce_session_client_new_full:
 * @restart_style: An XfceSessionClientRestartStyle
 * @priority: A restart priority
 * @resumed_client_id: The client id used in the previous session
 * @current_directory: The application's working directory
 * @restart_command: A command that can resume the application's
 *                   saved state
 * @desktop_file: The application's .desktop file
 *
 * Creates a new SM client instance, allowing the application
 * fine-grained control over the initial properties set.
 * Note that this function does not actually connect to the session
 * manager, so other actions can be taken (such as setting custom
 * properties or connecting signals) before calling
 * xfce_session_client_connect().
 *
 * If you have already created an #XfceSessionClient instance using any of the
 * "new" functions, this will return a new reference to the same instance,
 * ignoring any arguments passed.
 *
 * It is recommended to use xfce_session_client_new_with_argv(), or,
 * if you are using Gtk or Glib's command-line option parser,
 * xfce_session_client_get_option_group() and xfce_session_client_new() instead.
 *
 * Returns: (transfer full) (not nullable): A new #XfceSessionClient instance,
 * or the singleton with its reference count incremented.
 **/
XfceSessionClient *
xfce_session_client_new_full (XfceSessionClientRestartStyle restart_style,
                              guchar priority,
                              const gchar *resumed_client_id,
                              const gchar *current_directory,
                              const gchar **restart_command,
                              const gchar *desktop_file)
{
  return xfce_session_client_new_internal ("restart-style", restart_style,
                                           "priority", priority,
                                           "client-id", resumed_client_id,
                                           "current-directory", current_directory,
                                           "restart-command", restart_command,
                                           "desktop-file", desktop_file,
                                           NULL);
}

/**
 * xfce_session_client_get:
 *
 * Returns the singleton instance of #XfceSessionClient, if one has been
 * created.
 *
 * Returns: (transfer none) (nullable): A #XfceSessionClient instance
 **/
XfceSessionClient *
xfce_session_client_get (void)
{
  return session_client_singleton;
}

/**
 * xfce_session_client_connect:
 * @session_client: An #XfceSessionClient
 * @error: (out) (nullable) (transfer full): A #GError location.
 *
 * Attempts to connect to the session manager.
 *
 * Returns: %TRUE on success, %FALSE otherwise.  If an error
 *          occurs, @error will be set.
 **/
gboolean
xfce_session_client_connect (XfceSessionClient *session_client,
                             GError **error)
{
  g_return_val_if_fail (XFCE_IS_SESSION_CLIENT (session_client), FALSE);
  g_return_val_if_fail (!error || !*error, FALSE);

  if (startup_options.sm_disable)
    return TRUE;

  return XFCE_SESSION_CLIENT_GET_CLASS (session_client)->connect (session_client, error);
}

/**
 * xfce_session_client_disconnect:
 * @session_client: An #XfceSessionClient
 *
 * Disconnects the application from the session manager.
 *
 * <note><para>
 * This may not remove the application from the saved
 * session (if any) if the user later does not choose to save
 * the session when logging out.
 * </para></note>
 *
 **/
void
xfce_session_client_disconnect (XfceSessionClient *session_client)
{
  g_return_if_fail (XFCE_IS_SESSION_CLIENT (session_client));

  if (startup_options.sm_disable)
    return;

  XFCE_SESSION_CLIENT_GET_CLASS (session_client)->disconnect (session_client);

#ifdef ENABLE_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    gdk_x11_set_sm_client_id (NULL);
#endif

  _xfce_session_client_set_state (session_client, XFCE_SESSION_CLIENT_STATE_DISCONNECTED);
}

/**
 * xfce_session_client_is_connected:
 * @session_client: An #XfceSessionClient
 *
 * Determines whether or not the application has connected to the
 * session manager.
 *
 * Returns: %TRUE if connected to the session manager, %FALSE otherwise
 **/
gboolean
xfce_session_client_is_connected (XfceSessionClient *session_client)
{
  g_return_val_if_fail (XFCE_IS_SESSION_CLIENT (session_client), FALSE);
  return XFCE_SESSION_CLIENT_GET_CLASS (session_client)->is_connected (session_client);
}

/**
 * xfce_session_client_is_resumed:
 * @session_client: An #XfceSessionClient
 *
 * Determines whether the application was resumed from a previous
 * session, or if the application has been started fresh with no
 * state information associated with it.
 *
 * Returns: %TRUE if resumed from a previous session, %FALSE otherwise
 **/
gboolean
xfce_session_client_is_resumed (XfceSessionClient *session_client)
{
  g_return_val_if_fail (XFCE_IS_SESSION_CLIENT (session_client), FALSE);
  return GET_PRIV (session_client)->resumed;
}

/**
 * xfce_session_client_set_desktop_file:
 * @session_client: An #XfceSessionClient
 * @desktop_file: The path to the application's .desktop file
 *
 * Sets the application's .desktop file.  In addition to informing
 * the session manager of the .desktop file so it can present localized
 * names and an icon in session listings and the splash screen, this
 * also calls g_set_application_name() and
 * gtk_window_set_default_icon_name() (or
 * gtk_window_set_default_icon_from_file()) if the Name and Icon
 * keys are present, respectively.
 *
 * If a relative path to the file is provided, this function will search
 * the standard application directories as specified by the
 * <ulink type="http" url="http://standards.freedesktop.org/menu-spec/latest/">XDG
 * Desktop Menu Specification</ulink>.
 **/
void
xfce_session_client_set_desktop_file (XfceSessionClient *session_client,
                                      const gchar *desktop_file)
{
  XfceRc *rcfile = NULL;
  gchar *real_desktop_file = NULL;
  gboolean has_default_icon = FALSE;
  GList *icon_list;
  const gchar *name, *icon, *exec;

  g_return_if_fail (XFCE_IS_SESSION_CLIENT (session_client));
  XfceSessionClientPrivate *priv = GET_PRIV (session_client);
  g_return_if_fail (desktop_file != NULL || priv->desktop_file == NULL);

  if (desktop_file == NULL || g_strcmp0 (priv->desktop_file, desktop_file) == 0)
    return;

  if (!g_path_is_absolute (desktop_file))
    {
      gchar res_name[1024];

      g_snprintf (res_name, sizeof (res_name), "applications/%s", desktop_file);
      real_desktop_file = xfce_resource_lookup (XFCE_RESOURCE_DATA, res_name);
      if (!real_desktop_file)
        {
          g_warning ("Cannot find file \"%s\" in the standard search path",
                     desktop_file);
          return;
        }

      desktop_file = real_desktop_file;
    }

  rcfile = xfce_rc_simple_open (desktop_file, TRUE);
  if (!rcfile)
    {
      g_warning ("Unable to open \"%s\"", desktop_file);
      goto out;
    }

  if (!xfce_rc_has_group (rcfile, "Desktop Entry"))
    {
      g_warning ("File \"%s\" is not a valid .desktop file", desktop_file);
      goto out;
    }

  g_free (priv->desktop_file);
  priv->desktop_file = g_strdup (desktop_file);

  xfce_rc_set_group (rcfile, "Desktop Entry");

  if (!g_get_application_name ())
    {
      name = xfce_rc_read_entry (rcfile, "Name", NULL);
      if (name)
        g_set_application_name (name);
    }

  if (gtk_window_get_default_icon_name ())
    has_default_icon = TRUE;

  icon_list = gtk_window_get_default_icon_list ();
  if (icon_list)
    {
      has_default_icon = TRUE;
      g_list_free (icon_list);
    }

  if (!has_default_icon)
    {
      icon = xfce_rc_read_entry (rcfile, "Icon", NULL);
      if (icon)
        {
          if (g_path_is_absolute (icon))
            gtk_window_set_default_icon_from_file (icon, NULL);
          else
            gtk_window_set_default_icon_name (icon);
        }
    }

  exec = xfce_rc_read_entry (rcfile, "Exec", NULL);
  if (exec)
    {
      gchar **clone_argv = NULL;
      gint clone_argc = 0;

      /* FIXME: pull out the %-var substitutions first */

      if (g_shell_parse_argv (exec, &clone_argc, &clone_argv, NULL))
        {
          xfce_session_client_set_clone_command (session_client, (const gchar **) clone_argv);
          g_strfreev (clone_argv);
        }
    }

  XFCE_SESSION_CLIENT_GET_CLASS (session_client)->set_desktop_file (session_client, priv->desktop_file);

out:
  if (rcfile)
    xfce_rc_close (rcfile);
  g_free (real_desktop_file);
}

/**
 * xfce_session_client_request_shutdown:
 * @session_client: An #XfceSessionClient
 * @shutdown_hint: The type of shutdown requested
 *
 * Sends a request to the session manager to end the session.
 * Depending on @hint, the session manager may prompt for a
 * certain action (log out, halt, reboot, etc.) or may take the
 * requested action without user intervention.
 *
 * <note><para>
 * The session manager may or may not support all requested
 * actions, and is also free to ignore the requested action.
 * </para></note>
 **/
void
xfce_session_client_request_shutdown (XfceSessionClient *session_client,
                                      XfceSessionClientShutdownHint shutdown_hint)
{
  g_return_if_fail (XFCE_IS_SESSION_CLIENT (session_client));

  if (startup_options.sm_disable)
    return;

  XFCE_SESSION_CLIENT_GET_CLASS (session_client)->request_shutdown (session_client, shutdown_hint);
}

/**
 * xfce_session_client_set_restart_style:
 * @session_client: An #XfceSessionClient
 * @restart_style: An #XfceSessionClientRestartStyle value
 *
 * Sets the restart style hint to @restart_style.
 **/
void
xfce_session_client_set_restart_style (XfceSessionClient *session_client,
                                       XfceSessionClientRestartStyle restart_style)
{
  g_return_if_fail (XFCE_IS_SESSION_CLIENT (session_client));
  XfceSessionClientPrivate *priv = GET_PRIV (session_client);

  if (priv->restart_style == restart_style)
    return;

  priv->restart_style = restart_style;

  XFCE_SESSION_CLIENT_GET_CLASS (session_client)->set_restart_style (session_client, restart_style);

  g_object_notify (G_OBJECT (session_client), "restart-style");
}

/**
 * xfce_session_client_set_priority:
 * @session_client: An #XfceSessionClient
 * @priority: A 8-bit signed priority value
 *
 * Sets the startup priority for @session_client to @priority.  Note
 * that the default priority for applications is 50; lower values
 * should be reserved for components of the desktop environment.
 **/
void
xfce_session_client_set_priority (XfceSessionClient *session_client,
                                  guint8 priority)
{
  g_return_if_fail (XFCE_IS_SESSION_CLIENT (session_client));
  XfceSessionClientPrivate *priv = GET_PRIV (session_client);

  if (priv->priority == priority)
    return;

  priv->priority = priority;

  XFCE_SESSION_CLIENT_GET_CLASS (session_client)->set_priority (session_client, priority);

  g_object_notify (G_OBJECT (session_client), "priority");
}

/**
 * xfce_session_client_set_current_directory:
 * @session_client: An #XfceSessionClient
 * @current_directory: A valid path name
 *
 * Sets the startup working directory of @session_client to
 * @current_directory.  If unset, defaults to the user's
 * home directory.
 **/
void
xfce_session_client_set_current_directory (XfceSessionClient *session_client,
                                           const gchar *current_directory)
{
  g_return_if_fail (XFCE_IS_SESSION_CLIENT (session_client));
  XfceSessionClientPrivate *priv = GET_PRIV (session_client);

  if (current_directory == NULL)
    {
      current_directory = g_get_home_dir ();
    }

  if (g_strcmp0 (priv->current_directory, current_directory) == 0)
    return;

  g_free (priv->current_directory);
  priv->current_directory = g_strdup (current_directory);

  XFCE_SESSION_CLIENT_GET_CLASS (session_client)->set_current_directory (session_client, current_directory);

  g_object_notify (G_OBJECT (session_client), "current-directory");
}

/**
 * xfce_session_client_set_clone_command:
 * @session_client: An #XfceSessionClient
 * @clone_command: An argument vector
 *
 * Sets the application's "clone" command, which is used to start a fresh
 * instance of the application without restoring any state.
 *
 * If unset, defaults to the command used to start this instance
 * of the application, with session management related arguments
 * removed (if present).
 **/
void
xfce_session_client_set_clone_command (XfceSessionClient *session_client,
                                       const gchar *const *clone_command)
{
  g_return_if_fail (XFCE_IS_SESSION_CLIENT (session_client));
  XfceSessionClientPrivate *priv = GET_PRIV (session_client);

  if (priv->clone_command == (gchar **) clone_command
      || (priv->clone_command != NULL
          && clone_command != NULL
          && g_strv_equal ((const gchar **) priv->clone_command, clone_command)))
    {
      return;
    }

  g_strfreev (priv->clone_command);
  priv->clone_command = g_strdupv ((gchar **) clone_command);
  sync_commands (session_client);

  g_object_notify (G_OBJECT (session_client), "clone-command");
}

/**
 * xfce_session_client_set_restart_command:
 * @session_client: An #XfceSessionClient
 * @restart_command: An argument vector
 *
 * Sets the application's "restart" command, which is used to restart
 * the application and restore any saved state from the previous
 * run.
 *
 * If unset, defaults to the command used to start this instance
 * of the application, with session management related arguments
 * added (if not already present).
 **/
void
xfce_session_client_set_restart_command (XfceSessionClient *session_client,
                                         const gchar *const *restart_command)
{
  g_return_if_fail (XFCE_IS_SESSION_CLIENT (session_client));
  XfceSessionClientPrivate *priv = GET_PRIV (session_client);

  if (priv->restart_command == (gchar **) restart_command
      || (priv->restart_command != NULL
          && restart_command != NULL
          && g_strv_equal ((const gchar **) priv->restart_command, restart_command)))
    {
      return;
    }

  g_strfreev (priv->restart_command);
  priv->restart_command = g_strdupv ((gchar **) restart_command);
  sync_commands (session_client);

  g_object_notify (G_OBJECT (session_client), "restart-command");
}

/**
 * xfce_session_client_get_restart_style:
 * @session_client: An #XfceSessionClient
 *
 * Retrieves the session client's restart style.  See
 * xfce_session_client_set_restart_style() for more information.
 *
 * Returns: a value from the #XfceSessionClientRestartStyle enum
 **/
XfceSessionClientRestartStyle
xfce_session_client_get_restart_style (XfceSessionClient *session_client)
{
  g_return_val_if_fail (XFCE_IS_SESSION_CLIENT (session_client),
                        XFCE_SESSION_CLIENT_RESTART_NORMAL);
  return GET_PRIV (session_client)->restart_style;
}

/**
 * xfce_session_client_get_priority:
 * @session_client: An #XfceSessionClient
 *
 * Retrieves the session client's restart priority.  See
 * xfce_session_client_set_priority() for more information.
 *
 * Returns: a value from #G_MININT8 to #G_MAXINT8
 **/
guint8
xfce_session_client_get_priority (XfceSessionClient *session_client)
{
  g_return_val_if_fail (XFCE_IS_SESSION_CLIENT (session_client),
                        XFCE_SESSION_CLIENT_PRIORITY_DEFAULT);
  return GET_PRIV (session_client)->priority;
}

/**
 * xfce_session_client_get_client_id:
 * @session_client: An #XfceSessionClient
 *
 * Retrieves the session client's unique ID.  This ID can
 * be used to construct a filename used to restore the
 * application's state.  Note that this value is only
 * guaranteed to be valid if connected to the session manager.
 *
 * <note><para>
 * Instead of constructing a state filename, it is
 * recommended to use xfce_session_client_get_state_file().
 * </para></note>
 *
 * Returns: an opaque object-owned string
 **/
const gchar *
xfce_session_client_get_client_id (XfceSessionClient *session_client)
{
  g_return_val_if_fail (XFCE_IS_SESSION_CLIENT (session_client), NULL);
  return GET_PRIV (session_client)->client_id;
}

/**
 * xfce_session_client_get_state_file:
 * @session_client: An #XfceSessionClient
 *
 * Constructs a filename that can be used to restore or save
 * state information.
 *
 * When saving state, ote that this file may already exist (and
 * may have been used for saving previous state for the
 * application), so the application should first remove or empty
 * the file if it requires a fresh state file.
 *
 * On the next application start, this function can be used to
 * check to see if there is any previous saved state, and, if so,
 * the state can be restored from the file.
 *
 * This function will use a standard location and naming scheme
 * and handle state cleanup (setting of the discard command) for you.
 *
 * Before calling this function, the application must have a
 * valid client ID (see xfce_session_client_get_client_id()).
 *
 * Returns: a file name string, owned by the object or %NULL if
 *          the session client is disabled.
 **/
const gchar *
xfce_session_client_get_state_file (XfceSessionClient *session_client)
{
  gchar *resource, *p;
  const gchar *prgname;

  g_return_val_if_fail (XFCE_IS_SESSION_CLIENT (session_client), NULL);

  XfceSessionClientPrivate *priv = GET_PRIV (session_client);

  if (!priv->client_id)
    return NULL;

  if (priv->state_file)
    return priv->state_file;

  prgname = g_get_prgname ();
  if (G_UNLIKELY (!prgname))
    prgname = "unknown";

  resource = g_strdup_printf ("sessions/%s-%s.state",
                              prgname, priv->client_id);
  for (p = resource + 9; *p; p++)
    {
      if (*p == '/')
        *p = '_';
    }

  priv->state_file = xfce_resource_save_location (XFCE_RESOURCE_CACHE,
                                                  resource, TRUE);
  if (!priv->state_file)
    {
      g_critical ("XfceSessionClient: Unable to create state file as "
                  "\"$XDG_CACHE_HOME/%s\"",
                  resource);
    }

  g_free (resource);

  if (G_LIKELY (priv->state_file))
    {
      const gchar *discard_command[] = { "rm", "-rf", priv->state_file, NULL };

      g_strfreev (priv->discard_command);
      priv->discard_command = g_strdupv ((gchar **) discard_command);

      sync_commands (session_client);
    }

  return priv->state_file;
}

/**
 * xfce_session_client_get_current_directory:
 * @session_client: An #XfceSessionClient
 *
 * Retrieves the session client's working directory.  See
 * xfce_session_client_set_current_directory() for more information.
 *
 * Returns: an object-owned string
 **/
const gchar *
xfce_session_client_get_current_directory (XfceSessionClient *session_client)
{
  g_return_val_if_fail (XFCE_IS_SESSION_CLIENT (session_client), NULL);
  return GET_PRIV (session_client)->current_directory;
}

/**
 * xfce_session_client_get_clone_command:
 * @session_client: An #XfceSessionClient
 *
 * Retrieves the session client's clone command.  See
 * xfce_session_client_set_clone_command() for more information.
 *
 * Returns: an object-owned string vector
 **/
const gchar *const *
xfce_session_client_get_clone_command (XfceSessionClient *session_client)
{
  g_return_val_if_fail (XFCE_IS_SESSION_CLIENT (session_client), NULL);
  return (const gchar *const *) GET_PRIV (session_client)->clone_command;
}

/**
 * xfce_session_client_get_restart_command:
 * @session_client: An #XfceSessionClient
 *
 * Retrieves the session client's restart command.  See
 * xfce_session_client_set_restart_command() for more information.
 *
 * Returns: an object-owned string vector
 **/
const gchar *const *
xfce_session_client_get_restart_command (XfceSessionClient *session_client)
{
  g_return_val_if_fail (XFCE_IS_SESSION_CLIENT (session_client), NULL);
  return (const gchar *const *) GET_PRIV (session_client)->restart_command;
}

/**
 * xfce_session_client_add_window:
 * @session_client: An #XfceSessionClient
 * @window: A #GtkWindow
 * @name: A unique name
 *
 * Asks the session manager to keep track of @window so its WM state can be
 * restored later.  @window must not have already been added to the session.
 *
 * @name must be unique, and cannot be a string that has previously been sent
 * to the session manager using this function or
 * @xfce_session_client_restore_window().  If you wish to re-use @name with cleared
 * state, you must call @xfce_session_client_remove_window() first.  If you wish to
 * add a window with a name that has been previously used (without being
 * removed), you must use @xfce_session_client_restore_window() instead.
 *
 * Depending on your needs, you can use descriptive titles for @name, such as
 * "settings-dialog", or you can use random strings or UUIDs.
 **/
void
xfce_session_client_add_window (XfceSessionClient *session_client,
                                GtkWindow *window,
                                const gchar *name)
{
  g_return_if_fail (XFCE_IS_SESSION_CLIENT (session_client));
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (!xfce_str_is_empty (name));

  if (startup_options.sm_disable)
    return;

  XfceSessionClientClass *klass = XFCE_SESSION_CLIENT_GET_CLASS (session_client);
  if (klass->add_window != NULL)
    {
      klass->add_window (session_client, window, name);
    }
}

/**
 * xfce_session_client_restore_window:
 * @session_client: An #XfceSessionClient
 * @window: A #GtkWindow
 * @name: A unique name
 *
 * Asks the session manager to restore WM state for @name and associate it with
 * @window.  If @name is not known in the session, the session manager will act
 * as if @xfce_session_client_add_window() was called.  @window must not have
 * already been added to the session, and there must be no other window
 * currently registered with the session manager using @name.
 *
 * This must be called before @window is mapped.
 *
 * While this may be called at any time, note that the window restore will not
 * take place until @window is mapped.  If @window becomes mapped while
 * @session_client is not connected to the session manager, it cannot be
 * restored.  When @client is eventually connected, @window will instead be
 * removed and re-added (as if @xfce_session_client_remove_window() and then
 * @xfce_session_client_add_window() were called), which will discard any
 * previous WM state for @window.
 **/
void
xfce_session_client_restore_window (XfceSessionClient *session_client,
                                    GtkWindow *window,
                                    const gchar *name)
{
  g_return_if_fail (XFCE_IS_SESSION_CLIENT (session_client));
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (!gtk_widget_get_mapped (GTK_WIDGET (window)));
  g_return_if_fail (!xfce_str_is_empty (name));

  if (startup_options.sm_disable)
    return;

  XfceSessionClientClass *klass = XFCE_SESSION_CLIENT_GET_CLASS (session_client);
  if (klass->restore_window != NULL)
    {
      klass->restore_window (session_client, window, name);
    }
}

/**
 * xfce_session_client_rename_window:
 * @session_client: An #XfceSessionClient
 * @window: A #GtkWindow
 * @new_name: A unique name
 *
 * Asks the session manager to rename @window's stored state name to @new_name.
 *
 * @window must be registered with the compositor via a previous call to
 * @xfce_session_client_add_window() or @xfce_session_client_restore_window().
 *
 * @new_name must be unique, and cannot be a string that has previously been
 * sent to the session manager using @xfce_session_client_add_window(),
 * @xfce_session_client_restore_window(), or this function.  If you wish to re-use
 * @new_name, you must call @xfce_session_client_remove_window() first.
 *
 * Note that if this function is called while @window is not mapped or @client
 * is not connected, the actual rename operation will be deferred until the
 * next time it is mapped when @client is connected.
 *
 * If another window is named @new_name, but is unmapped with a pending rename
 * to another name, it is an error to call this function with @new_name on
 * @window.  You must first map the other window so the rename away from
 * @new_name can complete.
 **/
void
xfce_session_client_rename_window (XfceSessionClient *session_client,
                                   GtkWindow *window,
                                   const gchar *new_name)
{
  g_return_if_fail (XFCE_IS_SESSION_CLIENT (session_client));
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (!xfce_str_is_empty (new_name));

  if (startup_options.sm_disable)
    return;

  XfceSessionClientClass *klass = XFCE_SESSION_CLIENT_GET_CLASS (session_client);
  if (klass->rename_window != NULL)
    {
      klass->rename_window (session_client, window, new_name);
    }
}

/**
 * xfce_session_client_remove_window:
 * @session_client: An #XfceSessionClient
 * @name: Name of the window
 *
 * Asks the session manager to forget all state about the window identified by
 * @name.
 *
 * This will make @name available for use with other windows.
 *
 * If you just want to close/destroy the window while the session manager keeps
 * state for it (possibly to be restored later), destroy the window without
 * calling this function.
 **/
void
xfce_session_client_remove_window (XfceSessionClient *session_client,
                                   const gchar *name)
{
  g_return_if_fail (XFCE_IS_SESSION_CLIENT (session_client));
  g_return_if_fail (!xfce_str_is_empty (name));

  if (startup_options.sm_disable)
    return;

  XfceSessionClientClass *klass = XFCE_SESSION_CLIENT_GET_CLASS (session_client);
  if (klass->remove_window != NULL)
    {
      klass->remove_window (session_client, name);
    }
}



#define __XFCE_SESSION_CLIENT_C__
#include "libxfce4session-client-visibility.c"
