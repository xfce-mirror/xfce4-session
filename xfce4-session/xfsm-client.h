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

#ifndef __XFSM_CLIENT_H__
#define __XFSM_CLIENT_H__

#include <xfce4-session/xfsm-properties.h>

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
  

struct _XfsmClient
{
  XfsmClientState state;
  const gchar    *id;
  XfsmProperties *properties;
  SmsConn sms_conn;
  guint save_timeout_id;
};


#define XFSM_CLIENT(c) ((XfsmClient *) (c))


XfsmClient *xfsm_client_new (SmsConn sms_conn);

void xfsm_client_free (XfsmClient *client);

void xfsm_client_set_initial_properties (XfsmClient     *client,
                                         XfsmProperties *properties);

#endif /* !__XFSM_CLIENT_H__ */
