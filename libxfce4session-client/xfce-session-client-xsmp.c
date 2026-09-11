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
#include <X11/ICE/ICElib.h>
#include <X11/SM/SMlib.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include "xfce-session-client-xsmp.h"
#include "xfce-session-client.h"

struct _XfceSessionClientXsmp
{
  XfceSessionClient parent;

  SmcConn session_connection;
  IceConn ice_connection;
  guint32 needs_save_state : 1,
    shutdown_cancelled : 1;
};

static void
xfce_session_client_xsmp_finalize (GObject *obj);

static void
xfce_session_client_xsmp_set_command_property (XfceSessionClientXsmp *session_client,
                                               const char *property_name,
                                               gchar **command);

static gboolean
xfce_session_client_xsmp_connect (XfceSessionClient *session_client,
                                  GError **error);
static void
xfce_session_client_xsmp_disconnect (XfceSessionClient *session_client);
static void
xfce_session_client_xsmp_request_shutdown (XfceSessionClient *session_client,
                                           XfceSessionClientShutdownHint shutdown_hint);
static gboolean
xfce_session_client_xsmp_is_connected (XfceSessionClient *session_client);
static void
xfce_session_client_xsmp_set_desktop_file (XfceSessionClient *session_client,
                                           const gchar *desktop_file);
static void
xfce_session_client_xsmp_set_restart_style (XfceSessionClient *session_client,
                                            XfceSessionClientRestartStyle restart_style);
static void
xfce_session_client_xsmp_set_priority (XfceSessionClient *session_client,
                                       guint8 priority);
static void
xfce_session_client_xsmp_set_current_directory (XfceSessionClient *session_client,
                                                const gchar *current_directory);
static void
xfce_session_client_xsmp_sync_commands (XfceSessionClient *session_client);


G_DEFINE_FINAL_TYPE (XfceSessionClientXsmp, xfce_session_client_xsmp, XFCE_TYPE_SESSION_CLIENT)


static void
xfce_session_client_xsmp_class_init (XfceSessionClientXsmpClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = xfce_session_client_xsmp_finalize;

  XfceSessionClientClass *session_client_class = XFCE_SESSION_CLIENT_CLASS (klass);
  session_client_class->connect = xfce_session_client_xsmp_connect;
  session_client_class->disconnect = xfce_session_client_xsmp_disconnect;
  session_client_class->request_shutdown = xfce_session_client_xsmp_request_shutdown;
  session_client_class->is_connected = xfce_session_client_xsmp_is_connected;
  session_client_class->set_desktop_file = xfce_session_client_xsmp_set_desktop_file;
  session_client_class->set_restart_style = xfce_session_client_xsmp_set_restart_style;
  session_client_class->set_priority = xfce_session_client_xsmp_set_priority;
  session_client_class->set_current_directory = xfce_session_client_xsmp_set_current_directory;
  session_client_class->sync_commands = xfce_session_client_xsmp_sync_commands;
}

static void
xfce_session_client_xsmp_init (XfceSessionClientXsmp *session_client)
{
}

static void
xfce_session_client_xsmp_finalize (GObject *obj)
{
  XfceSessionClientXsmp *session_client = XFCE_SESSION_CLIENT_XSMP (obj);

  if (session_client->session_connection != NULL)
    xfce_session_client_disconnect (XFCE_SESSION_CLIENT (session_client));

  G_OBJECT_CLASS (xfce_session_client_xsmp_parent_class)->finalize (obj);
}


static void
xsmp_save_phase_2 (SmcConn smc_conn,
                   SmPointer client_data);
static void
xsmp_interact (SmcConn smc_conn,
               SmPointer client_data);
static void
xsmp_shutdown_cancelled (SmcConn smc_conn,
                         SmPointer client_data);
static void
xsmp_save_complete (SmcConn smc_conn,
                    SmPointer client_data);
static void
xsmp_die (SmcConn smc_conn,
          SmPointer client_data);
static void
xsmp_save_yourself (SmcConn smc_conn,
                    SmPointer client_data,
                    int save_style,
                    Bool shutdown,
                    int interact_style,
                    Bool fast);


static IceIOErrorHandler xsmp_ice_installed_handler = NULL;


/* This is called when data is available on an ICE connection.  */
static gboolean
xsmp_process_ice_messages (GIOChannel *channel,
                           GIOCondition condition,
                           gpointer client_data)
{
  XfceSessionClientXsmp *session_client = client_data;
  IceConn connection = session_client->ice_connection;
  IceProcessMessagesStatus status;

  status = IceProcessMessages (connection, NULL, NULL);
  if (status == IceProcessMessagesIOError)
    {
      g_warning ("Disconnected from session manager.");
      /* We were disconnected */
      IceSetShutdownNegotiation (connection, False);
      if (session_client->session_connection)
        {
          xfce_session_client_disconnect (XFCE_SESSION_CLIENT (session_client));
        }
      else
        {
          IceCloseConnection (connection);
        }
    }

  return TRUE;
}

