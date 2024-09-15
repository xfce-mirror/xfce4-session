/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2011      Nick Schermer <nick@xfce.org>
 * Copyright (c) 2014      Xfce Development Team <xfce4-dev@xfce.org>
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

#ifndef __XFSM_SHUTDOWN_FALLBACK_H__
#define __XFSM_SHUTDOWN_FALLBACK_H__

#include "xfsm-shutdown.h"

gboolean xfsm_shutdown_fallback_auth_shutdown     (void);
gboolean xfsm_shutdown_fallback_auth_restart      (void);
gboolean xfsm_shutdown_fallback_auth_suspend      (void);
gboolean xfsm_shutdown_fallback_auth_hibernate    (void);
gboolean xfsm_shutdown_fallback_auth_hybrid_sleep (void);

gboolean xfsm_shutdown_fallback_can_suspend       (void);
gboolean xfsm_shutdown_fallback_can_hibernate     (void);
gboolean xfsm_shutdown_fallback_can_hybrid_sleep  (void);

gboolean xfsm_shutdown_fallback_try_action     (XfsmShutdownType   type,
                                                GError           **error);

#endif	/* !__XFSM_SHUTDOWN_FALLBACK_H__ */
