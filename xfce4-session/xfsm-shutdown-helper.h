/* $Id$ */
/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2010      Ali Abdallah    <aliov@xfce.org>
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

#ifndef __XFSM_SHUTDOWN_HELPER_H
#define __XFSM_SHUTDOWN_HELPER_H

#include <glib-object.h>

#include "shutdown.h"

G_BEGIN_DECLS

#define XFSM_TYPE_SHUTDOWN_HELPER        (xfsm_shutdown_helper_get_type () )
#define XFSM_SHUTDOWN_HELPER(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), XFSM_TYPE_SHUTDOWN_HELPER, XfsmShutdownHelper))
#define XFSM_IS_SHUTDOWN_HELPER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), XFSM_TYPE_SHUTDOWN_HELPER))

typedef struct _XfsmShutdownHelperClass XfsmShutdownHelperClass;
typedef struct _XfsmShutdownHelper      XfsmShutdownHelper;

GType                     xfsm_shutdown_helper_get_type        (void) G_GNUC_CONST;

XfsmShutdownHelper       *xfsm_shutdown_helper_new             (void);

gboolean                  xfsm_shutdown_helper_send_password   (XfsmShutdownHelper *helper,
								const gchar *password);

gboolean                  xfsm_shutdown_helper_shutdown        (XfsmShutdownHelper *helper,
								GError **error);

gboolean                  xfsm_shutdown_helper_restart         (XfsmShutdownHelper *helper,
								GError **error);

gboolean                  xfsm_shutdown_helper_suspend         (XfsmShutdownHelper *helper,
								GError **error);

gboolean                  xfsm_shutdown_helper_hibernate       (XfsmShutdownHelper *helper,
								GError **error);

gboolean                  xfsm_shutdown_helper_send_command    (XfsmShutdownHelper *helper,
								XfsmShutdownType shutdown_type,
								GError **error);


G_END_DECLS

#endif /* __XFSM_SHUTDOWN_HELPER_H */
