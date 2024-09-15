/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
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

#ifndef __XFAE_WINDOW_H__
#define __XFAE_WINDOW_H__

#include "xfae-model.h"

G_BEGIN_DECLS;

typedef struct _XfaeWindowClass XfaeWindowClass;
typedef struct _XfaeWindow XfaeWindow;

#define XFAE_TYPE_WINDOW (xfae_window_get_type ())
#define XFAE_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFAE_TYPE_WINDOW, XfaeWindow))
#define XFAE_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), XFAE_TYPE_WINDOW, XfaeWindow))
#define XFAE_IS_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFAE_TYPE_WINDOW))
#define XFAE_IS_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFAE_TYPE_WINDOW))
#define XFAE_WINDOW_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), XFAE_TYPE_WINDOW, XfaeWindowClass))

GType
xfae_window_get_type (void) G_GNUC_CONST;

GtkWidget *
xfae_window_new (void) G_GNUC_MALLOC;

G_END_DECLS;

#endif /* !__XFAE_WINDOW_H__ */
