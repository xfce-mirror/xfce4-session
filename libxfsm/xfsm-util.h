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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 */

#ifndef __XFSM_UTIL_H__
#define __XFSM_UTIL_H__

#include <xfconf/xfconf.h>

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

G_BEGIN_DECLS;

gboolean xfsm_start_application (gchar      **command,
                                 gchar      **environment,
                                 GdkScreen   *screen,
                                 const gchar *current_directory,
                                 const gchar *client_machine,
                                 const gchar *user_id);

void xfsm_place_trash_window (GtkWindow *window,
                              GdkScreen *screen,
                              gint       monitor);

/* XXX - move to libxfce4util? */
gboolean xfsm_strv_equal (gchar **a, gchar **b);

void xfsm_window_add_border (GtkWindow *window);

XfconfChannel *xfsm_open_config (void);

gchar *xfsm_gdk_display_get_fullname (GdkDisplay *display);


G_END_DECLS;

#endif /* !__XFSM_UTIL_H__ */
