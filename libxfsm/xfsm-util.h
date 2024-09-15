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

#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#define WINDOWING_IS_X11() GDK_IS_X11_DISPLAY (gdk_display_get_default ())
#else
#define WINDOWING_IS_X11() FALSE
#endif
#ifdef ENABLE_WAYLAND
#include <gdk/gdkwayland.h>
#define WINDOWING_IS_WAYLAND() GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())
#else
#define WINDOWING_IS_WAYLAND() FALSE
#endif

#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

G_BEGIN_DECLS;

#define DEFAULT_SESSION_NAME "Default"
#define SETTINGS_CHANNEL "xfce4-session"
#define SESSION_FILE_DELIMITER ","

typedef struct _XfsmSessionInfo XfsmSessionInfo;

struct _XfsmSessionInfo
{
  gchar *name; /* name of the session */
  time_t atime; /* last access time */
  cairo_surface_t *preview; /* preview icon (52x42) */
};

enum
{
  PREVIEW_COLUMN,
  NAME_COLUMN,
  TITLE_COLUMN,
  ACCESSED_COLUMN,
  ATIME_COLUMN,
  N_COLUMNS,
};


gboolean
xfsm_start_application (gchar **command,
                        gchar **environment,
                        GdkScreen *screen,
                        const gchar *current_directory,
                        const gchar *client_machine,
                        const gchar *user_id);

void
xfsm_place_trash_window (GtkWindow *window,
                         GdkScreen *screen,
                         gint monitor);

/* XXX - move to libxfce4util? */
gboolean
xfsm_strv_equal (gchar **a,
                 gchar **b);

gchar *
xfsm_gdk_display_get_fullname (GdkDisplay *display);

cairo_surface_t *
xfsm_load_session_preview (const gchar *name,
                           gint size,
                           gint scale_factor);

const gchar *
settings_list_sessions_get_filename (void);

GKeyFile *
settings_list_sessions_open_key_file (gboolean readonly);

GList *
settings_list_sessions (GKeyFile *file,
                        gint scale_factor);

void
settings_list_sessions_treeview_init (GtkTreeView *treeview);

void
settings_list_sessions_populate (GtkTreeModel *model,
                                 GList *sessions);

void
settings_list_sessions_delete_session (GtkButton *button,
                                       GtkTreeView *treeview);

G_END_DECLS;

#endif /* !__XFSM_UTIL_H__ */
