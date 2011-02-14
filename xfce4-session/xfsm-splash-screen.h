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

#ifndef __XFSM_SPLASH_SCREEN_H__
#define __XFSM_SPLASH_SCREEN_H__

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>


G_BEGIN_DECLS;

typedef struct _XfsmSplashScreen XfsmSplashScreen;

XfsmSplashScreen *xfsm_splash_screen_new    (GdkDisplay       *display,
                                             const gchar      *engine);

void              xfsm_splash_screen_start  (XfsmSplashScreen *splash,
                                             const gchar      *name,
                                             GdkPixbuf        *preview,
                                             unsigned          steps);

void              xfsm_splash_screen_next   (XfsmSplashScreen *splash,
                                             const gchar      *text);

int               xfsm_splash_screen_run    (XfsmSplashScreen *splash,
                                             GtkWidget        *dialog);

int               xfsm_splash_screen_choose (XfsmSplashScreen *splash,
                                             GList            *sessions,
                                             const gchar      *default_session,
                                             gchar           **name_return);

void              xfsm_splash_screen_free (XfsmSplashScreen *splash);

G_END_DECLS;


#endif /* !__XFSM_SPLASH_SCREEN_H__ */
