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

#ifndef __XFSM_CHOOSER_H__
#define __XFSM_CHOOSER_H__

#include <gtk/gtk.h>

#define XFSM_RESPONSE_LOAD 1
#define XFSM_RESPONSE_NEW 2

G_BEGIN_DECLS

#define XFSM_TYPE_CHOOSER xfsm_chooser_get_type ()
G_DECLARE_FINAL_TYPE (XfsmChooser, xfsm_chooser, XFSM, CHOOSER, GtkDialog)

struct _XfsmChooser
{
  GtkDialog dialog;

  GtkWidget *tree;
};

void
xfsm_chooser_set_sessions (XfsmChooser *chooser,
                           GList *sessions,
                           const gchar *default_session);

gchar *
xfsm_chooser_get_session (const XfsmChooser *chooser);

G_END_DECLS


#endif /* !__XFSM_CHOOSER_H__ */
