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

#ifndef __XFSM_SHUTDOWN_H__
#define __XFSM_SHUTDOWN_H__

#include <glib.h>

/* */
#define	SHUTDOWN_LOGOUT	0
#define SHUTDOWN_REBOOT	1
#define SHUTDOWN_HALT	2

/* prototypes */
extern gboolean	shutdownDialog(gint *, gboolean *);
extern gint	shutdown(gint);

#endif	/* !__XFSM_SHUTDOWN_H__ */
