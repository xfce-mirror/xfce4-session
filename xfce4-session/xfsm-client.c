/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <xfce4-session/xfsm-client.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-util.h>


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
  if (client->old_discard_command != NULL)
    g_strfreev (client->old_discard_command);
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


void
xfsm_client_remember_discard_command (XfsmClient *client)
{
  XfsmProperties *properties = client->properties;

  if (client->old_discard_command != NULL)
    {
      g_strfreev (client->old_discard_command);
      client->old_discard_command = NULL;
    }

  if (properties != NULL && properties->discard_command != NULL)
    {
      client->old_discard_command = xfsm_strv_copy(properties->discard_command);
    }
}


void
xfsm_client_maybe_run_old_discard_command (XfsmClient *client)
{
  gchar **old_discard_command = client->old_discard_command;
  XfsmProperties *properties = client->properties;

  if (old_discard_command != NULL)
    {
      if (!xfsm_strv_equal (old_discard_command, properties->discard_command))
        {
          xfsm_verbose ("Client Id = %s, running old discard command.\n\n",
                        client->id);

          g_spawn_sync (properties->current_directory,
                        old_discard_command,
                        NULL,
                        G_SPAWN_SEARCH_PATH,
                        NULL, NULL,
                        NULL, NULL,
                        NULL, NULL);
        }

      g_strfreev (client->old_discard_command);
      client->old_discard_command = NULL;
    }
}


