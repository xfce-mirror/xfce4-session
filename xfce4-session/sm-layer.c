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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <X11/ICE/ICElib.h>
#include <X11/SM/SMlib.h>
#include <libxfce4util/libxfce4util.h>
#include <stdio.h>

#include "ice-layer.h"
#include "sm-layer.h"
#include "xfsm-global.h"

#define XFSM_CLIENT_MANAGER(c) (XFSM_MANAGER (g_object_get_data (G_OBJECT (c), "--xfsm-manager")))

#ifdef HAVE__ICETRANSNOLISTEN
extern void
_IceTransNoListen (char *protocol);
#endif

/* local prototypes */
static Status
sm_new_client (SmsConn sms_conn,
               SmPointer manager_data,
               unsigned long *mask,
               SmsCallbacks *callbacks,
               char **failure_reason);
static Status
sm_register_client (SmsConn sms_conn,
                    SmPointer client_data,
                    char *previous_id);
static void
sm_interact_request (SmsConn sms_conn,
                     SmPointer client_data,
                     int dialog_type);
static void
sm_interact_done (SmsConn sms_conn,
                  SmPointer client_data,
                  Bool cancel_shutdown);
static void
sm_save_yourself_request (SmsConn sms_conn,
                          SmPointer client_data,
                          int save_type,
                          Bool shutdown,
                          int interact_style,
                          Bool fast,
                          Bool global);
static void
sm_save_yourself_phase2_request (SmsConn sms_conn,
                                 SmPointer client_data);
static void
sm_save_yourself_done (SmsConn sms_conn,
                       SmPointer client_data,
                       Bool success);
static void
sm_close_connection (SmsConn sms_conn,
                     SmPointer client_data,
                     int num_reasons,
                     char **reasons);
static void
sm_set_properties (SmsConn sms_conn,
                   SmPointer client_data,
                   int num_props,
                   SmProp **props);
static void
sm_delete_properties (SmsConn sms_conn,
                      SmPointer client_data,
                      int num_props,
                      char **prop_names);
static void
sm_get_properties (SmsConn sms_conn,
                   SmPointer client_data);


#define SET_CALLBACK(_callbacks, _callback, _client) \
  G_STMT_START \
  { \
    _callbacks->_callback.callback = sm_##_callback; \
    _callbacks->_callback.manager_data = (SmPointer) _client; \
  } \
  G_STMT_END


static int num_listeners;
static IceListenObj *listen_objs;


void
sm_init (XfconfChannel *channel,
         gboolean disable_tcp,
         XfsmManager *manager)
{
  char *network_idlist;
  char error[2048];

  if (disable_tcp || !xfconf_channel_get_bool (channel, "/security/EnableTcp", FALSE))
    {
#ifdef HAVE__ICETRANSNOLISTEN
      _IceTransNoListen ("tcp");
#else
      fprintf (stderr,
               "xfce4-session: Requested to disable tcp connections, but "
               "_IceTransNoListen is not available on this plattform. "
               "Request will be ignored.\n");
      xfsm_verbose ("_IceTransNoListen unavailable on this platform");
#endif
    }

  if (!SmsInitialize (PACKAGE, VERSION, sm_new_client, manager, ice_auth_proc,
                      2048, error))
    {
      fprintf (stderr, "xfce4-session: Unable to register XSM protocol: %s\n", error);
      /* log to verbose so we don't have to look at both files */
      xfsm_verbose ("xfce4-session: Unable to register XSM protocol: %s\n", error);
      exit (EXIT_FAILURE);
    }

  if (!IceListenForConnections (&num_listeners, &listen_objs, 2048, error))
    {
      fprintf (stderr, "xfce4-session: Unable to establish ICE listeners: %s\n", error);
      /* log to verbose so we don't have to look at both files */
      xfsm_verbose ("xfce4-session: Unable to establish ICE listeners: %s\n", error);
      exit (EXIT_FAILURE);
    }

  ice_setup_listeners (num_listeners, listen_objs, manager);

  network_idlist = IceComposeNetworkIdList (num_listeners, listen_objs);
  g_setenv ("SESSION_MANAGER", network_idlist, TRUE);
  free (network_idlist);
}


