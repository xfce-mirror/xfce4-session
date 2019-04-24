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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <glib/gprintf.h>

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
          fprintf(fp, "log file opened\n");
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


static gboolean
xfsm_check_valid_exec (const gchar *exec)
{
  gboolean result = TRUE;
  gchar   *tmp;
  gchar   *p;

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



static void
xfsm_autostart_migrate (void)
{
  const gchar *entry;
  gchar        source_path[4096];
  gchar        target_path[4096];
  gchar       *source;
  gchar       *target;
  FILE        *fp;
  GDir        *dp;

  /* migrate the content */
  source = xfce_get_homefile ("Desktop", "Autostart/", NULL);
  dp = g_dir_open (source, 0, NULL);
  if (G_UNLIKELY (dp != NULL))
    {
      /* check if the LOCATION-CHANGED.txt file exists and the target can be opened */
      g_snprintf (source_path, 4096, "%s/LOCATION-CHANGED.txt", source);
      target = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, "autostart/", TRUE);
      if (G_LIKELY (target != NULL && !g_file_test (source_path, G_FILE_TEST_IS_REGULAR)))
        {
          g_message ("Trying to migrate autostart items from %s to %s...", source, target);

          for (;;)
            {
              entry = g_dir_read_name (dp);
              if (entry == NULL)
                break;

              /* determine full source and dest paths */
              g_snprintf (source_path, 4096, "%s%s", source, entry);
              g_snprintf (target_path, 4096, "%s%s", target, entry);

              /* try to move the file */
              if (rename (source_path, target_path) < 0)
                {
                  g_warning ("Failed to rename %s to %s: %s",
                              source_path, target_path,
                              g_strerror (errno));
                  continue;
                }

              /* check if the file is executable */
              if (!g_file_test (target_path, G_FILE_TEST_IS_EXECUTABLE))
                continue;

              /* generate a .desktop file for the executable file */
              g_snprintf (source_path, 4096, "%s.desktop", target_path);
              if (!g_file_test (source_path, G_FILE_TEST_IS_REGULAR))
                {
                  fp = fopen (source_path, "w");
                  if (G_LIKELY (fp != NULL))
                    {
                      fprintf (fp,
                               "# This file was automatically generated for the autostart\n"
                               "# item %s\n"
                               "[Desktop Entry]\n"
                               "Type=Application\n"
                               "Exec=%s\n"
                               "Hidden=False\n"
                               "Terminal=False\n"
                               "StartupNotify=False\n"
                               "Version=0.9.4\n"
                               "Name=%s\n",
                               entry, target_path, entry);
                      fclose (fp);
                    }
                  else
                    {
                      g_warning ("Failed to create a .desktop file for %s: %s",
                                 target_path, g_strerror (errno));
                    }
                }
            }

          /* create the LOCATION-CHANGED.txt file to let the user know */
          g_snprintf (source_path, 4096, "%s/LOCATION-CHANGED.txt", source);
          fp = fopen (source_path, "w");
          if (G_LIKELY (fp != NULL))
            {
              g_fprintf (fp, _("The location and the format of the autostart directory has changed.\n"
                               "The new location is\n"
                               "\n"
                               "  %s\n"
                               "\n"
                               "where you can place .desktop files to, that describe the applications\n"
                               "to start when you login to your Xfce desktop. The files in your old\n"
                               "autostart directory have been successfully migrated to the new\n"
                               "location.\n"
                               "You should delete this directory now.\n"), target);
              fclose (fp);
            }

          g_free (target);
        }

      g_dir_close (dp);
    }
}



gint
xfsm_launch_desktop_files_on_shutdown (gboolean         start_at_spi,
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
      g_error ("Failed to convert shutdown type '%d' to run hook name.", shutdown_type);
      return FALSE;
    }
}



gint
xfsm_launch_desktop_files_on_login (gboolean start_at_spi)
{
  return xfsm_launch_desktop_files_on_run_hook (start_at_spi, XFSM_RUN_HOOK_LOGIN);
}



gint
xfsm_launch_desktop_files_on_run_hook (gboolean    start_at_spi,
                                       XfsmRunHook run_hook)
{
  const gchar *try_exec;
  const gchar *type;
  XfsmRunHook  run_hook_from_file;
  const gchar *exec;
  gboolean     startup_notify;
  gboolean     terminal;
  gboolean     skip;
  GError      *error = NULL;
  XfceRc      *rc;
  gchar      **files;
  gchar      **only_show_in;
  gchar      **not_show_in;
  gint         started = 0;
  gint         n, m;
  gchar       *filename;
  const gchar *pattern;

  /* migrate the old autostart location (if still present) */
  xfsm_autostart_migrate ();

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
          xfsm_verbose("hidden set\n");

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

      /* execute the item */
      exec = xfce_rc_read_entry (rc, "Exec", NULL);
      if (G_LIKELY (!skip && exec != NULL))
        {
          /* query launch parameters */
          startup_notify = xfce_rc_read_bool_entry (rc, "StartupNotify", FALSE);
          terminal = xfce_rc_read_bool_entry (rc, "Terminal", FALSE);

          /* try to launch the command */
          xfsm_verbose ("Autostart: running command \"%s\"\n", exec);
          if (!xfce_spawn_command_line_on_screen (gdk_screen_get_default (),
                                                  exec,
                                                  terminal,
                                                  startup_notify,
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
    }
  g_strfreev (files);

  return started;
}
