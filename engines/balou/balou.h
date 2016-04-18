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

#ifndef __BALOU_H__
#define __BALOU_H__

#include <gtk/gtk.h>

#include <engines/balou/balou-theme.h>


G_BEGIN_DECLS;

#define BALOU(obj)  ((Balou *)(obj))

typedef struct _BalouWindow BalouWindow;
typedef struct _Balou       Balou;


struct _Balou
{
  GdkRGBA       bgcolor;
  GdkRGBA       fgcolor;

  BalouTheme    *theme;

  BalouWindow   *mainwin;
  BalouWindow   *windows;
  gint           nwindows;

  GdkRectangle   fader_area;
};


void  balou_init      (Balou        *balou,
                       GdkDisplay   *display,
                       GdkScreen    *mainscreen,
                       gint          mainmonitor,
                       BalouTheme   *theme);
void  balou_fadein    (Balou        *balou,
                       const gchar  *text);
int   balou_run       (Balou        *balou,
                       GtkWidget    *dialog);
void  balou_destroy   (Balou *balou);

G_END_DECLS;


#endif /* !__BALOU_H__ */