static Status
sm_new_client (SmsConn sms_conn,
               SmPointer manager_data,
               unsigned long *mask,
               SmsCallbacks *callbacks,
               char **failure_reason)
{
  XfsmManager *manager = XFSM_MANAGER (manager_data);
  XfsmClient *client;
  gchar *error = NULL;

  xfsm_verbose ("ICE connection fd = %d, received NEW CLIENT\n\n",
                IceConnectionNumber (SmsGetIceConnection (sms_conn)));

  client = xfsm_manager_new_client (manager, sms_conn, &error);
  if (client == NULL)
    {
      xfsm_verbose ("NEW CLIENT failed: %s\n", error);

      *failure_reason = g_strdup (error);

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

  g_object_set_data (G_OBJECT (client), "--xfsm-manager", manager);

  return True;
}


static Status
sm_register_client (SmsConn sms_conn,
                    SmPointer client_data,
                    char *previous_id)
{
  XfsmClient *client = (XfsmClient *) client_data;
  Status result;

  xfsm_verbose ("ICE connection fd = %d, received REGISTER CLIENT [Previous Id = %s]\n\n",
                IceConnectionNumber (SmsGetIceConnection (sms_conn)),
                previous_id != NULL ? previous_id : "None");

  result = xfsm_manager_register_client (XFSM_CLIENT_MANAGER (client), client, NULL, previous_id);

  if (previous_id != NULL)
    free (previous_id);

  return result;
}


static void
sm_interact_request (SmsConn sms_conn,
                     SmPointer client_data,
                     int dialog_type)
{
  XfsmClient *client = (XfsmClient *) client_data;

  xfsm_verbose ("Client Id = %s, received INTERACT REQUEST [Dialog type = %s]\n\n",
                xfsm_client_get_id (client), dialog_type == SmDialogError ? "Error" : "Normal");

  xfsm_manager_interact (XFSM_CLIENT_MANAGER (client), client, dialog_type);
}


static void
sm_interact_done (SmsConn sms_conn,
                  SmPointer client_data,
                  Bool cancel_shutdown)
{
  XfsmClient *client = (XfsmClient *) client_data;

  xfsm_verbose ("Client Id = %s, received INTERACT DONE [Cancel shutdown = %s]\n\n",
                xfsm_client_get_id (client), cancel_shutdown ? "True" : "False");

  xfsm_manager_interact_done (XFSM_CLIENT_MANAGER (client), client, cancel_shutdown);
}


static void
sm_save_yourself_request (SmsConn sms_conn,
                          SmPointer client_data,
                          int save_type,
                          Bool shutdown,
                          int interact_style,
                          Bool fast,
                          Bool global)
{
  XfsmClient *client = (XfsmClient *) client_data;

  if (G_UNLIKELY (verbose))
    {
      xfsm_verbose ("Client Id = %s, received SAVE YOURSELF REQUEST\n",
                    xfsm_client_get_id (client));
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

  xfsm_manager_save_yourself (XFSM_CLIENT_MANAGER (client), client, save_type, shutdown, interact_style, fast, global);
}


static void
sm_save_yourself_phase2_request (SmsConn sms_conn,
                                 SmPointer client_data)
{
  XfsmClient *client = (XfsmClient *) client_data;

  xfsm_verbose ("Client Id = %s, received SAVE YOURSELF PHASE2 REQUEST\n\n",
                xfsm_client_get_id (client));

  xfsm_manager_save_yourself_phase2 (XFSM_CLIENT_MANAGER (client), client);
}


static void
sm_save_yourself_done (SmsConn sms_conn,
                       SmPointer client_data,
                       Bool success)
{
  XfsmClient *client = (XfsmClient *) client_data;

  xfsm_verbose ("Client Id = %s, received SAVE YOURSELF DONE [Success = %s]\n\n",
                xfsm_client_get_id (client), success ? "True" : "False");

  xfsm_manager_save_yourself_done (XFSM_CLIENT_MANAGER (client), client, success);
}


static void
sm_close_connection (SmsConn sms_conn,
                     SmPointer client_data,
                     int num_reasons,
                     char **reasons)
{
  XfsmClient *client = (XfsmClient *) client_data;
  gint n;

  if (G_UNLIKELY (verbose))
    {
      xfsm_verbose ("Client Id = %s, received CLOSE CONNECTION [Num reasons = %d]\n",
                    xfsm_client_get_id (client), num_reasons);
      for (n = 0; n < num_reasons; ++n)
        xfsm_verbose ("   Reason %2d: %s\n", n, reasons[n]);
      xfsm_verbose ("\n");
    }

  xfsm_manager_close_connection (XFSM_CLIENT_MANAGER (client), client, TRUE);

  if (num_reasons > 0)
    SmFreeReasons (num_reasons, reasons);
}


static void
sm_set_properties (SmsConn sms_conn,
                   SmPointer client_data,
                   int num_props,
                   SmProp **props)
{
  XfsmClient *client = (XfsmClient *) client_data;
  int n;
  int i;

  if (G_UNLIKELY (verbose))
    {
      xfsm_verbose ("Client Id = %s, received SET PROPERTIES [Num props = %d]\n",
                    xfsm_client_get_id (client), num_props);
      for (n = 0; n < num_props; ++n)
        {
          xfsm_verbose ("   Name:  %s\n", props[n]->name);
          xfsm_verbose ("   Type:  %s\n", props[n]->type);
          if (strcmp (props[n]->type, "ARRAY8") == 0)
            {
              xfsm_verbose ("   Value: %s\n", (const gchar *) props[n]->vals->value);
            }
          else if (strcmp (props[n]->type, "CARD8") == 0)
            {
              const guint8 *cardptr = (const guint8 *) props[n]->vals->value;
              xfsm_verbose ("   Value: %u\n", (unsigned) *cardptr);
            }
          else if (strcmp (props[n]->type, "LISTofARRAY8") == 0)
            {
              xfsm_verbose ("   Value:\n");
              for (i = 0; i < props[n]->num_vals; ++i)
                {
                  xfsm_verbose ("          %s%s\n", (const gchar *) props[n]->vals[i].value,
                                (i == props[n]->num_vals - 1) ? "" : ",");
                }
            }
          xfsm_verbose ("\n");
        }
      xfsm_verbose ("\n");
    }

  xfsm_client_merge_properties (client, props, num_props);

  while (num_props-- > 0)
    SmFreeProperty (props[num_props]);
  g_free (props);
}


static void
sm_delete_properties (SmsConn sms_conn,
                      SmPointer client_data,
                      int num_props,
                      char **prop_names)
{
  XfsmClient *client = (XfsmClient *) client_data;
  int n;

  if (G_UNLIKELY (verbose))
    {
      xfsm_verbose ("Client Id = %s, received DELETE PROPERTIES [Num props = %d]\n",
                    xfsm_client_get_id (client), num_props);
      for (n = 0; n < num_props; ++n)
        xfsm_verbose ("   Name:   %s\n", prop_names[n]);
      xfsm_verbose ("\n");
    }

  xfsm_client_delete_properties (client, prop_names, num_props);

  while (num_props-- > 0)
    free (prop_names[num_props]);
  free (prop_names);
}


static void
sm_get_properties (SmsConn sms_conn,
                   SmPointer client_data)
{
  XfsmClient *client = (XfsmClient *) client_data;
  XfsmProperties *properties = xfsm_client_get_properties (client);
  SmProp **props = NULL;
  gint num_props = 0;

  xfsm_verbose ("Client Id = %s, received GET PROPERTIES\n\n", properties->client_id);

  xfsm_properties_extract (properties, &num_props, &props);

  SmsReturnProperties (sms_conn, num_props, props);

  if (num_props > 0)
    {
      while (num_props-- > 0)
        SmFreeProperty (props[num_props]);
      g_free (props);
    }
}
