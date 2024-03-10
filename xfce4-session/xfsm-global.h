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

#ifndef __XFSM_GLOBAL_H__
#define __XFSM_GLOBAL_H__

#include <glib.h>

#include <xfce4-session/xfsm-shutdown.h> /* XfsmShutdownType */

#include "settings/xfae-model.h" /* XfsmRunHook */


#define DEFAULT_SESSION_NAME "Default"

extern gboolean          verbose;

#if defined(G_HAVE_ISO_VARARGS)

#define xfsm_verbose(...)\
G_STMT_START{ \
  if (G_UNLIKELY (verbose)) \
    xfsm_verbose_real (__func__, __FILE__, __LINE__, __VA_ARGS__); \
}G_STMT_END

#else

#define xfsm_verbose(...)

#endif

void xfsm_enable_verbose (void);
gboolean xfsm_is_verbose_enabled (void);
void xfsm_verbose_real (const char *func,
                        const char *file,
                        int line,
                        const char *format,
                        ...) G_GNUC_PRINTF (4, 5);

GValue *xfsm_g_value_new (GType gtype);
void    xfsm_g_value_free (GValue *value);

gint    xfsm_launch_desktop_files_on_login    (gboolean         start_at_spi);
gint    xfsm_launch_desktop_files_on_shutdown (gboolean         start_at_spi,
                                               XfsmShutdownType shutdown_type);
gint    xfsm_launch_desktop_files_on_run_hook (gboolean         start_at_spi,
                                               XfsmRunHook      run_hook);

#endif /* !__XFSM_GLOBAL_H__ */
