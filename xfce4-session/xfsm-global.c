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
