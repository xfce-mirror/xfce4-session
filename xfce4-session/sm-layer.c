/* $Id */
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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <X11/ICE/ICElib.h>
#include <X11/SM/SMlib.h>

#include <libxfce4util/libxfce4util.h>

#include <xfce4-session/ice-layer.h>
#include <xfce4-session/sm-layer.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-manager.h>


/* local prototypes */
static Status sm_new_client                 (SmsConn         sms_conn,
                                             SmPointer       manager_data,
                                             unsigned long  *mask,
                                             SmsCallbacks   *callbacks,
                                             char          **failure_reason);
static Status sm_register_client            (SmsConn         sms_conn,
                                             SmPointer       client_data,
                                             char           *previous_id);
static void   sm_interact_request           (SmsConn         sms_conn,
                                             SmPointer       client_data,
                                             int             dialog_type);
static void   sm_interact_done              (SmsConn         sms_conn,
                                             SmPointer       client_data,
                                             Bool            cancel_shutdown);
static void   sm_save_yourself_request      (SmsConn         sms_conn,
                                             SmPointer       client_data,
                                             int             save_type,
                                             Bool            shutdown,
                                             int             interact_style,
                                             Bool            fast,
                                             Bool            global);
static void   sm_save_yourself_phase2_request(SmsConn         sms_conn,
                                             SmPointer       client_data);
static void   sm_save_yourself_done         (SmsConn         sms_conn,
                                             SmPointer       client_data,
                                             Bool            success);
static void   sm_close_connection           (SmsConn         sms_conn,
                                             SmPointer       client_data,
                                             int             num_reasons,
                                             char          **reasons);
static void   sm_set_properties             (SmsConn         sms_conn,
                                             SmPointer       client_data,
                                             int             num_props,
                                             SmProp        **props);
static void   sm_delete_properties          (SmsConn         sms_conn,
                                             SmPointer       client_data,
                                             int             num_props,
                                             char          **prop_names);
static void   sm_get_properties             (SmsConn         sms_conn,
                                             SmPointer       client_data);


#define SET_CALLBACK(_callbacks, _callback, _client)          \
G_STMT_START{                                                 \
  _callbacks->_callback.callback     = sm_##_callback;        \
  _callbacks->_callback.manager_data = (SmPointer) _client;   \
}G_STMT_END


static int           num_listeners;
static IceListenObj *listen_objs;


void
sm_init (XfceRc  *rc,
         gboolean disable_tcp)
{
  char *network_idlist;
  char  error[2048];

  if (disable_tcp || xfce_rc_read_bool_entry (rc, "DisableTcp", FALSE))
    {
#ifdef HAVE__ICETRANSNOLISTEN
      extern void _IceTransNoListen (char *protocol);
      _IceTransNoListen ("tcp");
#else
      if (G_UNLIKELY (verbose))
        {
          fprintf (stderr,
                   "xfce4-session: Requested to disable tcp connections, but "
                   "_IceTransNoListen is not available on this plattform. "
                   "Request will be ignored.\n");
        }
#endif
    }
  
  if (!SmsInitialize (PACKAGE, VERSION, sm_new_client, NULL, ice_auth_proc,
                      2048, error))
    {
      fprintf (stderr, "xfce4-session: Unable to register XSM protocol: %s\n", error);
      exit (EXIT_FAILURE);
    }

  if (!IceListenForConnections (&num_listeners, &listen_objs, 2048, error))
    {
      fprintf (stderr, "xfce4-session: Unable to establish ICE listeners: %s\n", error);
      exit (EXIT_FAILURE);
    }
  
  ice_setup_listeners (num_listeners, listen_objs);
  
  network_idlist = IceComposeNetworkIdList (num_listeners, listen_objs);
  xfce_setenv ("SESSION_MANAGER", network_idlist, TRUE);
  free (network_idlist);
}


