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
#include "config.h"
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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <gio/gio.h>
#include <glib/gprintf.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>

#include "libxfsm/xfsm-util.h"

#include "xfsm-global.h"


/* global variables */
gboolean verbose = FALSE;


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
  gchar *logfile;
  va_list valist;

  if (G_UNLIKELY (fp == NULL))
    {
      logfile = xfce_get_homefile (".xfce4-session.verbose-log", NULL);

      /* rename an existing log file to -log.last */
      if (logfile && g_file_test (logfile, G_FILE_TEST_EXISTS))
        {
          gchar *oldlogfile = g_strdup_printf ("%s.last", logfile);
          if (oldlogfile)
            {
              if (rename (logfile, oldlogfile) != 0)
                {
                  g_warning ("unable to rename logfile");
                }
              g_free (oldlogfile);
            }
        }

      if (logfile)
        {
          fp = fopen (logfile, "w");
          g_free (logfile);
          fprintf (fp, "log file opened\n");
        }
    }

  if (fp == NULL)
    {
      return;
    }

  fprintf (fp, "TRACE[%s:%d] %s(): ", file, line, func);
  va_start (valist, format);
  vfprintf (fp, format, valist);
  fflush (fp);
  va_end (valist);
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


static gboolean
xfsm_check_valid_exec (const gchar *exec)
{
  gboolean result = TRUE;
  gchar *tmp;
  gchar *p;

  if (*exec == '/')
    {
      result = (access (exec, X_OK) == 0);
    }
  else
    {
      tmp = g_strdup (exec);
      p = strchr (tmp, ' ');
      if (G_UNLIKELY (p != NULL))
        *p = '\0';

      p = g_find_program_in_path (tmp);
      g_free (tmp);

      if (G_UNLIKELY (p == NULL))
        {
          result = FALSE;
        }
      else
        {
          result = (access (p, X_OK) == 0);
          g_free (p);
        }
    }

  return result;
}

gint
xfsm_launch_desktop_files_on_shutdown (gboolean start_at_spi,
                                       XfsmShutdownType shutdown_type)
{
  switch (shutdown_type)
    {
    case XFSM_SHUTDOWN_LOGOUT:
      return xfsm_launch_desktop_files_on_run_hook (start_at_spi, XFSM_RUN_HOOK_LOGOUT);

    case XFSM_SHUTDOWN_SHUTDOWN:
      return xfsm_launch_desktop_files_on_run_hook (start_at_spi, XFSM_RUN_HOOK_SHUTDOWN);

    case XFSM_SHUTDOWN_RESTART:
      return xfsm_launch_desktop_files_on_run_hook (start_at_spi, XFSM_RUN_HOOK_RESTART);

    case XFSM_SHUTDOWN_SUSPEND:
      return xfsm_launch_desktop_files_on_run_hook (start_at_spi, XFSM_RUN_HOOK_SUSPEND);

    case XFSM_SHUTDOWN_HIBERNATE:
      return xfsm_launch_desktop_files_on_run_hook (start_at_spi, XFSM_RUN_HOOK_HIBERNATE);

    case XFSM_SHUTDOWN_HYBRID_SLEEP:
      return xfsm_launch_desktop_files_on_run_hook (start_at_spi, XFSM_RUN_HOOK_HYBRID_SLEEP);

    case XFSM_SHUTDOWN_SWITCH_USER:
      return xfsm_launch_desktop_files_on_run_hook (start_at_spi, XFSM_RUN_HOOK_SWITCH_USER);

    default:
      g_critical ("Failed to convert shutdown type '%d' to run hook name.", shutdown_type);
      return FALSE;
    }
}



gint
xfsm_launch_desktop_files_on_login (gboolean start_at_spi)
{
  return xfsm_launch_desktop_files_on_run_hook (start_at_spi, XFSM_RUN_HOOK_LOGIN);
}



