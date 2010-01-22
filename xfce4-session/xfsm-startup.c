/* $Id$ */
/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>
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

#include <libxfce4ui/libxfce4ui.h>

#include <libxfsm/xfsm-util.h>

#include <xfce4-session/xfsm-compat-gnome.h>
#include <xfce4-session/xfsm-compat-kde.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-manager.h>
#include <xfce4-session/xfsm-splash-screen.h>

#include "xfsm-startup.h"


typedef struct
{
  XfsmManager    *manager;
  XfsmProperties *properties;
} XfsmStartupData;


/*
   Prototypes
 */
static void     xfsm_startup_failsafe                (XfsmManager *manager);

static gboolean xfsm_startup_session_next_prio_group (XfsmManager *manager);

static void     xfsm_startup_data_free               (XfsmStartupData *sdata);
static void     xfsm_startup_child_watch             (GPid         pid,
                                                      gint         status,
                                                      gpointer     user_data);
static gboolean xfsm_startup_timeout                 (gpointer     data);

static void     xfsm_startup_handle_failed_startup   (XfsmProperties *properties,
                                                      XfsmManager    *manager);

void
xfsm_startup_init (XfconfChannel *channel)
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
          xfsm_verbose ("Autostart: running command \"%s\"\n", exec);
          if (!xfce_spawn_command_line_on_screen (gdk_screen_get_default (),
                                                  exec,
                                                  terminal,
                                                  startup_notify,
                                                  &error))
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
xfsm_startup_foreign (XfsmManager *manager)
{
  if (xfsm_manager_get_compat_startup(manager, XFSM_MANAGER_COMPAT_KDE))
    xfsm_compat_kde_startup (splash_screen);

  if (xfsm_manager_get_compat_startup(manager, XFSM_MANAGER_COMPAT_GNOME))
    xfsm_compat_gnome_startup (splash_screen);
}


void
xfsm_startup_begin (XfsmManager *manager)
{
  if (xfsm_manager_get_use_failsafe_mode (manager))
    {
      xfsm_startup_failsafe (manager);
      xfsm_startup_autostart ();
      xfsm_manager_signal_startup_done (manager);
    }
  else
    {
      xfsm_startup_session_continue (manager);
    }
}


static void
xfsm_startup_failsafe (XfsmManager *manager)
{
  GQueue *failsafe_clients = xfsm_manager_get_queue (manager, XFSM_MANAGER_QUEUE_FAILSAFE_CLIENTS);
  FailsafeClient *fclient;

  while ((fclient = g_queue_pop_head (failsafe_clients)))
    {
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
      xfsm_failsafe_client_free (fclient);
    }
}


gboolean
xfsm_startup_start_properties (XfsmProperties *properties,
                               XfsmManager    *manager)
{
  XfsmStartupData *child_watch_data;
  XfsmStartupData *startup_timeout_data;
  gchar          **restart_command;
  gchar          **argv;
  gint             argc;
  gint             n;
  const gchar     *current_directory;
  GPid             pid;

  /* release any possible old resources related to a previous startup */
  xfsm_properties_set_default_child_watch (properties);

  /* generate the argument vector for the application (expanding variables) */
  restart_command = xfsm_properties_get_strv (properties, SmRestartCommand);
  argc = g_strv_length (restart_command);
  argv = g_new (gchar *, argc + 1);
  for (n = 0; n < argc; ++n)
    argv[n] = xfce_expand_variables (restart_command[n], NULL);
  argv[n] = NULL;

  current_directory = xfsm_properties_get_string (properties, SmCurrentDirectory);

  /* fork a new process for the application */
#ifdef HAVE_VFORK
  /* vfork() doesn't allow you to do anything but call exec*() or _exit(),
   * so if we need to set the working directory, we can't use vfork() */
  if (current_directory == NULL)
    pid = vfork ();
  else
#endif
    pid = fork ();

  /* handle the child process */
  if (pid == 0)
    {
      /* execute the application here */
      if (current_directory)
        {
          if (chdir (current_directory))
            g_warning ("Unable to chdir to \"%s\": %s", current_directory, strerror (errno));
        }
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

  properties->pid = pid;

  /* set a watch to make sure the child doesn't quit before registering */
  child_watch_data = g_new (XfsmStartupData, 1);
  child_watch_data->manager = g_object_ref (manager);
  child_watch_data->properties = properties;
  g_child_watch_add_full (G_PRIORITY_LOW, properties->pid,
                          xfsm_startup_child_watch, child_watch_data,
                          (GDestroyNotify) xfsm_startup_data_free);

  /* set a timeout -- client must register in a a certain amount of time
   * or it's assumed to be broken/have issues. */
  startup_timeout_data = g_new (XfsmStartupData, 1);
  startup_timeout_data->manager = g_object_ref (manager);
  startup_timeout_data->properties = properties;
  properties->startup_timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT,
                                                       STARTUP_TIMEOUT,
                                                       xfsm_startup_timeout,
                                                       startup_timeout_data,
                                                       (GDestroyNotify) xfsm_startup_data_free);

  return TRUE;
}


void
xfsm_startup_session_continue (XfsmManager *manager)
{
  GQueue  *pending_properties = xfsm_manager_get_queue (manager, XFSM_MANAGER_QUEUE_PENDING_PROPS);
  gboolean client_started = FALSE;

  /* try to start some clients.  if we fail to start anything in the current
   * priority group, move right to the next one.  if we *did* start something,
   * the failed/registered handlers will take care of moving us on to the
   * next priority group */
  while (client_started == FALSE && g_queue_peek_head (pending_properties) != NULL)
    client_started = xfsm_startup_session_next_prio_group (manager);

  if (G_UNLIKELY (client_started == FALSE && g_queue_peek_head (pending_properties) == NULL))
    {
      /* we failed to start anything, and we don't have anything else,
       * to start, so just move on to the autostart items and signal
       * the manager that we're finished */
      xfsm_startup_autostart ();
      xfsm_manager_signal_startup_done (manager);
    }
}