/* This is called when a new ICE connection is made.  It arranges for
   the ICE connection to be handled via the event loop.  */
static void
xsmp_new_ice_connection (IceConn connection,
                         IcePointer client_data,
                         Bool opening,
                         IcePointer *watch_data)
{
  XfceSessionClientXsmp *session_client = client_data;
  guint input_id;

  if (opening)
    {
      /* Make sure we don't pass on these file descriptors to any
       * exec'ed children
       */
      GIOChannel *channel;

      session_client->ice_connection = connection;
      fcntl (IceConnectionNumber (connection), F_SETFD,
             fcntl (IceConnectionNumber (connection), F_GETFD) | FD_CLOEXEC);

      channel = g_io_channel_unix_new (IceConnectionNumber (connection));

      input_id = g_io_add_watch (channel,
                                 G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI,
                                 xsmp_process_ice_messages, session_client);

      g_io_channel_unref (channel);

      *watch_data = (IcePointer) GUINT_TO_POINTER (input_id);
    }
  else
    {
      input_id = GPOINTER_TO_UINT ((gpointer) *watch_data);

      g_source_remove (input_id);
    }
}

static void
xsmp_ice_io_error_handler (IceConn connection)
{
  g_warning ("ICE I/O Error");

  if (xsmp_ice_installed_handler)
    xsmp_ice_installed_handler (connection);
}

static void
xfce_session_client_handle_save_yourself (XfceSessionClientXsmp *session_client,
                                          gboolean do_quit_requested,
                                          int dialog_type,
                                          gboolean do_save_state)
{
  if (do_quit_requested
      && (g_signal_has_handler_pending (G_OBJECT (session_client),
                                        _xfce_session_client_class_quit_requested_signal_id (),
                                        0, FALSE)
          || g_signal_has_handler_pending (G_OBJECT (session_client),
                                           _xfce_session_client_class_save_state_extended_signal_id (),
                                           0, FALSE)))
    {
      Status status;

      status = SmcInteractRequest (session_client->session_connection,
                                   dialog_type, xsmp_interact,
                                   (SmPointer) session_client);

      if (status)
        {
          _xfce_session_client_set_state (XFCE_SESSION_CLIENT (session_client), XFCE_SESSION_CLIENT_STATE_WAITING_FOR_INTERACT);
          session_client->needs_save_state = do_save_state;
          /* we want save-state to happen *after* quit-requested if we're
           * doing both, but we can't do quit-requested until we hear
           * about our interact request */
          return;
        }
      else
        {
          /* if the interact request failed, fall through and at least
           * attempt to save state below if appropriate */
          g_warning ("SmcInteractRequest failed!");
        }
    }

  if (do_save_state)
    g_signal_emit_by_name (G_OBJECT (session_client), "save-state");

  if (session_client->shutdown_cancelled)
    {
      /* this is a slightly bizarre case that probably won't happen.
       * if we got to this point, then we didn't do quit-requested,
       * but the system was shutting down, and then it was later
       * cancelled.  since we never did a quit-requested, the client
       * probably won't expect a quit-cancelled, so we do nothing here. */
      session_client->shutdown_cancelled = FALSE;
      _xfce_session_client_set_state (XFCE_SESSION_CLIENT (session_client), XFCE_SESSION_CLIENT_STATE_IDLE);
    }
  else
    {
      /* otherwise, we're just done with the SaveYourself here */
      SmcSaveYourselfDone (session_client->session_connection, True);
      /* the XSMP spec state diagram says to go right back to IDLE after a
       * non-shutdown SaveYourself, but everything else in the spec disagrees:
       * we need to wait for a SaveComplete before going back to IDLE */
      _xfce_session_client_set_state (XFCE_SESSION_CLIENT (session_client), XFCE_SESSION_CLIENT_STATE_FROZEN);
    }
}

