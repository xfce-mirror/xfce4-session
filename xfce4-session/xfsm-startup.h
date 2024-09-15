/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 */

#ifndef __XFSM_STARTUP_H__
#define __XFSM_STARTUP_H__

#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

void xfsm_startup_init (XfconfChannel *channel);
void xfsm_startup_shutdown (void);
void xfsm_startup_foreign (XfsmManager *manager);
void xfsm_startup_begin (XfsmManager *manager);
void xfsm_startup_session_continue (XfsmManager *manager);
gboolean xfsm_startup_start_properties (XfsmProperties *properties,
                                        XfsmManager    *manager);

#endif /* !__XFSM_STARTUP_H__ */
