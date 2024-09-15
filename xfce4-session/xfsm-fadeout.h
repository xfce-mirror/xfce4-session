/* $Id$ */
/*-
 * Copyright (c) 2004 Benedikt Meurer <benny@xfce.org>
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

#ifndef __XFSM_FADEOUT_H__
#define __XFSM_FADEOUT_H__

#include <gdk/gdk.h>


G_BEGIN_DECLS;

typedef struct _XfsmFadeout XfsmFadeout;

XfsmFadeout *
xfsm_fadeout_new (GdkDisplay *display);
void
xfsm_fadeout_destroy (XfsmFadeout *fadeout);

G_END_DECLS;


#endif /* !__XFSM_FADEOUT_H__ */
