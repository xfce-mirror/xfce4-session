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

#include "xfsm-shutdown.h"

typedef struct _XfsmLogoutDialogClass XfsmLogoutDialogClass;
typedef struct _XfsmLogoutDialog      XfsmLogoutDialog;

#define XFSM_TYPE_LOGOUT_DIALOG            (xfsm_logout_dialog_get_type ())
#define XFSM_LOGOUT_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFSM_TYPE_LOGOUT_DIALOG, XfsmLogoutDialog))
#define XFSM_LOGOUT_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFSM_TYPE_LOGOUT_DIALOG, XfsmLogoutDialogClass))
#define XFSM_IS_LOGOUT_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFSM_TYPE_LOGOUT_DIALOG))
#define XFSM_IS_LOGOUT_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFSM_TYPE_LOGOUT_DIALOG))
#define XFSM_LOGOUT_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFSM_TYPE_LOGOUT_DIALOG, XfsmLogoutDialogClass))

GType      xfsm_logout_dialog_get_type (void) G_GNUC_CONST;

gboolean   xfsm_logout_dialog          (const gchar      *session_name,
                                        XfsmShutdownType *return_type,
                                        gboolean          accessibility);

#endif
