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

#ifndef __XFSM_GLOBAL_H__
#define __XFSM_GLOBAL_H__

#include <xfce4-session/xfsm-splash-screen.h>


typedef struct _FailsafeClient FailsafeClient;
struct _FailsafeClient
{
  gchar    **command;
  GdkScreen *screen;
};
  

#define DEFAULT_SESSION_NAME "Default"


extern gboolean          verbose;
extern GList            *pending_properties;
extern GList            *restart_properties;
extern GList            *running_clients;
extern gchar            *session_name;
extern gchar            *session_file;
extern GList            *failsafe_clients;
extern gboolean          failsafe_mode;
extern gint              shutdown_type;
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


#endif /* !__XFSM_GLOBAL_H__ */
