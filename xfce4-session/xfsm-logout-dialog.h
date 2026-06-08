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

#ifndef __XFSM_LOGOUT_DIALOG_H__
#define __XFSM_LOGOUT_DIALOG_H__

#include <gtk/gtk.h>

#include "xfsm-shutdown.h"

#define XFSM_TYPE_LOGOUT_DIALOG (xfsm_logout_dialog_get_type ())
G_DECLARE_FINAL_TYPE (XfsmLogoutDialog, xfsm_logout_dialog, XFSM, LOGOUT_DIALOG, GtkDialog)

gboolean
xfsm_logout_dialog (const gchar *session_name,
                    XfsmShutdownType *return_type,
                    gboolean accessibility);

#endif
