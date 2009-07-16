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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __XFSM_GLOBAL_H__
#define __XFSM_GLOBAL_H__

#include <glib.h>

#include <X11/SM/SMlib.h>

#include <xfce4-session/xfsm-splash-screen.h>

typedef enum
{
  XFSM_SHUTDOWN_ASK = 0,
  XFSM_SHUTDOWN_LOGOUT,
  XFSM_SHUTDOWN_HALT,
  XFSM_SHUTDOWN_REBOOT,
  XFSM_SHUTDOWN_SUSPEND,
  XFSM_SHUTDOWN_HIBERNATE,
} XfsmShutdownType;

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

#define xfsm_verbose(...)             \
G_STMT_START{                         \
  if (G_UNLIKELY (verbose))           \
    xfsm_verbose_real (__VA_ARGS__);  \
}G_STMT_END

#elif defined(G_HAVE_GNUC_VARARGS)

#define xfsm_verbose(format, ...)     \
G_STMT_START{                         \
  if (G_UNLIKELY (verbose))           \
    xfsm_verbose_real ( ## format);   \
}G_STMT_END
  
#endif

void xfsm_enable_verbose (void);
void xfsm_verbose_real (const gchar *format, ...) G_GNUC_PRINTF (1, 2);

gchar *xfsm_generate_client_id (SmsConn sms_conn) G_GNUC_PURE;

GdkPixbuf *xfsm_load_session_preview (const gchar *name);

GValue *xfsm_g_value_new (GType gtype);
void    xfsm_g_value_free (GValue *value);

#endif /* !__XFSM_GLOBAL_H__ */
