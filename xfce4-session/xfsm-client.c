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

#include <libxfsm/xfsm-util.h>

#include <xfce4-session/xfsm-client.h>
#include <xfce4-session/xfsm-global.h>


XfsmClient*
xfsm_client_new (SmsConn sms_conn)
{
  XfsmClient *client;
  
  client = g_new0 (XfsmClient, 1);
  client->sms_conn = sms_conn;
  client->state = XFSM_CLIENT_IDLE;
  
  return client;
}

void
xfsm_client_free (XfsmClient *client)
{
  g_return_if_fail (client != NULL);
  
  if (client->properties != NULL)
    xfsm_properties_free (client->properties);
  if (client->save_timeout_id > 0)
    g_source_remove (client->save_timeout_id);
  g_free (client);
}


void
xfsm_client_set_initial_properties (XfsmClient     *client,
                                    XfsmProperties *properties)
{
  g_return_if_fail (client != NULL);
  g_return_if_fail (properties != NULL);
  
  if (client->properties != NULL)
    xfsm_properties_free (client->properties);
  client->properties = properties;
  client->id = properties->client_id;
}



