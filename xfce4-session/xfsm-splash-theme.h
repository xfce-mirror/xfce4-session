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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __XFSM_SPLASH_THEME_H__
#define __XFSM_SPLASH_THEME_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS;

typedef struct _XfsmSplashTheme XfsmSplashTheme;

XfsmSplashTheme *xfsm_splash_theme_load (const gchar *name);

XfsmSplashTheme *xfsm_splash_theme_copy (const XfsmSplashTheme *theme);

const gchar *xfsm_splash_theme_get_name (const XfsmSplashTheme *theme);

const gchar *xfsm_splash_theme_get_description (const XfsmSplashTheme *theme);

void xfsm_splash_theme_get_bgcolor (const XfsmSplashTheme *theme,
                                    GdkColor *color_return);

void xfsm_splash_theme_get_fgcolor (const XfsmSplashTheme *theme,
                                    GdkColor *color_return);

void xfsm_splash_theme_get_focolors (const XfsmSplashTheme *theme,
                                     GdkColor *color1_return,
                                     GdkColor *color2_return);

GdkPixbuf *xfsm_splash_theme_get_logo (const XfsmSplashTheme *theme,
                                       gint available_width,
                                       gint available_height);

GdkPixbuf *xfsm_splash_theme_get_skip_icon (const XfsmSplashTheme *theme,
                                            gint available_width,
                                            gint available_height);

GdkPixbuf *xfsm_splash_theme_get_chooser_icon (const XfsmSplashTheme *theme,
                                               gint available_width,
                                               gint available_height);

void xfsm_splash_theme_draw_gradient (const XfsmSplashTheme *theme,
                                      GdkDrawable *drawable,
                                      gint x,
                                      gint y,
                                      gint width,
                                      gint height);

GdkPixbuf *xfsm_splash_theme_generate_preview (const XfsmSplashTheme *theme,
                                               gint width,
                                               gint height);

gboolean xfsm_splash_theme_is_writable (const XfsmSplashTheme *theme);

void xfsm_splash_theme_destroy (XfsmSplashTheme *theme);

G_END_DECLS;

#endif /* !__XFSM_SPLASH_THEME_H__ */
