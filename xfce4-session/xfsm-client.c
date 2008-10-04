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

#include "xfsm-client.h"

struct _XfsmClient
{
  GObject parent;

  XfsmClientState state;
  gchar          *id;
  XfsmProperties *properties;
  SmsConn         sms_conn;
};

typedef struct _XfsmClientClass
{
  GObjectClass parent;
} XfsmClientClass;


static void xfsm_client_class_init (XfsmClientClass *klass);
static void xfsm_client_init (XfsmClient *client);
static void xfsm_client_finalize (GObject *obj);


G_DEFINE_TYPE(XfsmClient, xfsm_client, G_TYPE_OBJECT)


static void
xfsm_client_class_init (XfsmClientClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = xfsm_client_finalize;
}


static void
xfsm_client_init (XfsmClient *client)
{

}

static void
xfsm_client_finalize (GObject *obj)
{
  XfsmClient *client = XFSM_CLIENT (obj);

  if (client->properties != NULL)
    xfsm_properties_free (client->properties);

  g_free (client->id);

  G_OBJECT_CLASS (xfsm_client_parent_class)->finalize (obj);
}



XfsmClient*
xfsm_client_new (SmsConn sms_conn)
{
  XfsmClient *client;

  g_return_val_if_fail (sms_conn, NULL);

  client = g_object_new (XFSM_TYPE_CLIENT, NULL);

  client->sms_conn = sms_conn;
  client->state = XFSM_CLIENT_IDLE;

  return client;
}


void
xfsm_client_set_initial_properties (XfsmClient     *client,
                                    XfsmProperties *properties)
{
  g_return_if_fail (XFSM_IS_CLIENT (client));
  g_return_if_fail (properties != NULL);
  
  if (client->properties != NULL)
    xfsm_properties_free (client->properties);
  client->properties = properties;
  client->id = g_strdup (properties->client_id);
}


XfsmClientState
xfsm_client_get_state (XfsmClient *client)
{
  g_return_val_if_fail (XFSM_IS_CLIENT (client), XFSM_CLIENT_DISCONNECTED);
  return client->state;
}


void
xfsm_client_set_state (XfsmClient     *client,
                       XfsmClientState state)
{
  g_return_if_fail (XFSM_IS_CLIENT (client));
  client->state = state;
}


G_CONST_RETURN gchar *
xfsm_client_get_id (XfsmClient *client)
{
  g_return_val_if_fail (XFSM_IS_CLIENT (client), NULL);
  return client->id;
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

  g_return_val_if_fail(XFSM_IS_CLIENT (client), NULL);

  properties = client->properties;
  client->properties = NULL;

  return properties;
}