static void
xsmp_save_phase_2 (SmcConn smc_conn,
                   SmPointer client_data)
{
  XfceSessionClientXsmp *session_client = XFCE_SESSION_CLIENT_XSMP (client_data);

  TRACE ("entering");

  /* As a simplification, we won't support interacting in save phase 2,
   * even though XSMP allows an interact request (errors only) from
   * phase 2.  In our SM client's terminolgy, we support save-state
   * but not quit-requested for phase 2. */

  XfceSessionClientState cur_state = _xfce_session_client_get_state (XFCE_SESSION_CLIENT (session_client));
  if (cur_state != XFCE_SESSION_CLIENT_STATE_WAITING_FOR_PHASE_2)
    {
      g_warning ("Got SaveYourselfPhase2 in state %s, ignoring",
                 _xfce_session_client_state_to_string (cur_state));
      SmcSaveYourselfDone (session_client->session_connection, True);
      _xfce_session_client_set_state (XFCE_SESSION_CLIENT (session_client), XFCE_SESSION_CLIENT_STATE_FROZEN);
      return;
    }

  _xfce_session_client_set_state (XFCE_SESSION_CLIENT (session_client), XFCE_SESSION_CLIENT_STATE_SAVING_PHASE_2);

  g_signal_emit_by_name (G_OBJECT (session_client), "save-state-extended");

  SmcSaveYourselfDone (session_client->session_connection, True);
  /* the XSMP spec state diagram says to go right back to IDLE after a
   * non-shutdown SaveYourself, but everything else in the spec disagrees:
   * we need to wait for a SaveComplete before going back to IDLE */
  _xfce_session_client_set_state (XFCE_SESSION_CLIENT (session_client), XFCE_SESSION_CLIENT_STATE_FROZEN);

  if (session_client->shutdown_cancelled)
    {
      /* if we get here, we received ShutdownCancelled while in a recursive
       * invocation of the main loop in save-state-extended.  in this case, we
       * go back to idle and send quit-cancelled. */
      session_client->shutdown_cancelled = FALSE;

      _xfce_session_client_set_state (XFCE_SESSION_CLIENT (session_client), XFCE_SESSION_CLIENT_STATE_IDLE);
      g_signal_emit_by_name (G_OBJECT (session_client), "quit-cancelled");
    }
}

static void
xsmp_save_yourself (SmcConn smc_conn,
                    SmPointer client_data,
                    int save_style,
                    Bool shutdown,
                    int interact_style,
                    Bool fast)
{
  XfceSessionClientXsmp *session_client = XFCE_SESSION_CLIENT_XSMP (client_data);
  gboolean do_save_state, do_quit_requested;

  TRACE ("entering (save_style=%s, shutdown=%s, interact_style=%s, fast=%s",
         save_style == SmSaveGlobal ? "global" : (save_style == SmSaveLocal ? "local" : "both"),
         shutdown ? "true" : "false",
         interact_style == SmInteractStyleNone ? "none" : (interact_style == SmInteractStyleErrors ? "errors" : "any"),
         fast ? "true" : "false");

  /* The first SaveYourself after registering for the first time
   * is a special case (SM specs 7.2).
   */
  XfceSessionClientState cur_state = _xfce_session_client_get_state (XFCE_SESSION_CLIENT (session_client));
  if (cur_state == XFCE_SESSION_CLIENT_STATE_REGISTERING)
    {
      if (save_style == SmSaveLocal
          && interact_style == SmInteractStyleNone
          && !shutdown
          && !fast)
        {
          xfce_session_client_xsmp_sync_commands (XFCE_SESSION_CLIENT (session_client));
          SmcSaveYourselfDone (session_client->session_connection, True);
          /* XSMP spec state diagram says idle, but the rest of the spec disagrees */
          _xfce_session_client_set_state (XFCE_SESSION_CLIENT (session_client), XFCE_SESSION_CLIENT_STATE_FROZEN);
        }
      else
        {
          g_warning ("Initial SaveYourself had unexpected parameters");
          _xfce_session_client_set_state (XFCE_SESSION_CLIENT (session_client), XFCE_SESSION_CLIENT_STATE_IDLE);
        }

      return;
    }

  /* the spec says we can receive a SaveYourself even if we
   * haven't responded with SaveYourselfDone to a previous
   * SaveYourself.  in that case, we're supposed to immediately end
   * the previous SaveYourself and start handling the new one.  that's
   * a bit of a pain, so we're just gonna try to fixup our state on
   * this side and let things go. */
  cur_state = _xfce_session_client_get_state (XFCE_SESSION_CLIENT (session_client));
  if (cur_state != XFCE_SESSION_CLIENT_STATE_IDLE
      && cur_state != XFCE_SESSION_CLIENT_STATE_FROZEN)
    {
      SmcSaveYourselfDone (session_client->session_connection, True);
      _xfce_session_client_set_state (XFCE_SESSION_CLIENT (session_client), XFCE_SESSION_CLIENT_STATE_FROZEN);
      return;
    }

  _xfce_session_client_set_state (XFCE_SESSION_CLIENT (session_client), XFCE_SESSION_CLIENT_STATE_SAVING_PHASE_1);

  /* Most of this logic is taken from EggSMClient.  There are many
   * combinations of parameters to the SaveYourself request, but we're
   * going to boil them down to a few possibilities:
   *
   * 1.  Do nothing if:
   *     a) Global save and we're not shutting down, as this is
   *        somewhat pointless.
   *     b) Global save and we're shutting down, but the SM isn't
   *        going to let us interact, because we can't do anything
   *        anyway.
   * 2.  Emit save-state if:
   *     a) Local (or Both) save and we're not shutting down.  We
   *        ignore the Global part of the Both save since prompting
   *        to save files when not shutting down is stupid.
   *     b) Local (or Both) save and we *are* shutting down, but
   *        aren't allowed to interact.  Here a save-state is just
   *        the best we can do.  We ignore the Global part of a
   *        Both save because it's unlikely we can (or should) save
   *        without user interaction.
   * 3.  Emit quit-requested if:
   *     a) Global save, we're shutting down, and we're allowed to
   *        interact.
   * 4.  Emit quit-requested followed by save-state if:
   *     a) Local or Both save, we're shutting down, and we're
   *        allowed to interact.  If it's a Local save, we promote
   *        it to a Both save since shutting down without asking
   *        the user to save their work when it's allowed is rude.
   *
   * We ignore the 'fast' parameter.  I really don't expect any
   * client to do anything differently based on its value.
   */

  do_quit_requested = (shutdown && interact_style != SmInteractStyleNone);
  do_save_state = (save_style != SmSaveGlobal);

  xfce_session_client_handle_save_yourself (session_client, do_quit_requested,
                                            (interact_style == SmInteractStyleAny
                                               ? SmDialogNormal
                                               : SmDialogError),
                                            do_save_state);
}

