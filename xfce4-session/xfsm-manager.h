/* $Id$ */
/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
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

#include <glib-object.h>

G_BEGIN_DECLS

#include <libxfce4util/libxfce4util.h>

#include <xfce4-session/xfsm-client.h>

#define XFSM_TYPE_MANAGER     (xfsm_manager_get_type())
#define XFSM_MANAGER(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), XFSM_TYPE_MANAGER, XfsmManager))
#define XFSM_IS_MANAGER(obj)  (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFSM_TYPE_MANAGER))

#define DIE_TIMEOUT     ( 7 * 1000)
#define SAVE_TIMEOUT    (60 * 1000)
#define STARTUP_TIMEOUT ( 8 * 1000)

typedef enum
{
  XFSM_MANAGER_STARTUP,
  XFSM_MANAGER_IDLE,
  XFSM_MANAGER_CHECKPOINT,
  XFSM_MANAGER_SHUTDOWN,
  XFSM_MANAGER_SHUTDOWNPHASE2,
} XfsmManagerState;

typedef enum
{
    XFSM_MANAGER_QUEUE_PENDING_PROPS = 0,
    XFSM_MANAGER_QUEUE_STARTING_PROPS,
    XFSM_MANAGER_QUEUE_RESTART_PROPS,
    XFSM_MANAGER_QUEUE_RUNNING_CLIENTS,
    XFSM_MANAGER_QUEUE_FAILSAFE_CLIENTS,
} XfsmManagerQueueType;

typedef enum
{
    XFSM_MANAGER_COMPAT_GNOME = 0,
    XFSM_MANAGER_COMPAT_KDE,
} XfsmManagerCompatType;

typedef struct _XfsmManager  XfsmManager;

GType xfsm_manager_get_type (void) G_GNUC_CONST;

XfsmManager *xfsm_manager_new (void);

void xfsm_manager_load (XfsmManager *manager,
                        XfceRc      *rc);

gboolean xfsm_manager_restart (XfsmManager *manager);

/* call when startup is finished */
void xfsm_manager_signal_startup_done (XfsmManager *manager);

/* call for each client that fails */
void xfsm_manager_handle_failed_client (XfsmManager    *manager,
                                        XfsmProperties *properties);

gchar* xfsm_manager_generate_client_id (SmsConn sms_conn) G_GNUC_PURE;

XfsmClient* xfsm_manager_new_client (XfsmManager *manager,
                                     SmsConn      sms_conn,
                                     gchar      **error);

gboolean xfsm_manager_register_client (XfsmManager *manager,
                                       XfsmClient  *client,
                                       const gchar *previous_id);

void xfsm_manager_start_interact (XfsmManager *manager,
                                  XfsmClient  *client);

void xfsm_manager_interact (XfsmManager *manager,
                            XfsmClient  *client,
                            gint         dialog_type);

void xfsm_manager_interact_done (XfsmManager *manager,
                                 XfsmClient  *client,
                                 gboolean     cancel_shutdown);

void xfsm_manager_save_yourself (XfsmManager *manager,
                                 XfsmClient  *client,
                                 gint         save_type,
                                 gboolean     shutdown,
                                 gint         interact_style,
                                 gboolean     fast,
                                 gboolean     global);

void xfsm_manager_save_yourself_phase2 (XfsmManager *manager,
                                        XfsmClient  *client);

void xfsm_manager_save_yourself_done (XfsmManager *manager,
                                      XfsmClient  *client,
                                      gboolean     success);

void xfsm_manager_close_connection (XfsmManager *manager,
                                    XfsmClient  *client,
                                    gboolean     cleanup);

void xfsm_manager_close_connection_by_ice_conn (XfsmManager *manager,
                                                IceConn ice_conn);

gboolean xfsm_manager_check_clients_saving (XfsmManager *manager);

gboolean xfsm_manager_maybe_enter_phase2 (XfsmManager *manager);

void xfsm_manager_perform_shutdown (XfsmManager *manager);

gboolean xfsm_manager_run_command (XfsmManager          *manager,
                                   const XfsmProperties *properties,
                                   const gchar          *command);

void xfsm_manager_store_session (XfsmManager *manager);

void xfsm_manager_complete_saveyourself (XfsmManager *manager);

gint xfsm_manager_get_shutdown_type (XfsmManager *manager);

GQueue *xfsm_manager_get_queue (XfsmManager         *manager,
                                XfsmManagerQueueType q_type);

gboolean xfsm_manager_get_use_failsafe_mode (XfsmManager *manager);

gboolean xfsm_manager_get_compat_startup (XfsmManager          *manager,
                                          XfsmManagerCompatType type);

#endif /* !__XFSM_MANAGER_H__ */
