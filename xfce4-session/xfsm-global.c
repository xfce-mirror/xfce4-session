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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#include <stdio.h>

#include <libxfce4util/libxfce4util.h>

#include <xfce4-session/shutdown.h>
#include <xfce4-session/xfsm-global.h>


/* global variables */
gboolean          verbose = FALSE;
GList            *pending_properties = NULL;
GList            *restart_properties = NULL;
GList            *running_clients = NULL;
gchar            *session_name = NULL;
gchar            *session_file = NULL;
GList            *failsafe_clients = NULL;
gboolean          failsafe_mode = TRUE;
gint              shutdown_type = SHUTDOWN_LOGOUT;
XfsmSplashScreen *splash_screen = NULL;

void
xfsm_enable_verbose (void)
{
  if (!verbose)
    {
      verbose = TRUE;
      printf ("xfce4-session: Session Manager running in verbose mode.\n");
    }
}


void
xfsm_verbose_real (const gchar *format, ...)
{
  static FILE *fp = NULL;
  gchar       *logfile;
  va_list      valist;
  
  if (G_UNLIKELY (fp == NULL))
    {
      logfile = xfce_get_homefile (".xfce4-session.verbose-log", NULL);
      fp = fopen (logfile, "w");
      g_free (logfile);
    }
  
  va_start (valist, format);
  vfprintf (fp, format, valist);
  fflush (fp);
  va_end (valist);
}
