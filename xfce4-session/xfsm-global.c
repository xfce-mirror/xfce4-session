/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_SYS_TYPE_SH
#include <sys/types.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include <xfce4-session/xfsm-global.h>

#include <libxfsm/xfsm-util.h>


/* global variables */
gboolean          verbose = FALSE;
XfsmSplashScreen *splash_screen = NULL;

void
xfsm_failsafe_client_free (FailsafeClient *fclient)
{
  if (fclient->command)
    g_strfreev (fclient->command);
  g_free (fclient);
}


void
xfsm_enable_verbose (void)
{
  if (!verbose)
    {
      verbose = TRUE;
      printf ("xfce4-session: Session Manager running in verbose mode.\n");
    }
}

gboolean
xfsm_is_verbose_enabled (void)
{
  return verbose;
}

void
xfsm_verbose_real (const char *func,
                   const char *file,
                   int line,
                   const char *format,
                   ...)
{
  static FILE *fp = NULL;
  gchar       *logfile;
  va_list      valist;

  if (G_UNLIKELY (fp == NULL))
    {
      logfile = xfce_get_homefile (".xfce4-session.verbose-log", NULL);

      /* rename an existing log file to -log.last */
      if (logfile && g_file_test (logfile, G_FILE_TEST_EXISTS))
        {
          gchar *oldlogfile = g_strdup_printf ("%s.last", logfile);
          if (oldlogfile)
            {
              rename (logfile, oldlogfile);
              g_free (oldlogfile);
            }
        }

      fp = fopen (logfile, "w");
      g_free (logfile);
      fprintf(fp, "log file opened\n");
    }

  fprintf (fp, "TRACE[%s:%d] %s(): ", file, line, func);
  va_start (valist, format);
  vfprintf (fp, format, valist);
  fflush (fp);
  va_end (valist);
}


gchar *
xfsm_generate_client_id (SmsConn sms_conn)
{
  static char *addr = NULL;
  static int   sequence = 0;
  char        *sms_id;
  char        *id = NULL;

  if (sms_conn != NULL)
    {
      sms_id = SmsGenerateClientID (sms_conn);
      if (sms_id != NULL)
        {
          id = g_strdup (sms_id);
          free (sms_id);
        }
    }

  if (id == NULL)
    {
      if (addr == NULL)
        {
          /*
           * Faking our IP address, the 0 below is "unknown"
           * address format (1 would be IP, 2 would be DEC-NET
           * format). Stolen from KDE :-)
           */
          addr = g_strdup_printf ("0%.8x", g_random_int ());
        }

      id = (char *) g_malloc (50);
      g_snprintf (id, 50, "1%s%.13ld%.10d%.4d", addr,
                  (long) time (NULL), (int) getpid (), sequence);
      sequence = (sequence + 1) % 10000;
    }

  return id;
}


GdkPixbuf *
xfsm_load_session_preview (const gchar *name)
{
  GdkDisplay *display;
  GdkPixbuf  *pb = NULL;
  gchar *display_name;
  gchar *filename;
  gchar *path;

  /* determine thumb file */
  display = gdk_display_get_default ();
  display_name = xfsm_gdk_display_get_fullname (display);
  path = g_strconcat ("sessions/thumbs-", display_name, "/", name, ".png", NULL);
  filename = xfce_resource_lookup (XFCE_RESOURCE_CACHE, path);
  g_free (display_name);
  g_free (path);

  if (filename != NULL)
    pb = gdk_pixbuf_new_from_file (filename, NULL);
  g_free (filename);

  return pb;
}


GValue *
xfsm_g_value_new (GType gtype)
{
  GValue *value = g_new0 (GValue, 1);
  g_value_init (value, gtype);
  return value;
}


void
xfsm_g_value_free (GValue *value)
{
  if (G_LIKELY (value))
    {
      g_value_unset (value);
      g_free (value);
    }
}