static Status
sm_new_client (SmsConn        sms_conn,
               SmPointer      manager_data,
               unsigned long *mask,
               SmsCallbacks  *callbacks,
               char         **failure_reason)
{
  XfsmClient *client;
  gchar      *error = NULL;

  xfsm_verbose ("ICE connection fd = %d, received NEW CLIENT\n\n",
                IceConnectionNumber (SmsGetIceConnection (sms_conn)));
  
  client = xfsm_manager_new_client (sms_conn, &error);
  if (client == NULL)
    {
      xfsm_verbose ("NEW CLIENT failed: %s\n", error);

#ifdef HAVE_STRDUP
      *failure_reason = strdup (error);
#else
      *failure_reason = (char *) malloc (strlen (error) + 1);
      if (*failure_reason != NULL)
        strcpy (*failure_reason, error);
#endif

      return False;
    }
  
  SET_CALLBACK (callbacks, register_client, client);
  SET_CALLBACK (callbacks, interact_request, client);
  SET_CALLBACK (callbacks, interact_done, client);
  SET_CALLBACK (callbacks, save_yourself_request, client);
  SET_CALLBACK (callbacks, save_yourself_phase2_request, client);
  SET_CALLBACK (callbacks, save_yourself_done, client);
  SET_CALLBACK (callbacks, close_connection, client);
  SET_CALLBACK (callbacks, set_properties, client);
  SET_CALLBACK (callbacks, delete_properties, client);
  SET_CALLBACK (callbacks, get_properties, client);
  
  *mask = SmsRegisterClientProcMask | SmsInteractRequestProcMask
    | SmsInteractDoneProcMask | SmsSaveYourselfRequestProcMask 
    | SmsSaveYourselfP2RequestProcMask | SmsSaveYourselfDoneProcMask
    | SmsCloseConnectionProcMask | SmsSetPropertiesProcMask
    | SmsDeletePropertiesProcMask | SmsGetPropertiesProcMask;
                                                                                
  return True;
}


static Status
sm_register_client (SmsConn   sms_conn,
                    SmPointer client_data,
                    char     *previous_id)
{
  XfsmClient *client = (XfsmClient *) client_data;
  Status      result;
  
  xfsm_verbose ("ICE connection fd = %d, received REGISTER CLIENT [Previous Id = %s]\n\n",
                IceConnectionNumber (SmsGetIceConnection (sms_conn)),
                previous_id != NULL ? previous_id : "None");
  
  result = xfsm_manager_register_client (client, previous_id);

  if (previous_id != NULL)
    free (previous_id);
  
  return result;
}


static void
sm_interact_request (SmsConn   sms_conn,
                     SmPointer client_data,
                     int       dialog_type)
{
  XfsmClient *client = (XfsmClient *) client_data;

  xfsm_verbose ("Client Id = %s, received INTERACT REQUEST [Dialog type = %s]\n\n",
                client->id, dialog_type == SmDialogError ? "Error" : "Normal");
  
  xfsm_manager_interact (client, dialog_type);
}


static void
sm_interact_done (SmsConn   sms_conn,
                  SmPointer client_data,
                  Bool      cancel_shutdown)
{
  XfsmClient *client = (XfsmClient *) client_data;

  xfsm_verbose ("Client Id = %s, received INTERACT DONE [Cancel shutdown = %s]\n\n",
                client->id, cancel_shutdown ? "True" : "False");
  
  xfsm_manager_interact_done (client, cancel_shutdown);
}


static void
sm_save_yourself_request (SmsConn   sms_conn,
                          SmPointer client_data,
                          int       save_type,
                          Bool      shutdown,
                          int       interact_style,
                          Bool      fast,
                          Bool      global)
{
  XfsmClient *client = (XfsmClient *) client_data;

  if (G_UNLIKELY (verbose))
    {
      xfsm_verbose ("Client Id = %s, received SAVE YOURSELF REQUEST\n",
                    client->id);
      xfsm_verbose ("   Save type:      %s\n",
                    save_type == SmSaveLocal ? "Local"
                    : (save_type == SmSaveGlobal ? "Global" : "Both"));
      xfsm_verbose ("   Shutdown:       %s\n", shutdown ? "True" : "False");
      xfsm_verbose ("   Interact Style: %s\n", 
                    interact_style == SmInteractStyleNone ? "None"
                    : (interact_style == SmInteractStyleErrors ? "Errors" : "Any"));
      xfsm_verbose ("   Fast:           %s\n", fast ? "True" : "False");
      xfsm_verbose ("   Global:         %s\n", global ? "True" : "False");
      xfsm_verbose ("\n");
    }
  
  xfsm_manager_save_yourself (client, save_type, shutdown, interact_style, fast, global);
}


