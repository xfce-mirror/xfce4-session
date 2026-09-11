/*
 * Copyright (c) 2009 Brian Tarricone <brian@terricone.org>
 * Copyright (C) 1999 Olivier Fourdan <fourdan@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#if !defined(_LIBXFCE4SESSION_CLIENT_INSIDE_LIBXFCE4SESSION_CLIENT_H) && !defined(LIBXFCE4SESSION_CLIENT_COMPILATION)
#error "Only <libxfce4session-client/libxfce4session-client.h> can be included directly, this file is not part of the public API."
#endif

#ifndef __XFCE_SESSION_CLIENT_H__
#define __XFCE_SESSION_CLIENT_H__

#include <gtk/gtk.h>
#include <libxfce4session-client/libxfce4session-client-enums.h>

/**
 * XFCE_SESSION_CLIENT_ERROR:
 *
 * Error domain for XfceSessionClient.
 *
 * Errors in this domain will be from the #XfceSessionClientErrorEnum enumeration.
 *
 * See #GError for more information on error domains.
 **/
#define XFCE_SESSION_CLIENT_ERROR xfce_session_client_error_quark ()

G_BEGIN_DECLS

/**
 * XfceSessionClient:
 *
 * An opaque struct with only private fields.
 **/
#define XFCE_TYPE_SESSION_CLIENT (xfce_session_client_get_type ())
G_DECLARE_DERIVABLE_TYPE (XfceSessionClient, xfce_session_client, XFCE, SESSION_CLIENT, GObject);

GQuark
xfce_session_client_error_quark (void);

GOptionGroup *
xfce_session_client_get_option_group (gint argc,
                                      gchar **argv);

XfceSessionClient *
xfce_session_client_new (void);

XfceSessionClient *
xfce_session_client_new_with_argv (gint argc,
                                   gchar **argv,
                                   XfceSessionClientRestartStyle restart_style,
                                   guchar priority);

XfceSessionClient *
xfce_session_client_new_full (XfceSessionClientRestartStyle restart_style,
                              guchar priority,
                              const gchar *resumed_client_id,
                              const gchar *current_directory,
                              const gchar **restart_command,
                              const gchar *desktop_file);

XfceSessionClient *
xfce_session_client_get (void);

gboolean
xfce_session_client_connect (XfceSessionClient *session_client,
                             GError **error);
void
xfce_session_client_disconnect (XfceSessionClient *session_client);

void
xfce_session_client_request_shutdown (XfceSessionClient *session_client,
                                      XfceSessionClientShutdownHint shutdown_hint);

gboolean
xfce_session_client_is_connected (XfceSessionClient *session_client);
gboolean
xfce_session_client_is_resumed (XfceSessionClient *session_client);

void
xfce_session_client_set_desktop_file (XfceSessionClient *session_client,
                                      const gchar *desktop_file);

const gchar *
xfce_session_client_get_client_id (XfceSessionClient *session_client);

const gchar *
xfce_session_client_get_state_file (XfceSessionClient *session_client);

void
xfce_session_client_set_restart_style (XfceSessionClient *session_client,
                                       XfceSessionClientRestartStyle restart_style);
XfceSessionClientRestartStyle
xfce_session_client_get_restart_style (XfceSessionClient *session_client);

void
xfce_session_client_set_priority (XfceSessionClient *session_client,
                                  guint8 priority);
guint8
xfce_session_client_get_priority (XfceSessionClient *session_client);

void
xfce_session_client_set_current_directory (XfceSessionClient *session_client,
                                           const gchar *current_directory);
const gchar *
xfce_session_client_get_current_directory (XfceSessionClient *session_client);

void
xfce_session_client_set_restart_command (XfceSessionClient *session_client,
                                         const gchar *const *restart_command);
const gchar *const *
xfce_session_client_get_restart_command (XfceSessionClient *session_client);

void
xfce_session_client_set_clone_command (XfceSessionClient *session_client,
                                       const gchar *const *clone_command);
const gchar *const *
xfce_session_client_get_clone_command (XfceSessionClient *session_client);

void
xfce_session_client_add_window (XfceSessionClient *session_client,
                                GtkWindow *window,
                                const gchar *name);
void
xfce_session_client_restore_window (XfceSessionClient *session_client,
                                    GtkWindow *window,
                                    const gchar *name);
void
xfce_session_client_rename_window (XfceSessionClient *session_client,
                                   GtkWindow *window,
                                   const gchar *new_name);
void
xfce_session_client_remove_window (XfceSessionClient *session_client,
                                   const gchar *name);

G_END_DECLS

#endif /* __XFCE_SESSION_CLIENT_H__ */
