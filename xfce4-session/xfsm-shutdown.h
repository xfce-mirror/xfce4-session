/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2011      Nick Schermer <nick@xfce.org>
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

#ifndef __XFSM_SHUTDOWN_H__
#define __XFSM_SHUTDOWN_H__

#define LOGIND_RUNNING() (access ("/run/systemd/seats/", F_OK) >= 0)

typedef struct _XfsmShutdownClass XfsmShutdownClass;
typedef struct _XfsmShutdown XfsmShutdown;

#define XFSM_TYPE_SHUTDOWN (xfsm_shutdown_get_type ())
#define XFSM_SHUTDOWN(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFSM_TYPE_SHUTDOWN, XfsmShutdown))
#define XFSM_SHUTDOWN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), XFSM_TYPE_SHUTDOWN, XfsmShutdownClass))
#define XFSM_IS_SHUTDOWN(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFSM_TYPE_SHUTDOWN))
#define XFSM_IS_SHUTDOWN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFSM_TYPE_SHUTDOWN))
#define XFSM_SHUTDOWN_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), XFSM_TYPE_SHUTDOWN, XfsmShutdownClass))

typedef enum
{
  XFSM_SHUTDOWN_ASK = 0,
  XFSM_SHUTDOWN_LOGOUT,
  XFSM_SHUTDOWN_SHUTDOWN,
  XFSM_SHUTDOWN_RESTART,
  XFSM_SHUTDOWN_SUSPEND,
  XFSM_SHUTDOWN_HIBERNATE,
  XFSM_SHUTDOWN_HYBRID_SLEEP,
  XFSM_SHUTDOWN_SWITCH_USER,
} XfsmShutdownType;

typedef enum
{
  PASSWORD_RETRY,
  PASSWORD_SUCCEED,
  PASSWORD_FAILED
} XfsmPassState;

GType
xfsm_shutdown_get_type (void) G_GNUC_CONST;

XfsmShutdown *
xfsm_shutdown_get (void);

gboolean
xfsm_shutdown_password_require (XfsmShutdown *shutdown,
                                XfsmShutdownType type);

XfsmPassState
xfsm_shutdown_password_send (XfsmShutdown *shutdown,
                             XfsmShutdownType type,
                             const gchar *password);

gboolean
xfsm_shutdown_try_type (XfsmShutdown *shutdown,
                        XfsmShutdownType type,
                        GError **error);

gboolean
xfsm_shutdown_try_restart (XfsmShutdown *shutdown,
                           GError **error);

gboolean
xfsm_shutdown_try_shutdown (XfsmShutdown *shutdown,
                            GError **error);

gboolean
xfsm_shutdown_try_suspend (XfsmShutdown *shutdown,
                           GError **error);

gboolean
xfsm_shutdown_try_hibernate (XfsmShutdown *shutdown,
                             GError **error);

gboolean
xfsm_shutdown_try_hybrid_sleep (XfsmShutdown *shutdown,
                                GError **error);

gboolean
xfsm_shutdown_try_switch_user (XfsmShutdown *shutdown,
                               GError **error);

void
xfsm_shutdown_can_restart (XfsmShutdown *shutdown,
                           gboolean *can_restart,
                           gboolean *auth_restart);

void
xfsm_shutdown_can_shutdown (XfsmShutdown *shutdown,
                            gboolean *can_shutdown,
                            gboolean *auth_shutdown);

void
xfsm_shutdown_can_suspend (XfsmShutdown *shutdown,
                           gboolean *can_suspend,
                           gboolean *auth_suspend);

void
xfsm_shutdown_can_hibernate (XfsmShutdown *shutdown,
                             gboolean *can_hibernate,
                             gboolean *auth_hibernate);

void
xfsm_shutdown_can_hybrid_sleep (XfsmShutdown *shutdown,
                                gboolean *can_hybrid_sleep,
                                gboolean *auth_hybrid_sleep);

gboolean
xfsm_shutdown_can_switch_user (XfsmShutdown *shutdown,
                               gboolean *can_switch_user,
                               GError **error);

gboolean
xfsm_shutdown_can_save_session (XfsmShutdown *shutdown);

gboolean
xfsm_shutdown_can_logout (XfsmShutdown *shutdown);

gboolean
xfsm_shutdown_has_update_prepared (XfsmShutdown *shutdown);

#endif /* !__XFSM_SHUTDOWN_H__ */
