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

#ifndef __XFSM_ICE_LAYER_H__
#define __XFSM_ICE_LAYER_H__

#include <X11/ICE/ICElib.h>
#include <glib.h>

#include "xfsm-manager.h"

Bool     ice_auth_proc       (char         *hostname);
gboolean ice_setup_listeners (int           num_listeners,
                              IceListenObj *listen_objs,
                              XfsmManager  *manager);
void     ice_cleanup         (void);

#endif	/* !__XFSM_ICE_LAYER_H__ */
