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

#ifndef __XFSM_MANAGER_H__
#define __XFSM_MANAGER_H__

#include <libxfce4util/libxfce4util.h>

#include <xfce4-session/xfsm-client.h>


#define DIE_TIMEOUT     ( 7 * 1000)
#define SAVE_TIMEOUT    (20 * 1000)
#define STARTUP_TIMEOUT ( 8 * 1000)

typedef enum
{
  XFSM_MANAGER_STARTUP,
  XFSM_MANAGER_IDLE,
  XFSM_MANAGER_CHECKPOINT,
  XFSM_MANAGER_SHUTDOWN,
  XFSM_MANAGER_SHUTDOWNPHASE2,
} XfsmManagerState;


void xfsm_manager_init (XfceRc *rc);

gboolean xfsm_manager_restart (void);

gchar* xfsm_manager_generate_client_id (SmsConn sms_conn) G_GNUC_PURE;

XfsmClient* xfsm_manager_new_client           (SmsConn      sms_conn,
                                               gchar      **error);
gboolean        xfsm_manager_register_client      (XfsmClient  *client,
                                                   const gchar *previous_id);
void xfsm_manager_start_interact (XfsmClient *client);
void xfsm_manager_interact             (XfsmClient *client,
                                   gint        dialog_type);
void xfsm_manager_interact_done        (XfsmClient *client,
                                   gboolean    cancel_shutdown);
void xfsm_manager_save_yourself        (XfsmClient *client,
                                   gint        save_type,
                                   gboolean    shutdown,
                                   gint        interact_style,
                                   gboolean    fast,
                                   gboolean    global);
void xfsm_manager_save_yourself_phase2 (XfsmClient *client);
void xfsm_manager_save_yourself_done   (XfsmClient *client,
                                   gboolean    success);
void xfsm_manager_close_connection (XfsmClient *client,
                                    gboolean    cleanup);
void xfsm_manager_close_connection_by_ice_conn (IceConn ice_conn);

gboolean xfsm_manager_check_clients_saving (void);
gboolean xfsm_manager_maybe_enter_phase2 (void);
gboolean xfsm_manager_save_timeout (gpointer client_data);
void xfsm_manager_perform_shutdown (void);
gboolean xfsm_manager_run_command (const XfsmProperties *properties,
                                   const gchar          *command);
void xfsm_manager_store_session (void);
void xfsm_manager_complete_saveyourself (void);

#endif /* !__XFSM_MANAGER_H__ */

