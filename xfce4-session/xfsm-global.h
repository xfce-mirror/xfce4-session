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

#include <X11/SM/SMlib.h>

#include <xfce4-session/xfsm-splash-screen.h>

typedef struct _FailsafeClient FailsafeClient;
struct _FailsafeClient
{
  gchar    **command;
  GdkScreen *screen;
};

void xfsm_failsafe_client_free (FailsafeClient *fclient);


#define DEFAULT_SESSION_NAME "Default"


extern gboolean          verbose;
extern XfsmSplashScreen *splash_screen;


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

gchar *xfsm_generate_client_id (SmsConn sms_conn) G_GNUC_PURE;

GdkPixbuf *xfsm_load_session_preview (const gchar *name);

GValue *xfsm_g_value_new (GType gtype);
void    xfsm_g_value_free (GValue *value);


gint xfsm_startup_autostart_xdg (gboolean start_at_spi);

#endif /* !__XFSM_GLOBAL_H__ */