static void
sm_save_yourself_phase2_request (SmsConn   sms_conn,
                                 SmPointer client_data)
{
  XfsmClient *client = (XfsmClient *) client_data;

  xfsm_verbose ("Client Id = %s, received SAVE YOURSELF PHASE2 REQUEST\n\n", client->id);
  
  xfsm_manager_save_yourself_phase2 (client);
}


static void
sm_save_yourself_done (SmsConn   sms_conn,
                       SmPointer client_data,
                       Bool      success)
{
  XfsmClient *client = (XfsmClient *) client_data;

  xfsm_verbose ("Client Id = %s, received SAVE YOURSELF DONE [Success = %s]\n\n",
                client->id, success ? "True" : "False");
  
  xfsm_manager_save_yourself_done (client, success);
}


static void
sm_close_connection (SmsConn   sms_conn,
                     SmPointer client_data,
                     int       num_reasons,
                     char    **reasons)
{
  XfsmClient *client = (XfsmClient *) client_data;
  gint        n;

  if (G_UNLIKELY (verbose))
    {
      xfsm_verbose ("Client Id = %s, received CLOSE CONNECTION [Num reasons = %d]\n",
                    client->id, num_reasons);
      for (n = 0; n < num_reasons; ++n)
        xfsm_verbose ("   Reason %2d: %s\n", n, reasons[n]);
      xfsm_verbose ("\n");
    }
  
  xfsm_manager_close_connection (client, TRUE);

  if (num_reasons > 0)
    SmFreeReasons (num_reasons, reasons);
}


static void
sm_set_properties (SmsConn   sms_conn,
                   SmPointer client_data,
                   int       num_props,
                   SmProp  **props)
{
  XfsmClient     *client     = (XfsmClient *) client_data;
  XfsmProperties *properties = client->properties;
  int             n;
  
  if (G_UNLIKELY (verbose))
    {
      xfsm_verbose ("Client Id = %s, received SET PROPERTIES [Num props = %d]\n",
                    properties->client_id, num_props);
      for (n = 0; n < num_props; ++n)
        {
          xfsm_verbose ("   Name:  %s\n   Type:  %s\n", props[n]->name, props[n]->type);
          if (strcmp (props[n]->type, "ARRAY8") == 0)
            {
              xfsm_verbose ("   Value: %s\n", (const gchar *) props[n]->vals->value);
            }
          else if (strcmp (props[n]->type, "CARD8") == 0)
            {
              const guint8 *cardptr = (const guint8 *) props[n]->vals->value;
              xfsm_verbose ("   Value: %u\n", (unsigned) *cardptr);
            }
        }
      xfsm_verbose ("\n");
    }  
  
  xfsm_properties_merge (properties, num_props, props);
  
  while (num_props-- > 0)
    SmFreeProperty (props[num_props]);
  free (props);
}


static void
sm_delete_properties (SmsConn   sms_conn,
                      SmPointer client_data,
                      int       num_props,
                      char    **prop_names)
{
  XfsmClient     *client     = (XfsmClient *) client_data;
  XfsmProperties *properties = client->properties;
  int             n;

  if (G_UNLIKELY (verbose))
    {
      xfsm_verbose ("Client Id = %s, received DELETE PROPERTIES [Num props = %d]\n",
                    properties->client_id, num_props);
      for (n = 0; n < num_props; ++n)
        xfsm_verbose ("   Name:   %s\n", prop_names[n]);
      xfsm_verbose ("\n");
    }
  
  xfsm_properties_delete (properties, num_props, prop_names);
  
  while (num_props-- > 0)
    free (prop_names[num_props]);
  free (prop_names);
}


static void
sm_get_properties (SmsConn   sms_conn,
                   SmPointer client_data)
{
  XfsmClient     *client     = (XfsmClient *) client_data;
  XfsmProperties *properties = client->properties;
  SmProp        **props      = NULL;
  gint            num_props  = 0;

  xfsm_verbose ("Client Id = %s, received GET PROPERTIES\n\n", properties->client_id);
  
  xfsm_properties_extract (client->properties, &num_props, &props);

  SmsReturnProperties (sms_conn, num_props, props);
  
  if (num_props > 0)
    {
      while (num_props-- > 0)
        SmFreeProperty (props[num_props]);
      free (props);
    }
}
