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

#ifndef __XFSM_UTIL_H__
#define __XFSM_UTIL_H__

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>

G_BEGIN_DECLS;

GtkWidget	*xfsm_imgbtn_new(const gchar *, const gchar *, GtkWidget **);

gchar* xfsm_display_fullname (GdkDisplay *display) G_GNUC_CONST;
gchar* xfsm_screen_fullname (GdkScreen *screen) G_GNUC_CONST;

gchar* xfsm_expand_variables (const gchar *command,
                              gchar      **envp);

gboolean xfsm_start_application (gchar      **command,
                                 gchar      **environment,
                                 GdkScreen   *screen,
                                 const gchar *current_directory,
                                 const gchar *client_machine,
                                 const gchar *user_id);

GdkScreen *xfsm_locate_screen_with_pointer (gint *monitor_ret);

void xfsm_center_window_on_screen (GtkWindow *window,
                                   GdkScreen *screen,
                                   gint       monitor);

gchar **xfsm_strv_copy (gchar **v);

gboolean xfsm_strv_equal (gchar **a, gchar **b);

void xfsm_window_add_border (GtkWindow *window);

XfceRc *xfsm_open_config (gboolean readonly);

G_END_DECLS;

#endif /* !__XFSM_UTIL_H__ */