/* returns TRUE if we started anything, FALSE if we didn't */
static gboolean
xfsm_startup_session_next_prio_group (XfsmManager *manager)
{
  GQueue         *pending_properties = xfsm_manager_get_queue (manager, XFSM_MANAGER_QUEUE_PENDING_PROPS);
  GQueue         *starting_properties = xfsm_manager_get_queue (manager, XFSM_MANAGER_QUEUE_STARTING_PROPS);
  XfsmProperties *properties;
  gint            cur_prio_group;
  gboolean        client_started = FALSE;

  properties = (XfsmProperties *) g_queue_peek_head (pending_properties);
  if (properties == NULL)
    return FALSE;

  cur_prio_group = xfsm_properties_get_uchar (properties, GsmPriority, 50);

  xfsm_verbose ("Starting apps in prio group %d\n", cur_prio_group);

  while ((properties = g_queue_pop_head (pending_properties)))
    {
      /* quit if we've hit all the clients in the current prio group */
      if (xfsm_properties_get_uchar (properties, GsmPriority, 50) != cur_prio_group)
        {
          /* we're not starting this one yet; put it back */
          g_queue_push_head (pending_properties, properties);
          break;
        }

      /* FIXME: splash */
      if (G_LIKELY (splash_screen != NULL))
        {
          const gchar *app_name = NULL;
          const gchar *desktop_file;
          XfceRc      *rcfile = NULL;

          desktop_file = xfsm_properties_get_string (properties, GsmDesktopFile);

          if (desktop_file)
            {
              rcfile = xfce_rc_simple_open (desktop_file, TRUE);
              if (rcfile)
                {
                  xfce_rc_set_group (rcfile, "Desktop Entry");
                  app_name = xfce_rc_read_entry (rcfile, "Name", NULL);
                }
            }

          if (!app_name)
            app_name = figure_app_name (xfsm_properties_get_string (properties,
                                                                    SmProgram));

          xfsm_splash_screen_next (splash_screen, app_name);

          if (rcfile)
            {
              /* delay closing because app_name belongs to the rcfile
               * if we found it in the file */
              xfce_rc_close (rcfile);
            }
        }

      if (G_LIKELY (xfsm_startup_start_properties (properties, manager)))
        {
          g_queue_push_tail (starting_properties, properties);
          client_started = TRUE;
        }
      else
        {
          /* if starting the app failed, let the manager handle it */
          if (xfsm_manager_handle_failed_properties (manager, properties) == FALSE)
            xfsm_properties_free (properties);
        }
    }

  return client_started;
}


static void
xfsm_startup_data_free (XfsmStartupData *sdata)
{
  g_object_unref (sdata->manager);
  g_free (sdata);
}


static void
xfsm_startup_child_watch (GPid     pid,
                          gint     status,
                          gpointer user_data)
{
  XfsmStartupData *cwdata = user_data;
  GQueue          *starting_properties;

  xfsm_verbose ("Client Id = %s, PID %d exited with status %d\n",
                cwdata->properties->client_id, (gint)pid, status);

  cwdata->properties->child_watch_id = 0;
  cwdata->properties->pid = -1;

  starting_properties = xfsm_manager_get_queue (cwdata->manager, XFSM_MANAGER_QUEUE_STARTING_PROPS);
  if (g_queue_find (starting_properties, cwdata->properties) != NULL)
    {
      xfsm_verbose ("Client Id = %s died while starting up\n",
                    cwdata->properties->client_id);
      xfsm_startup_handle_failed_startup (cwdata->properties, cwdata->manager);
    }

  /* NOTE: cwdata->properties could have been freed by
   * xfsm_startup_handle_failed_startup() above, so don't try to access
   * any of its members here. */

  g_spawn_close_pid (pid);
}


static gboolean
xfsm_startup_timeout (gpointer data)
{
  XfsmStartupData *stdata = data;

  xfsm_verbose ("Client Id = %s failed to register in time\n",
                stdata->properties->client_id);

  stdata->properties->startup_timeout_id = 0;
  xfsm_startup_handle_failed_startup (stdata->properties, stdata->manager);

  return FALSE;
}


static void
xfsm_startup_handle_failed_startup (XfsmProperties *properties,
                                    XfsmManager    *manager)
{
  GQueue *starting_properties = xfsm_manager_get_queue (manager, XFSM_MANAGER_QUEUE_STARTING_PROPS);

  /* if our timer hasn't run out yet, kill it */
  if (properties->startup_timeout_id > 0)
    {
      g_source_remove (properties->startup_timeout_id);
      properties->startup_timeout_id = 0;
    }

  xfsm_properties_set_default_child_watch (properties);

  /* not starting anymore, so remove it from the list.  tell the manager
   * it failed, and let it do its thing. */
  g_queue_remove (starting_properties, properties);
  if (xfsm_manager_handle_failed_properties (manager, properties) == FALSE)
      xfsm_properties_free (properties);

  if (g_queue_peek_head (starting_properties) == NULL
      && xfsm_manager_get_state (manager) == XFSM_MANAGER_STARTUP)
    {
      /* everything has finished starting or failed; continue startup */
      xfsm_startup_session_continue (manager);
    }
}


