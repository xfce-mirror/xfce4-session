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

#ifndef __BALOU_THEME_H__
#define __BALOU_THEME_H__

#include <gdk/gdk.h>


G_BEGIN_DECLS;

typedef struct _BalouTheme BalouTheme;


BalouTheme  *balou_theme_load             (const gchar *name);
const gchar *balou_theme_get_name         (const BalouTheme *theme);
const gchar *balou_theme_get_description  (const BalouTheme *theme);
const gchar *balou_theme_get_font         (const BalouTheme *theme);
void         balou_theme_get_bgcolor      (const BalouTheme *theme,
                                           GdkColor         *color_return);
void         balou_theme_get_fgcolor      (const BalouTheme *theme,
                                           GdkColor         *color_return);
GdkPixbuf   *balou_theme_get_logo         (const BalouTheme *theme,
                                           gint              available_width,
                                           gint              available_height);
void         balou_theme_draw_gradient    (const BalouTheme *theme,
                                           GdkDrawable      *drawable,
                                           GdkGC            *gc,
                                           GdkRectangle      logobox,
                                           GdkRectangle      textbox);
GdkPixbuf   *balou_theme_generate_preview (const BalouTheme *theme,
                                           gint              width,
                                           gint              height);
void         balou_theme_destroy          (BalouTheme       *theme);

G_END_DECLS;


#endif /* !__BALOU_THEME_H__ */
