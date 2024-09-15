/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#ifndef __XFAE_DIALOG_H__
#define __XFAE_DIALOG_H__

#include <gtk/gtk.h>

#include "xfae-model.h" /* Type XfsmRunHook */

G_BEGIN_DECLS;

typedef struct _XfaeDialogClass XfaeDialogClass;
typedef struct _XfaeDialog      XfaeDialog;

#define XFAE_TYPE_DIALOG            (xfae_dialog_get_type ())
#define XFAE_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFAE_TYPE_DIALOG, XfaeDialog))
#define XFAE_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFAE_TYPE_DIALOG, XfaeDialogClass))
#define XFAE_IS_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFAE_TYPE_DIALOG))
#define XFAE_IS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFAE_TYPE_DIALOG))
#define XFAE_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFAE_TYPE_DIALOG, XfaeDialogClass))

GType      xfae_dialog_get_type (void) G_GNUC_CONST;

GtkWidget *xfae_dialog_new      (const gchar *name,
                                 const gchar *descr,
                                 const gchar *command,
                                 XfsmRunHook  trigger);

void       xfae_dialog_get      (XfaeDialog   *dialog,
                                 gchar       **name,
                                 gchar       **descr,
                                 gchar       **command,
                                 XfsmRunHook  *trigger);

G_END_DECLS;

#endif /* !__XFAE_DIALOG_H__ */
