/* $Id$ */
/*-
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


G_INLINE_FUNC XfsmClient *xfsm_client_new (SmsConn sms_conn)
{
  XfsmClient *client;
  
  client = g_new0 (XfsmClient, 1);
  client->sms_conn = sms_conn;
  client->state = XFSM_CLIENT_IDLE;
  
  return client;
}

void xfsm_client_free (XfsmClient *client);

void xfsm_client_set_initial_properties (XfsmClient     *client,
                                         XfsmProperties *properties);
  
#endif /* !__XFSM_CLIENT_H__ */
