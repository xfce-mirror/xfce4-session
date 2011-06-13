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

#include <glib-object.h>

#include <xfce4-session/xfsm-properties.h>

G_BEGIN_DECLS

#define XFSM_TYPE_CLIENT     (xfsm_client_get_type ())
#define XFSM_CLIENT(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), XFSM_TYPE_CLIENT, XfsmClient))
#define XFSM_IS_CLIENT(obj)  (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFSM_TYPE_CLIENT))

/* fwd decl */
struct _XfsmManager;

typedef struct _XfsmClient XfsmClient;

typedef enum
{
  XFSM_CLIENT_IDLE,
  XFSM_CLIENT_INTERACTING,
  XFSM_CLIENT_SAVEDONE,
  XFSM_CLIENT_SAVING,
  XFSM_CLIENT_SAVINGLOCAL,
  XFSM_CLIENT_WAITFORINTERACT,
  XFSM_CLIENT_WAITFORPHASE2,
  XFSM_CLIENT_DISCONNECTED,
} XfsmClientState;

GType xfsm_client_get_type (void) G_GNUC_CONST;

XfsmClient *xfsm_client_new (struct _XfsmManager *manager,
                             SmsConn              sms_conn);

void xfsm_client_set_initial_properties (XfsmClient     *client,
                                         XfsmProperties *properties);

XfsmClientState xfsm_client_get_state (XfsmClient *client);
void xfsm_client_set_state (XfsmClient     *client,
                            XfsmClientState state);

const gchar *xfsm_client_get_id (XfsmClient *client);

SmsConn xfsm_client_get_sms_connection (XfsmClient *client);

XfsmProperties *xfsm_client_get_properties (XfsmClient *client);
XfsmProperties *xfsm_client_steal_properties (XfsmClient *client);

void xfsm_client_merge_properties (XfsmClient *client,
                                   SmProp    **props,
                                   gint        num_props);
void xfsm_client_delete_properties (XfsmClient *client,
                                    gchar     **prop_names,
                                    gint        num_props);

const gchar *xfsm_client_get_object_path (XfsmClient *client);

G_END_DECLS

#endif /* !__XFSM_CLIENT_H__ */
