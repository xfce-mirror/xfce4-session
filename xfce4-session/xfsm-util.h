/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __XFSM_UTIL_H__
#define __XFSM_UTIL_H__

#include <gtk/gtk.h>

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

G_END_DECLS;

#endif /* !__XFSM_UTIL_H__ */
