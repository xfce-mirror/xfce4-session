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

#ifndef __XFCE_SESSION_CLIENT_PRIVATE_H__
#define __XFCE_SESSION_CLIENT_PRIVATE_H__

#include "xfce-session-client.h"

#ifdef ENABLE_X11
#include <X11/SM/SM.h>
#endif

#ifndef SmCloneCommand
#define SmCloneCommand "CloneCommand"
#define SmCurrentDirectory "CurrentDirectory"
#define SmDiscardCommand "DiscardCommand"
#define SmEnvironment "Environment"
#define SmProcessID "ProcessID"
#define SmProgram "Program"
#define SmRestartCommand "RestartCommand"
#define SmResignCommand "ResignCommand"
#define SmRestartStyleHint "RestartStyleHint"
#define SmShutdownCommand "ShutdownCommand"
#define SmUserID "UserID"

#define SmRestartIfRunning 0
#define SmRestartAnyway 1
#define SmRestartImmediately 2
#define SmRestartNever 3
#endif

#define GsmPriority "_GSM_Priority"
#define GsmDesktopFile "_GSM_DesktopFile"

typedef enum
{
  XFCE_SESSION_CLIENT_STATE_DISCONNECTED = 0,
  XFCE_SESSION_CLIENT_STATE_REGISTERING,
  XFCE_SESSION_CLIENT_STATE_IDLE,
  XFCE_SESSION_CLIENT_STATE_SAVING_PHASE_1,
  XFCE_SESSION_CLIENT_STATE_WAITING_FOR_INTERACT,
  XFCE_SESSION_CLIENT_STATE_INTERACTING,
  XFCE_SESSION_CLIENT_STATE_WAITING_FOR_PHASE_2,
  XFCE_SESSION_CLIENT_STATE_SAVING_PHASE_2,
  XFCE_SESSION_CLIENT_STATE_FROZEN,
} XfceSessionClientState;

struct _XfceSessionClientClass
{
  GObjectClass parent_class;

  /*< vfuncs >*/

  gboolean (*connect) (XfceSessionClient *session_client,
                       GError **error);

  void (*disconnect) (XfceSessionClient *session_client);

  void (*request_shutdown) (XfceSessionClient *session_client,
                            XfceSessionClientShutdownHint shutdown_hint);

  gboolean (*is_connected) (XfceSessionClient *session_client);

  void (*set_desktop_file) (XfceSessionClient *session_client,
                            const gchar *desktop_file);

  void (*set_restart_style) (XfceSessionClient *session_client,
                             XfceSessionClientRestartStyle restart_style);

  void (*set_priority) (XfceSessionClient *session_client,
                        guint8 priority);

  void (*set_current_directory) (XfceSessionClient *session_client,
                                 const gchar *current_directory);

  /* Called whenever the clone, restart, or discard commands change, or when
   * the client ID they embed changes.  Implementations should (re)send all
   * three, as returned by the _xfce_session_client_dup_effective_*() accessors. */
  void (*sync_commands) (XfceSessionClient *session_client);

  void (*add_window) (XfceSessionClient *session_client,
                      GtkWindow *window,
                      const gchar *name);
  void (*restore_window) (XfceSessionClient *session_client,
                          GtkWindow *window,
                          const gchar *name);
  void (*rename_window) (XfceSessionClient *session_client,
                         GtkWindow *window,
                         const gchar *new_name);
  void (*remove_window) (XfceSessionClient *session_client,
                         const gchar *name);
};

guint
_xfce_session_client_class_quit_requested_signal_id (void);
guint
_xfce_session_client_class_save_state_extended_signal_id (void);
guint
_xfce_session_client_class_quit_signal_id (void);

void
_xfce_session_client_set_client_id (XfceSessionClient *session_client,
                                    const gchar *client_id);

void
_xfce_session_client_set_state (XfceSessionClient *session_client,
                                XfceSessionClientState state);
XfceSessionClientState
_xfce_session_client_get_state (XfceSessionClient *session_client);

const gchar *
_xfce_session_client_state_to_string (XfceSessionClientState state);

void
_xfce_session_client_set_is_resumed (XfceSessionClient *session_client,
                                     gboolean is_resumed);

const gchar *
_xfce_session_client_peek_state_file (XfceSessionClient *session_client);

const gchar *
_xfce_session_client_peek_desktop_file (XfceSessionClient *session_client);

gchar **
_xfce_session_client_dup_effective_clone_command (XfceSessionClient *session_client);
gchar **
_xfce_session_client_dup_effective_restart_command (XfceSessionClient *session_client);
gchar **
_xfce_session_client_dup_effective_discard_command (XfceSessionClient *session_client);

static inline char
_xfce_session_client_restart_style_to_xsmp (XfceSessionClientRestartStyle style)
{
  switch (style)
    {
    case XFCE_SESSION_CLIENT_RESTART_IMMEDIATELY:
      return SmRestartImmediately;
    case XFCE_SESSION_CLIENT_RESTART_NORMAL:
    default:
      return SmRestartIfRunning;
    }
}


#endif
