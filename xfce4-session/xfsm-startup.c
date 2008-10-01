/* $Id$ */
/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib/gstdio.h>

#include <libxfcegui4/libxfcegui4.h>

#include <libxfsm/xfsm-util.h>

#include <xfce4-session/xfsm-compat-gnome.h>
#include <xfce4-session/xfsm-compat-kde.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-manager.h>
#include <xfce4-session/xfsm-splash-screen.h>

#include "xfsm-startup.h"



/*
   Prototypes
 */
static void     xfsm_startup_failsafe                (void);
static gboolean xfsm_startup_session_next_prio_group (void);
static void     xfsm_startup_child_watch             (GPid         pid,
                                                      gint         status,
                                                      gpointer     user_data);
static void     xfsm_startup_child_watch_destroy     (gpointer     user_data);
static gboolean xfsm_startup_handle_failed_client    (gpointer     data);

void
xfsm_startup_init (XfceRc *rc)
{
  /* nothing to be done here, currently */
}


static gboolean
destroy_splash (gpointer user_data)
{
  if (G_LIKELY (splash_screen != NULL))
    {
      xfsm_splash_screen_free (splash_screen);
      splash_screen = NULL;
    }

  return FALSE;
}


static const gchar*
figure_app_name (const gchar *program_path)
{
  static char progbuf[256];
  gchar *prog;

  prog = g_path_get_basename (program_path);

  /* Xfce applications */
  if (strcmp (prog, "xfce4-mixer") == 0)
    return _("Starting the Volume Controller");
  else if (strcmp (prog, "xfce4-panel") == 0)
    return _("Starting the Panel");
  else if (strcmp (prog, "xfdesktop") == 0)
    return _("Starting the Desktop Manager");
  else if (strcmp (prog, "xftaskbar4") == 0)
    return _("Starting the Taskbar");
  else if (strcmp (prog, "xfwm4") == 0)
    return _("Starting the Window Manager");

  /* Gnome applications */
  if (strcmp (prog, "gnome-terminal") == 0)
    return _("Starting the Gnome Terminal Emulator");

  /* KDE applications */
  if (strcmp (prog, "kate") == 0)
    return _("Starting the KDE Advanced Text Editor");
  else if (strcmp (prog, "klipper") == 0)
    return _("Starting the KDE Clipboard Manager");
  else if (strcmp (prog, "kmail") == 0)
    return _("Starting the KDE Mail Reader");
  else if (strcmp (prog, "knews") == 0)
    return _("Starting the KDE News Reader");
  else if (strcmp (prog, "konqueror") == 0)
    return _("Starting the Konqueror");
  else if (strcmp (prog, "konsole") == 0)
    return _("Starting the KDE Terminal Emulator");

  /* 3rd party applications */
  if (strcmp (prog, "beep-media-player") == 0)
    return _("Starting the Beep Media Player");
  else if (strncmp (prog, "gimp", 4) == 0)
    return _("Starting The Gimp");
  else if (strcmp (prog, "gvim") == 0)
    return _("Starting the VI Improved Editor");
  else if (strcmp (prog, "smproxy") == 0)
    return _("Starting the Session Management Proxy");
  else if (strcmp (prog, "xchat") == 0 || strcmp (prog, "xchat2") == 0)
    return _("Starting the X-Chat IRC Client");
  else if (strcmp (prog, "xmms") == 0)
    return _("Starting the X Multimedia System");
  else if (strcmp (prog, "xterm") == 0)
    return _("Starting the X Terminal Emulator");

  g_snprintf (progbuf, 256, _("Starting %s"), prog);

  return progbuf;
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
xfsm_startup_autostart_migrate (void)
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
                               "Encoding=UTF-8\n"
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



static gint
xfsm_startup_autostart_xdg (void)
{
  const gchar *try_exec;
  const gchar *type;
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

  /* migrate the old autostart location (if still present) */
  xfsm_startup_autostart_migrate ();

  files = xfce_resource_match (XFCE_RESOURCE_CONFIG, "autostart/*.desktop", TRUE);
  for (n = 0; files[n] != NULL; ++n)
    {
      rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, files[n], TRUE);
      if (G_UNLIKELY (rc == NULL))
        continue;

      xfce_rc_set_group (rc, "Desktop Entry");

      /* check the Hidden key */
      skip = xfce_rc_read_bool_entry (rc, "Hidden", FALSE);
      if (G_LIKELY (!skip))
        {
          /* check the OnlyShowIn setting */
          only_show_in = xfce_rc_read_list_entry (rc, "OnlyShowIn", ";");
          if (G_UNLIKELY (only_show_in != NULL))
            {
              /* check if "Xfce" is specified */
              for (m = 0, skip = TRUE; only_show_in[m] != NULL; ++m)
                if (g_ascii_strcasecmp (only_show_in[m], "Xfce") == 0)
                  {
                    skip = FALSE;
                    break;
                  }

              g_strfreev (only_show_in);
            }
          else
            {
              /* check the NotShowIn setting */
              not_show_in = xfce_rc_read_list_entry (rc, "NotShowIn", ";");
              if (G_UNLIKELY (not_show_in != NULL))
                {
                  /* check if "Xfce" is not specified */
                  for (m = 0; not_show_in[m] != NULL; ++m)
                    if (g_ascii_strcasecmp (not_show_in[m], "Xfce") == 0)
                      {
                        skip = TRUE;
                        break;
                      }

                  g_strfreev (not_show_in);
                }
            }
        }

      /* check the "Type" key */
      type = xfce_rc_read_entry (rc, "Type", NULL);
      if (G_UNLIKELY (!skip && type != NULL && g_ascii_strcasecmp (type, "Application") != 0))
        skip = TRUE;

      /* check the "TryExec" key */
      try_exec = xfce_rc_read_entry (rc, "TryExec", NULL);
      if (G_UNLIKELY (!skip && try_exec != NULL))
        skip = !xfsm_check_valid_exec (try_exec);

      /* execute the item */
      exec = xfce_rc_read_entry (rc, "Exec", NULL);
      if (G_LIKELY (!skip && exec != NULL))
        {
          /* query launch parameters */
          startup_notify = xfce_rc_read_bool_entry (rc, "StartupNotify", FALSE);
          terminal = xfce_rc_read_bool_entry (rc, "Terminal", FALSE);

          /* try to launch the command */
          if (!xfce_exec (exec, terminal, startup_notify, &error))
            {
              g_warning ("Unable to launch \"%s\" (specified by %s): %s", exec, files[n], error->message);
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



static void
xfsm_startup_autostart (void)
{
  gint n;

  n = xfsm_startup_autostart_xdg ();

  if (n > 0)
    {
      if (G_LIKELY (splash_screen != NULL))
        xfsm_splash_screen_next (splash_screen, _("Performing Autostart..."));

      g_timeout_add (2000, destroy_splash, NULL);
    }
  else
    {
      g_timeout_add (1000, destroy_splash, NULL);
    }
}



void
xfsm_startup_foreign (void)
{
  if (compat_kde)
    xfsm_compat_kde_startup (splash_screen);

  if (compat_gnome)
    xfsm_compat_gnome_startup (splash_screen);
}


void
xfsm_startup_begin (void)
{
  if (failsafe_mode)
    {
      xfsm_startup_failsafe ();
      xfsm_startup_autostart ();
    }
  else
    {
      xfsm_startup_session_continue ();
    }
}


static void
xfsm_startup_failsafe (void)
{
  GList *lp;

  for (lp = failsafe_clients; lp; lp = lp->next)
    {
      FailsafeClient *fclient = (FailsafeClient *) lp->data;

      /* FIXME: splash */
      /* let the user know whats going on */
      if (G_LIKELY (splash_screen != NULL))
        {
          xfsm_splash_screen_next (splash_screen,
                                   figure_app_name (fclient->command[0]));
        }

      /* start the application */
      xfsm_start_application (fclient->command, NULL, fclient->screen,
                              NULL, NULL, NULL);
      g_strfreev (fclient->command);
      g_free (fclient);
    }

  g_list_free (failsafe_clients);
  failsafe_clients = NULL;
}


static gboolean
xfsm_startup_session_client (XfsmProperties *properties)
{
  gchar **argv;
  gint    argc;
  GPid    pid;
  gint    n;

  /* generate the argument vector for the application (expanding variables) */
  argc = g_strv_length (properties->restart_command);
  argv = g_new (gchar *, argc + 1);
  for (n = 0; n < argc; ++n)
    argv[n] = xfce_expand_variables (properties->restart_command[n], NULL);
  argv[n] = NULL;

  /* fork a new process for the application */
#ifdef HAVE_VFORK
  pid = vfork ();
#else
  pid = fork ();
#endif

  /* handle the child process */
  if (pid == 0)
    {
      /* execute the application here */
      execvp (argv[0], argv);
      _exit (127);
    }

  /* cleanup */
  g_strfreev (argv);

  /* check if we failed to fork */
  if (G_UNLIKELY (pid < 0))
    {
      /* tell the user that we failed to fork */
      perror ("Failed to fork new process");
      return FALSE;
    }

  /* watch the child process */
  g_child_watch_add_full (G_PRIORITY_LOW, pid, xfsm_startup_child_watch, g_strdup (properties->client_id), xfsm_startup_child_watch_destroy);
  properties->startup_timeout_id = g_timeout_add (STARTUP_TIMEOUT,
                                                  xfsm_startup_handle_failed_client,
                                                  properties);
  return TRUE;
}


void
xfsm_startup_session_continue ()
{
  gboolean client_started = FALSE;

  /* try to start some clients.  if we fail to start anything in the current
   * priority group, move right to the next one.  if we *did* start something,
   * the failed/registered handlers will take care of moving us on to the
   * next priority group */
  while (client_started == FALSE && pending_properties != NULL)
    client_started = xfsm_startup_session_next_prio_group ();

  if (G_UNLIKELY (client_started == FALSE && pending_properties == NULL))
    {
      /* we failed to start anything, and we don't have anything else,
       * to start, so just move on to the autostart items and signal
       * the manager that we're finished */
      xfsm_startup_autostart ();
      xfsm_manager_signal_startup_done ();
    }
}


/* returns TRUE if we started anything, FALSE if we didn't */
static gboolean
xfsm_startup_session_next_prio_group (void)
{
  XfsmProperties *properties;
  gint            cur_prio_group;
  gboolean        client_started = FALSE;

  /* can this ever happen? */
  while (G_UNLIKELY (pending_properties && pending_properties->data == NULL))
    pending_properties = g_list_delete_link (pending_properties, pending_properties);

  if (pending_properties == NULL)
    return FALSE;

  /* determine prio group for this run */
  properties = (XfsmProperties *) pending_properties->data;
  cur_prio_group = properties->priority;

  xfsm_verbose ("Starting apps in prio group %d\n", cur_prio_group);

  while (pending_properties)
    {
      XfsmProperties *properties = (XfsmProperties *) pending_properties->data;

      if (G_UNLIKELY (properties == NULL))
        {
          pending_properties = g_list_delete_link (pending_properties, pending_properties);
          continue;
        }

      /* quit if we've hit all the clients in the current prio group */
      if (properties->priority != cur_prio_group)
        break;

      /* FIXME: splash */
      if (G_LIKELY (splash_screen != NULL))
        {
          xfsm_splash_screen_next (splash_screen,
                                   figure_app_name (properties->program));
        }

      /* it's not pending anymore... */
      pending_properties = g_list_remove (pending_properties, properties);

      if (G_LIKELY (xfsm_startup_session_client (properties)))
        {
          starting_properties = g_list_append (starting_properties, properties);
          client_started = TRUE;
        }
      else
        {
          /* if starting the app failed, just ditch it */
          xfsm_manager_handle_failed_client (properties);
          xfsm_properties_free (properties);
        }
    }

  return client_started;
}


static void
xfsm_startup_child_watch (GPid     pid,
                          gint     status,
                          gpointer user_data)
{
  XfsmProperties *properties;
  const gchar    *client_id = user_data;
  GList          *lp;

  /* check if we have a starting process with the given client_id */
  for (lp = starting_properties; lp != NULL; lp = lp->next)
    {
      /* check if this properties matches */
      properties = (XfsmProperties *) lp->data;
      if (strcmp (properties->client_id, client_id) == 0)
        {
          if (properties->startup_timeout_id)
            {
              g_source_remove (properties->startup_timeout_id);
              properties->startup_timeout_id = 0;
            }

          /* continue startup, this client failed most probably */
          xfsm_startup_handle_failed_client (properties);
          break;
        }
    }
}


static void
xfsm_startup_child_watch_destroy (gpointer user_data)
{
  /* release the client_id */
  g_free (user_data);
}


static gboolean
xfsm_startup_handle_failed_client (gpointer data)
{
  XfsmProperties *properties = (XfsmProperties *) data;

  properties->startup_timeout_id = 0;

  xfsm_manager_handle_failed_client (properties);

  starting_properties = g_list_remove (starting_properties, properties);
  xfsm_properties_free (properties);

  if (starting_properties == NULL)
    {
      /* everything has finished starting or failed; continue startup */
      xfsm_startup_session_continue ();
    }

  return FALSE;
}


