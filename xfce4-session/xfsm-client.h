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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 */

#ifndef __XFSM_CLIENT_H__
#define __XFSM_CLIENT_H__

#include <gio/gio.h>
#include <glib-object.h>

#include "xfsm-client-dbus.h"
#include "xfsm-properties.h"

G_BEGIN_DECLS

#define XFSM_TYPE_CLIENT (xfsm_client_get_type ())
G_DECLARE_FINAL_TYPE (XfsmClient, xfsm_client, XFSM, CLIENT, XfsmDbusClientSkeleton)

/* fwd decl */
typedef struct _XfsmManager XfsmManager;

typedef enum
{
  XFSM_CLIENT_IDLE = 0,
  XFSM_CLIENT_INTERACTING,
  XFSM_CLIENT_SAVEDONE,
  XFSM_CLIENT_SAVING,
  XFSM_CLIENT_SAVINGLOCAL,
  XFSM_CLIENT_WAITFORINTERACT,
  XFSM_CLIENT_WAITFORPHASE2,
  XFSM_CLIENT_DISCONNECTED,
  XFSM_CLIENT_STATE_COUNT
} XfsmClientState;

typedef enum
{
  XFSM_START_REASON_LAUNCH = 0,
  XFSM_START_REASON_RECOVER,
  XFSM_START_REASON_SESSION_RESTORE,
} XfsmStartReason;

typedef enum
{
  XFSM_SESSION_STATUS_CREATED = 0,
  XFSM_SESSION_STATUS_RESTORED,
  XFSM_SESSION_STATUS_REPLACED,
} XfsmSessionStatus;

typedef enum
{
  XFSM_CLIENT_SET_PID_FLAGS_NONE = 0,
  XFSM_CLIENT_SET_PID_FLAGS_UPDATE_RESTART_COMMAND = (1 << 0),
  XFSM_CLIENT_SET_PID_FLAGS_UPDATE_PROGRAM_NAME = (1 << 1),
} XfsmClientSetPidFlags;

gchar *
xfsm_client_generate_id (SmsConn sms_conn) G_GNUC_PURE;

XfsmClient *
xfsm_client_new (XfsmManager *manager,
                 SmsConn sms_conn,
                 GDBusConnection *connection);

void
xfsm_client_set_initial_properties (XfsmClient *client,
                                    XfsmProperties *properties);

XfsmClientState
xfsm_client_get_state (XfsmClient *client);
void
xfsm_client_set_state (XfsmClient *client,
                       XfsmClientState state);

const gchar *
xfsm_client_get_id (XfsmClient *client);
const gchar *
xfsm_client_get_app_id (XfsmClient *client);

SmsConn
xfsm_client_get_sms_connection (XfsmClient *client);

XfsmProperties *
xfsm_client_get_properties (XfsmClient *client);
XfsmProperties *
xfsm_client_steal_properties (XfsmClient *client);

#ifdef ENABLE_X11
void
xfsm_client_merge_properties (XfsmClient *client,
                              SmProp **props,
                              gint num_props);
#endif
void
xfsm_client_delete_properties (XfsmClient *client,
                               gchar **prop_names,
                               gint num_props);

const gchar *
xfsm_client_get_object_path (XfsmClient *client);

pid_t
xfsm_client_get_pid (XfsmClient *client);
void
xfsm_client_set_pid (XfsmClient *client,
                     pid_t pid,
                     XfsmClientSetPidFlags flags);

void
xfsm_client_set_app_id (XfsmClient *client,
                        const gchar *app_id);

void
xfsm_client_set_service_name (XfsmClient *client,
                              const gchar *service_name);
const gchar *
xfsm_client_get_service_name (XfsmClient *client);

void
xfsm_client_set_start_reason (XfsmClient *client,
                              XfsmStartReason reason);
XfsmStartReason
xfsm_client_get_start_reason (XfsmClient *client);

void
xfsm_client_set_session_status (XfsmClient *client,
                                XfsmSessionStatus status);
XfsmSessionStatus
xfsm_client_get_session_status (XfsmClient *client);

gboolean
xfsm_client_is_xfsm_aware (XfsmClient *client);

void
xfsm_client_terminate (XfsmClient *client);

void
xfsm_client_end_session (XfsmClient *client);

void
xfsm_client_cancel_shutdown (XfsmClient *client);

XfsmStartReason
xfsm_start_reason_parse (const gchar *reason_str);
const gchar *
xfsm_start_reason_to_string (XfsmStartReason reason);

const gchar *
xfsm_session_status_to_string (XfsmSessionStatus status);

G_END_DECLS

#endif /* !__XFSM_CLIENT_H__ */