gint
xfsm_launch_desktop_files_on_run_hook (gboolean start_at_spi,
                                       XfsmRunHook run_hook)
{
  const gchar *try_exec;
  const gchar *type;
  XfsmRunHook run_hook_from_file;
  gchar *exec;
  gboolean startup_notify;
  gboolean terminal;
  gboolean skip;
  GError *error = NULL;
  XfceRc *rc;
  gchar **files;
  gchar **only_show_in;
  gchar **not_show_in;
  gint started = 0;
  gint n, m;
  gchar *filename;
  const gchar *pattern;
  gchar *uri;

  /* pattern for only at-spi desktop files or everything */
  if (start_at_spi)
    pattern = "autostart/at-spi-*.desktop";
  else
    pattern = "autostart/*.desktop";

  files = xfce_resource_match (XFCE_RESOURCE_CONFIG, pattern, TRUE);
  for (n = 0; files[n] != NULL; ++n)
    {
      rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, files[n], TRUE);
      if (G_UNLIKELY (rc == NULL))
        continue;

      xfce_rc_set_group (rc, "Desktop Entry");

      /* check the Hidden key */
      skip = xfce_rc_read_bool_entry (rc, "Hidden", FALSE);
      run_hook_from_file = xfce_rc_read_int_entry (rc, "RunHook", XFSM_RUN_HOOK_LOGIN);

      /* only execute scripts with match the requested run hook */
      if (run_hook != run_hook_from_file)
        skip = TRUE;

      if (G_LIKELY (!skip))
        {
          xfsm_verbose ("hidden set\n");

          if (xfce_rc_read_bool_entry (rc, "X-XFCE-Autostart-Override", FALSE))
            {
              /* override the OnlyShowIn check */
              skip = FALSE;
              xfsm_verbose ("X-XFCE-Autostart-Override set, launching\n");
            }
          else
            {
              /* check the OnlyShowIn setting */
              only_show_in = xfce_rc_read_list_entry (rc, "OnlyShowIn", ";");
              if (G_UNLIKELY (only_show_in != NULL))
                {
                  /* check if "XFCE" is specified */
                  for (m = 0, skip = TRUE; only_show_in[m] != NULL; ++m)
                    {
                      if (g_ascii_strcasecmp (only_show_in[m], "XFCE") == 0)
                        {
                          skip = FALSE;
                          xfsm_verbose ("only show in XFCE set, launching\n");
                          break;
                        }
                    }

                  g_strfreev (only_show_in);
                }
            }

          /* check the NotShowIn setting */
          not_show_in = xfce_rc_read_list_entry (rc, "NotShowIn", ";");
          if (G_UNLIKELY (not_show_in != NULL))
            {
              /* check if "Xfce" is not specified */
              for (m = 0; not_show_in[m] != NULL; ++m)
                if (g_ascii_strcasecmp (not_show_in[m], "XFCE") == 0)
                  {
                    skip = TRUE;
                    xfsm_verbose ("NotShowIn Xfce set, skipping\n");
                    break;
                  }

              g_strfreev (not_show_in);
            }

          /* skip at-spi launchers if not in at-spi mode or don't skip
           * them no matter what the OnlyShowIn key says if only
           * launching at-spi */
          filename = g_path_get_basename (files[n]);
          if (g_str_has_prefix (filename, "at-spi-"))
            {
              skip = !start_at_spi;
              xfsm_verbose ("start_at_spi (a11y support), %s\n", skip ? "skipping" : "showing");
            }
          g_free (filename);
        }

      /* check the "Type" key */
      type = xfce_rc_read_entry (rc, "Type", NULL);
      if (G_UNLIKELY (!skip && type != NULL && g_ascii_strcasecmp (type, "Application") != 0))
        {
          skip = TRUE;
          xfsm_verbose ("Type == Application, skipping\n");
        }

      /* check the "TryExec" key */
      try_exec = xfce_rc_read_entry (rc, "TryExec", NULL);
      if (G_UNLIKELY (!skip && try_exec != NULL))
        {
          skip = !xfsm_check_valid_exec (try_exec);
          if (skip)
            xfsm_verbose ("TryExec set and xfsm_check_valid_exec failed, skipping\n");
        }

      /* expand the field codes */
      filename = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, files[n]);
      uri = g_filename_to_uri (filename, NULL, NULL);
      g_free (filename);
      exec = xfce_expand_desktop_entry_field_codes (xfce_rc_read_entry (rc, "Exec", NULL),
                                                    NULL,
                                                    xfce_rc_read_entry (rc, "Icon", NULL),
                                                    xfce_rc_read_entry (rc, "Name", NULL),
                                                    uri,
                                                    FALSE);
      g_free (uri);

      /* execute the item */
      if (G_LIKELY (!skip && exec != NULL))
        {
          /* query launch parameters */
          startup_notify = xfce_rc_read_bool_entry (rc, "StartupNotify", FALSE);
          terminal = xfce_rc_read_bool_entry (rc, "Terminal", FALSE);

          /* try to launch the command */
          xfsm_verbose ("Autostart: running command \"%s\"\n", exec);
          if (!xfce_spawn_command_line (gdk_screen_get_default (),
                                        exec,
                                        terminal,
                                        startup_notify,
                                        TRUE,
                                        &error))
            {
              g_warning ("Unable to launch \"%s\" (specified by %s): %s", exec, files[n], error->message);
              xfsm_verbose ("Unable to launch \"%s\" (specified by %s): %s\n", exec, files[n], error->message);
              g_error_free (error);
              error = NULL;
            }
          else
            {
              ++started;
            }
        }

      /* cleanup */
      xfce_rc_close (rc);
      g_free (exec);
    }
  g_strfreev (files);

  return started;
}
