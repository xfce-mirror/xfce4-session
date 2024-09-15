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

#ifndef __XFSM_LEGACY_H__
#define __XFSM_LEGACY_H__

#include <gdk/gdk.h>
#include <libxfce4util/libxfce4util.h>


void xfsm_legacy_perform_session_save (void);
void xfsm_legacy_store_session (GKeyFile *file,
                                const gchar *group);
void xfsm_legacy_load_session (GKeyFile *file,
                               const gchar *group);
void xfsm_legacy_init (void);
void xfsm_legacy_startup (void);
void xfsm_legacy_shutdown (void);

#endif /* !__XFSM_LEGACY_H__ */
