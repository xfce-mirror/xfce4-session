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

#ifndef __MODULE_H__
#define __MODULE_H__

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>


G_BEGIN_DECLS;

#define MODULE(obj) ((Module *)(obj))

typedef struct _Module Module;


Module      *module_load          (const gchar *path,
                                   const gchar *channel_name);

const gchar *module_engine        (const Module *module);

const gchar *module_name          (const Module *module);

const gchar *module_descr         (const Module *module);

const gchar *module_version       (const Module *module);

const gchar *module_author        (const Module *module);

const gchar *module_homepage      (const Module *module);

GdkPixbuf   *module_preview       (Module       *module);

gboolean     module_can_configure (const Module *module);

void         module_configure     (Module       *module,
                                   GtkWidget    *parent);

void         module_test          (Module       *module,
                                   GdkDisplay   *display);

void         module_free          (Module       *module);

G_END_DECLS;


#endif /* !__XFSM_SPLASH_MODULE_H__ */