static void
xsmp_die (SmcConn smc_conn,
          SmPointer client_data)
{
  XfceSessionClientXsmp *session_client = XFCE_SESSION_CLIENT_XSMP (client_data);

  TRACE ("entering");

  xfce_session_client_disconnect (XFCE_SESSION_CLIENT (session_client));

  /* here we give the app a chance to gracefully quit.  if the app
   * has indicated it doesn't want to (by not connecting to the quit
   * signal), then we force the exit here. */

  if (g_signal_has_handler_pending (G_OBJECT (session_client),
                                    _xfce_session_client_class_quit_signal_id (),
                                    0, FALSE))
    {
      g_signal_emit_by_name (G_OBJECT (session_client), "quit");
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
xsmp_save_complete (SmcConn smc_conn,
                    SmPointer client_data)
{
  XfceSessionClientXsmp *session_client = XFCE_SESSION_CLIENT_XSMP (client_data);

  TRACE ("entering");

  XfceSessionClientState cur_state = _xfce_session_client_get_state (XFCE_SESSION_CLIENT (session_client));
  if (cur_state != XFCE_SESSION_CLIENT_STATE_FROZEN)
    {
      g_warning ("Got SaveComplete in state %s, ignoring",
                 _xfce_session_client_state_to_string (cur_state));
    }

  _xfce_session_client_set_state (XFCE_SESSION_CLIENT (session_client), XFCE_SESSION_CLIENT_STATE_IDLE);
}

static void
xsmp_shutdown_cancelled (SmcConn smc_conn,
                         SmPointer client_data)
{
  XfceSessionClientXsmp *session_client = XFCE_SESSION_CLIENT_XSMP (client_data);

  TRACE ("entering");

  XfceSessionClientState cur_state = _xfce_session_client_get_state (XFCE_SESSION_CLIENT (session_client));
  switch (cur_state)
    {
    case XFCE_SESSION_CLIENT_STATE_FROZEN:
    case XFCE_SESSION_CLIENT_STATE_WAITING_FOR_PHASE_2:
      /* if the client has already handled quit-requested, we just go
       * back to idle and inform the client that shutdown was
       * cancelled. */
      _xfce_session_client_set_state (XFCE_SESSION_CLIENT (session_client), XFCE_SESSION_CLIENT_STATE_IDLE);
      g_signal_emit_by_name (G_OBJECT (session_client), "quit-cancelled");
      break;

    case XFCE_SESSION_CLIENT_STATE_WAITING_FOR_INTERACT:
      /* if we're waiting to interact (thus waiting to send
       * quit-requested), we just cancel that, and in this case we finish
       * the SaveYourself and move on.  we don't inform the client of
       * the cancellation since we haven't gotten to inform them about
       * the shutdown in the first place. */
      SmcSaveYourselfDone (session_client->session_connection, True);
      _xfce_session_client_set_state (XFCE_SESSION_CLIENT (session_client), XFCE_SESSION_CLIENT_STATE_IDLE);
      break;

    case XFCE_SESSION_CLIENT_STATE_INTERACTING:
    case XFCE_SESSION_CLIENT_STATE_SAVING_PHASE_1:
    case XFCE_SESSION_CLIENT_STATE_SAVING_PHASE_2:
      /* this should only happen if the client is currently *inside* one
       * of the quit-requested, save-state, or save-state-extended
       * handlers and is presumably inside a recursive invocation of the
       * main loop.  we shouldn't inform them of anything just yet, but
       * we'll set a flag so we can do so when they return. */
      session_client->shutdown_cancelled = TRUE;
      break;

    default:
      g_warning ("Got ShutdownCancelled in state %s, ignoring",
                 _xfce_session_client_state_to_string (cur_state));
      _xfce_session_client_set_state (XFCE_SESSION_CLIENT (session_client), XFCE_SESSION_CLIENT_STATE_IDLE);
      break;
    }
}

static void
xsmp_interact (SmcConn smc_conn,
               SmPointer client_data)
{
  XfceSessionClientXsmp *session_client = XFCE_SESSION_CLIENT_XSMP (client_data);
  gboolean cancel = FALSE;

  TRACE ("entering");

  /* since we don't support interacting during phase 2, the only time
   * we should receive an interact message is during a normal save
   * yourself. */

  XfceSessionClientState cur_state = _xfce_session_client_get_state (XFCE_SESSION_CLIENT (session_client));
  if (cur_state != XFCE_SESSION_CLIENT_STATE_WAITING_FOR_INTERACT)
    {
      g_warning ("Got Interact message in state %s, ignoring",
                 _xfce_session_client_state_to_string (cur_state));
      SmcInteractDone (session_client->session_connection, False);
      SmcSaveYourselfDone (session_client->session_connection, True);
      _xfce_session_client_set_state (XFCE_SESSION_CLIENT (session_client), XFCE_SESSION_CLIENT_STATE_FROZEN);
      return;
    }

  _xfce_session_client_set_state (XFCE_SESSION_CLIENT (session_client), XFCE_SESSION_CLIENT_STATE_INTERACTING);

  /* at this point we're ready to emit quit-requested */
  g_signal_emit_by_name (G_OBJECT (session_client), "quit-requested", &cancel);

  if (session_client->shutdown_cancelled)
    {
      /* if we get here, we received ShutdownCancelled while in a recursive
       * invocation of the main loop in quit-requested.  in this case, we
       * go back to idle and send quit-cancelled. */
      session_client->shutdown_cancelled = FALSE;
      cancel = TRUE;

      _xfce_session_client_set_state (XFCE_SESSION_CLIENT (session_client), XFCE_SESSION_CLIENT_STATE_IDLE);
      g_signal_emit_by_name (G_OBJECT (session_client), "quit-cancelled");
    }
  else
    {
      /* we only send InteractDone if we didn't get ShutdownCancelled */
      _xfce_session_client_set_state (XFCE_SESSION_CLIENT (session_client), XFCE_SESSION_CLIENT_STATE_FROZEN);
      SmcInteractDone (session_client->session_connection, cancel);
    }

  if (cancel)
    {
      /* if we requested (or got) a shutdown cancellation, we skip the
       * save-state signal. */
      session_client->needs_save_state = FALSE;
    }
  else if (session_client->needs_save_state)
    {
      /* if we still have a pending save-state, send that now */
      session_client->needs_save_state = FALSE;
      g_signal_emit_by_name (G_OBJECT (session_client), "save-state");

      if (session_client->shutdown_cancelled)
        {
          /* this is exceedingly unlikely, but it could happen here too */
          session_client->shutdown_cancelled = FALSE;
          cancel = TRUE;

          _xfce_session_client_set_state (XFCE_SESSION_CLIENT (session_client), XFCE_SESSION_CLIENT_STATE_IDLE);
          g_signal_emit_by_name (G_OBJECT (session_client), "quit-cancelled");
        }
    }

  /* if the app also wants a phase 2 save, we request it now */
  if (!cancel && g_signal_has_handler_pending (G_OBJECT (session_client), _xfce_session_client_class_save_state_extended_signal_id (), 0, FALSE))
    {
      Status status;

      status = SmcRequestSaveYourselfPhase2 (session_client->session_connection,
                                             xsmp_save_phase_2,
                                             (SmPointer) session_client);
      if (status)
        {
          _xfce_session_client_set_state (XFCE_SESSION_CLIENT (session_client), XFCE_SESSION_CLIENT_STATE_WAITING_FOR_PHASE_2);
          return;
        }
    }

  /* finally, signal to the SM that we're done.  we also fall through and
   * bail to this if a phase2 request failed for some reason. */
  SmcSaveYourselfDone (session_client->session_connection, True);
}

static void
xfce_session_client_xsmp_set_command_property (XfceSessionClientXsmp *session_client,
                                               const char *property_name,
                                               gchar **command)
{
  TRACE ("entering (%s)", property_name);

  if (command != NULL && command[0] != NULL && session_client->session_connection != NULL)
    {
      SmProp prop, *props[1];
      SmPropValue *vals;
      gint argc;

      for (argc = 0; command[argc] != NULL; ++argc)
        ;

      vals = g_new (SmPropValue, argc);
      for (gint i = 0; i < argc; ++i)
        {
          vals[i].length = strlen (command[i]);
          vals[i].value = command[i];
        }

      prop.name = (char *) property_name;
      prop.type = SmLISTofARRAY8;
      prop.vals = vals;
      prop.num_vals = argc;

      props[0] = &prop;
      DBG ("setting %s", property_name);
      SmcSetProperties (session_client->session_connection, 1, props);

      g_free (vals);
    }
}

static void
xfce_session_client_xsmp_sync_commands (XfceSessionClient *session_client)
{
  XfceSessionClientXsmp *xsession_client = XFCE_SESSION_CLIENT_XSMP (session_client);

  TRACE ("entering");

  if (xsession_client->session_connection != NULL)
    {
      gchar **clone_command = _xfce_session_client_dup_effective_clone_command (session_client);
      gchar **restart_command = _xfce_session_client_dup_effective_restart_command (session_client);
      gchar **discard_command = _xfce_session_client_dup_effective_discard_command (session_client);

      xfce_session_client_xsmp_set_command_property (xsession_client, SmCloneCommand, clone_command);
      xfce_session_client_xsmp_set_command_property (xsession_client, SmRestartCommand, restart_command);
      xfce_session_client_xsmp_set_command_property (xsession_client, SmDiscardCommand, discard_command);

      g_strfreev (clone_command);
      g_strfreev (restart_command);
      g_strfreev (discard_command);
    }
}

static gboolean
xfce_session_client_xsmp_connect (XfceSessionClient *session_client,
                                  GError **error)
{
  XfceSessionClientXsmp *xsession_client = XFCE_SESSION_CLIENT_XSMP (session_client);
  char buf[256] = "";
  unsigned long mask;
  SmcCallbacks callbacks;
  SmProp prop1, prop2, prop3, prop4, prop5, prop6, prop7, *props[7];
  SmPropValue prop1val, prop2val, prop3val, prop4val, prop5val, prop6val, prop7val;
  int n_props = 0;
  char pid[32];
  unsigned char hint = SmRestartIfRunning;
  char *given_client_id = NULL;
  IceIOErrorHandler default_handler;

  if (xsession_client->session_connection != NULL)
    return TRUE;

  xsmp_ice_installed_handler = IceSetIOErrorHandler (NULL);
  default_handler = IceSetIOErrorHandler (xsmp_ice_io_error_handler);
  if (xsmp_ice_installed_handler == default_handler)
    xsmp_ice_installed_handler = NULL;

  IceAddConnectionWatch (xsmp_new_ice_connection, session_client);

  mask = SmcSaveYourselfProcMask | SmcDieProcMask | SmcSaveCompleteProcMask
         | SmcShutdownCancelledProcMask;

  callbacks.save_yourself.callback = xsmp_save_yourself;
  callbacks.save_yourself.client_data = (SmPointer) session_client;

  callbacks.die.callback = xsmp_die;
  callbacks.die.client_data = (SmPointer) session_client;

  callbacks.save_complete.callback = xsmp_save_complete;
  callbacks.save_complete.client_data = (SmPointer) session_client;

  callbacks.shutdown_cancelled.callback = xsmp_shutdown_cancelled;
  callbacks.shutdown_cancelled.client_data = (SmPointer) session_client;

  const gchar *client_id = xfce_session_client_get_client_id (session_client);
  xsession_client->session_connection = SmcOpenConnection (NULL, NULL,
                                                           SmProtoMajor,
                                                           SmProtoMinor,
                                                           mask,
                                                           &callbacks,
                                                           (char *) client_id,
                                                           &given_client_id,
                                                           sizeof (buf) - 1,
                                                           buf);

  if (!xsession_client->session_connection)
    {
      if (error)
        {
          g_set_error (error, XFCE_SESSION_CLIENT_ERROR, XFCE_SESSION_CLIENT_ERROR_FAILED,
                       _( "Failed to connect to the session manager: %s"), buf);
        }
      goto out_err;
    }
  else if (!given_client_id)
    {
      if (error)
        {
          g_set_error (error, XFCE_SESSION_CLIENT_ERROR, XFCE_SESSION_CLIENT_ERROR_INVALID_CLIENT,
                       _( "Session manager did not return a valid client id"));
        }
      goto out_err;
    }

  if (client_id != NULL
      && strcmp (client_id, given_client_id) == 0)
    {
      _xfce_session_client_set_state (session_client, XFCE_SESSION_CLIENT_STATE_IDLE);
      _xfce_session_client_set_is_resumed (session_client, TRUE);
    }
  else
    {
      _xfce_session_client_set_client_id (session_client, given_client_id);
      _xfce_session_client_set_state (session_client, XFCE_SESSION_CLIENT_STATE_REGISTERING);
    }

  free (given_client_id);

  hint = _xfce_session_client_restart_style_to_xsmp (xfce_session_client_get_restart_style (session_client));

  prop1.name = SmProgram;
  prop1.type = SmARRAY8;
  prop1.num_vals = 1;
  prop1.vals = &prop1val;
  prop1val.value = (char *) g_get_prgname ();
  if (G_UNLIKELY (!prop1val.value))
    prop1val.value = "<unknown program>";
  prop1val.length = strlen (prop1val.value);
  n_props++;

  prop2.name = SmUserID;
  prop2.type = SmARRAY8;
  prop2.num_vals = 1;
  prop2.vals = &prop2val;
  prop2val.value = (char *) g_get_user_name ();
  prop2val.length = strlen (prop2val.value);
  n_props++;

  prop3.name = SmRestartStyleHint;
  prop3.type = SmCARD8;
  prop3.num_vals = 1;
  prop3.vals = &prop3val;
  prop3val.value = &hint;
  prop3val.length = 1;
  n_props++;

  g_snprintf (pid, sizeof (pid), "%d", getpid ());
  prop4.name = SmProcessID;
  prop4.type = SmARRAY8;
  prop4.num_vals = 1;
  prop4.vals = &prop4val;
  prop4val.value = pid;
  prop4val.length = strlen (prop4val.value);
  n_props++;

  prop5.name = SmCurrentDirectory;
  prop5.type = SmARRAY8;
  prop5.num_vals = 1;
  prop5.vals = &prop5val;
  prop5val.value = (char *) xfce_session_client_get_current_directory (session_client);
  prop5val.length = strlen (prop5val.value);
  n_props++;

  prop6.name = GsmPriority;
  prop6.type = SmCARD8;
  prop6.num_vals = 1;
  prop6.vals = &prop6val;
  guint8 priority = xfce_session_client_get_priority (session_client);
  prop6val.value = &priority;
  prop6val.length = 1;
  n_props++;

  const gchar *desktop_file = _xfce_session_client_peek_desktop_file (session_client);
  if (desktop_file != NULL)
    {
      prop7.name = GsmDesktopFile;
      prop7.type = SmARRAY8;
      prop7.num_vals = 1;
      prop7.vals = &prop7val;
      prop7val.value = (char *) desktop_file;
      prop7val.length = strlen (desktop_file);
      n_props++;
    }

  props[0] = &prop1;
  props[1] = &prop2;
  props[2] = &prop3;
  props[3] = &prop4;
  props[4] = &prop5;
  props[5] = &prop6;
  props[6] = &prop7;

  SmcSetProperties (xsession_client->session_connection, n_props, props);

  xfce_session_client_xsmp_sync_commands (session_client);

  return TRUE;

out_err:
  if (xsession_client->session_connection != NULL)
    {
      SmcCloseConnection (xsession_client->session_connection, 0, NULL);
      xsession_client->session_connection = NULL;
    }

  IceRemoveConnectionWatch (xsmp_new_ice_connection, xsession_client);
  IceSetIOErrorHandler (xsmp_ice_installed_handler);
  xsmp_ice_installed_handler = NULL;

  return FALSE;
}

static void
xfce_session_client_xsmp_disconnect (XfceSessionClient *session_client)
{
  XfceSessionClientXsmp *xsession_client = XFCE_SESSION_CLIENT_XSMP (session_client);

  if (G_UNLIKELY (!xsession_client->session_connection))
    {
      g_warning ("%s() called with no session connection", G_STRFUNC);
      return;
    }

  if (xfce_session_client_get_restart_style (session_client) == XFCE_SESSION_CLIENT_RESTART_IMMEDIATELY)
    xfce_session_client_set_restart_style (session_client, XFCE_SESSION_CLIENT_RESTART_NORMAL);

  SmcCloseConnection (xsession_client->session_connection, 0, NULL);
  xsession_client->session_connection = NULL;

  IceRemoveConnectionWatch (xsmp_new_ice_connection, xsession_client);
  IceSetIOErrorHandler (xsmp_ice_installed_handler);
  xsmp_ice_installed_handler = NULL;

  _xfce_session_client_set_state (session_client, XFCE_SESSION_CLIENT_STATE_DISCONNECTED);
}

static gboolean
xfce_session_client_xsmp_is_connected (XfceSessionClient *session_client)
{
  g_return_val_if_fail (XFCE_IS_SESSION_CLIENT (session_client), FALSE);
  return XFCE_SESSION_CLIENT_XSMP (session_client)->session_connection != NULL;
}

static void
xfce_session_client_xsmp_set_desktop_file (XfceSessionClient *session_client,
                                           const gchar *desktop_file)
{
  XfceSessionClientXsmp *xsession_client = XFCE_SESSION_CLIENT_XSMP (session_client);

  if (xsession_client->session_connection)
    {
      if (desktop_file != NULL)
        {
          SmProp prop, *props[1];
          SmPropValue propval;

          prop.name = GsmDesktopFile;
          prop.type = SmARRAY8;
          prop.num_vals = 1;
          prop.vals = &propval;
          propval.value = (char *) desktop_file;
          propval.length = strlen (desktop_file);
          props[0] = &prop;

          SmcSetProperties (xsession_client->session_connection, 1, props);
        }
      else
        {
          const char *prop_names[] = {
            GsmDesktopFile,
            NULL,
          };
          SmcDeleteProperties (xsession_client->session_connection, 1, (char **) prop_names);
        }
    }
}

static void
xfce_session_client_xsmp_request_shutdown (XfceSessionClient *session_client,
                                           XfceSessionClientShutdownHint shutdown_hint)
{
  XfceSessionClientXsmp *xsession_client = XFCE_SESSION_CLIENT_XSMP (session_client);

  if (G_LIKELY (xsession_client->session_connection))
    {
      SmcRequestSaveYourself (xsession_client->session_connection, SmSaveBoth,
                              True, SmInteractStyleAny, False, True);
    }
}

static void
xfce_session_client_xsmp_set_restart_style (XfceSessionClient *session_client,
                                            XfceSessionClientRestartStyle restart_style)
{
  XfceSessionClientXsmp *xsession_client = XFCE_SESSION_CLIENT_XSMP (session_client);

  if (xsession_client->session_connection)
    {
      SmProp prop, *props[1];
      SmPropValue propval;
      char hint;

      hint = _xfce_session_client_restart_style_to_xsmp (restart_style);

      prop.name = SmRestartStyleHint;
      prop.type = SmCARD8;
      prop.num_vals = 1;
      prop.vals = &propval;
      propval.value = &hint;
      propval.length = 1;
      props[0] = &prop;

      SmcSetProperties (xsession_client->session_connection, 1, props);
    }
}

static void
xfce_session_client_xsmp_set_priority (XfceSessionClient *session_client,
                                       guint8 priority)
{
  XfceSessionClientXsmp *xsession_client = XFCE_SESSION_CLIENT_XSMP (session_client);

  if (xsession_client->session_connection)
    {
      SmProp prop, *props[1];
      SmPropValue propval;

      prop.name = GsmPriority;
      prop.type = SmCARD8;
      prop.num_vals = 1;
      prop.vals = &propval;
      propval.value = &priority;
      propval.length = 1;
      props[0] = &prop;

      SmcSetProperties (xsession_client->session_connection, 1, props);
    }
}

static void
xfce_session_client_xsmp_set_current_directory (XfceSessionClient *session_client,
                                                const gchar *current_directory)
{
  XfceSessionClientXsmp *xsession_client = XFCE_SESSION_CLIENT_XSMP (session_client);

  if (xsession_client->session_connection)
    {
      SmProp prop, *props[1];
      SmPropValue propval;

      prop.name = SmCurrentDirectory;
      prop.type = SmARRAY8;
      prop.num_vals = 1;
      prop.vals = &propval;
      propval.value = (char *) (current_directory != NULL ? current_directory : g_get_home_dir ());
      propval.length = strlen (propval.value);
      props[0] = &prop;

      SmcSetProperties (xsession_client->session_connection, 1, props);
    }
}
