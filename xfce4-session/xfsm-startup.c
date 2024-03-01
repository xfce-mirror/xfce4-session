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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
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
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
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
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#include <glib/gstdio.h>
#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#endif
#include <libxfce4ui/libxfce4ui.h>

#include <libxfsm/xfsm-util.h>

#include <xfce4-session/xfsm-compat-gnome.h>
#include <xfce4-session/xfsm-compat-kde.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-manager.h>
#include <xfce4-session/xfsm-startup.h>


typedef struct
{
  XfsmManager    *manager;
  XfsmProperties *properties;
} XfsmStartupData;

static gboolean xfsm_startup_session_next_prio_group (XfsmManager *manager);

static void     xfsm_startup_data_free               (XfsmStartupData *sdata);
static void     xfsm_startup_child_watch             (GPid         pid,
                                                      gint         status,
                                                      gpointer     user_data);
static gboolean xfsm_startup_timeout                 (gpointer     data);

static void     xfsm_startup_handle_failed_startup   (XfsmProperties *properties,
                                                      XfsmManager    *manager);


static pid_t running_sshagent = -1;
static pid_t running_gpgagent = -1;
static gboolean gpgagent_ssh_enabled = FALSE;



static pid_t
xfsm_gpg_agent_pid (const gchar *gpg_agent_info)
{
  pid_t        pid = -1;
  gchar      **fields;

  if (gpg_agent_info == NULL || *gpg_agent_info == '\0')
    return -1;

  fields = g_strsplit (gpg_agent_info, ":", 3);
  if (fields != NULL)
    {
      /* second field of GPG_AGENT_INFO is a PID */
      pid = atoi (fields[1]);
      g_strfreev (fields);
    }

  return pid;
}



static pid_t
xfsm_ssh_agent_pid (const gchar *ssh_agent_pid)
{
  if (ssh_agent_pid == NULL || *ssh_agent_pid == '\0')
    return -1;

  return atoi (ssh_agent_pid);
}



static pid_t
xfsm_startup_init_agent (const gchar *cmd,
                         const gchar *agent,
                         gboolean want_pid)
{
  gchar     *cmdoutput = NULL;
  GError    *error = NULL;
  gchar    **lines;
  guint      i;
  gchar     *p, *t;
  gchar     *variable, *value;
  pid_t      pid = -1;

  if (g_spawn_command_line_sync (cmd, &cmdoutput, NULL, NULL, &error))
    {
      if (G_UNLIKELY (cmdoutput == NULL))
        {
          g_message ("%s returned no variables to stdout", agent);
          return -1;
        }

      lines = g_strsplit (cmdoutput, "\n", -1);
      g_assert (lines != NULL);
      for (i = 0; lines[i] != NULL; i++)
        {
          p = strchr (lines[i], '=');
          if (G_UNLIKELY (p == NULL))
            continue;
          t = strchr (p + 1, ';');
          if (G_UNLIKELY (t == NULL))
            continue;

          variable = g_strndup (lines[i], p - lines[i]);
          value = g_strndup (p + 1, t - p - 1);

          /* try to get agent pid from the variable */
          if (want_pid && pid <= 0)
            {
              if (g_strcmp0 (variable, "SSH_AGENT_PID") == 0)
                pid = xfsm_ssh_agent_pid (value);
              else if (g_strcmp0 (variable, "GPG_AGENT_INFO") == 0)
                pid = xfsm_gpg_agent_pid (value);
            }

          g_setenv (variable, value, TRUE);

          g_free (variable);
          g_free (value);
        }
      g_strfreev (lines);
    }
  else
    {
      g_warning ("Failed to spawn %s: %s", agent, error->message);
      g_error_free (error);

      return -1;
    }

  g_free (cmdoutput);

  if (!want_pid)
    pid = 1;

  if (pid <= 0)
    g_warning ("%s returned no PID in the variables", agent);

  return pid;
}


static void xfsm_gpg_agent_shutdown(gboolean quiet)
{
      GError      *error = NULL;

      g_spawn_command_line_sync ("gpgconf --kill gpg-agent",
                                 NULL, NULL, NULL, &error);
      if (error)
        {
          if (!quiet)
            g_warning ("failed to kill gpg-agent via gpgconf, error was %s",
                       error->message);
          g_error_free (error);
        }
}


void
xfsm_startup_init (XfconfChannel *channel)
{
  gchar       *ssh_agent;
  gchar       *ssh_agent_path = NULL;
  gchar       *gpg_agent_path = NULL;
  gchar       *cmd;
  gchar       *cmdoutput = NULL;
  GError      *error = NULL;
  pid_t        agentpid;
  gboolean     gnome_keyring_found;

      /* if GNOME compatibility is enabled and gnome-keyring-daemon
       * is found, skip the gpg/ssh agent startup and wait for
       * gnome-keyring, which is probably what the user wants */
  if (xfconf_channel_get_bool (channel, "/compat/LaunchGNOME", FALSE))
    {
      cmd = g_find_program_in_path ("gnome-keyring-daemon");
      gnome_keyring_found = (cmd != NULL);
      g_free (cmd);

      if (gnome_keyring_found)
        {
          g_message ("GNOME compatibility is enabled and gnome-keyring-daemon is "
                     "found on the system. Skipping gpg/ssh-agent startup.");
          return;
        }
    }

  if (xfconf_channel_get_bool (channel, "/startup/gpg-agent/enabled", TRUE))
    {
      gpg_agent_path = g_find_program_in_path ("gpg-agent");
      if (gpg_agent_path == NULL)
        g_warning ("No GPG agent found");
    }

  if (xfconf_channel_get_bool (channel, "/startup/ssh-agent/enabled", TRUE))
    {
      ssh_agent = xfconf_channel_get_string (channel, "/startup/ssh-agent/type", NULL);

      if (ssh_agent == NULL
          || g_strcmp0 (ssh_agent, "ssh-agent") == 0)
        {
          ssh_agent_path = g_find_program_in_path ("ssh-agent");
          if (ssh_agent_path == NULL)
            g_warning ("No SSH authentication agent found");
        }
      else if (g_strcmp0 (ssh_agent, "gpg-agent") == 0)
        {
          if (gpg_agent_path != NULL)
             gpgagent_ssh_enabled = TRUE;
           else
               g_warning ("gpg-agent is configured as SSH agent, but gpg-agent is disabled or not found");
        }
      else
        {
          g_message ("Unknown SSH authentication agent \"%s\" set", ssh_agent);
        }
      g_free (ssh_agent);
    }

  if (G_LIKELY (ssh_agent_path != NULL || gpgagent_ssh_enabled))
    {
      agentpid = xfsm_ssh_agent_pid (g_getenv ("SSH_AGENT_PID"));

      /* check if the pid is still responding (ie not stale) */
      if (agentpid > 0 && kill (agentpid, 0) == 0)
        {
          g_message ("SSH authentication agent is already running");

          gpgagent_ssh_enabled = FALSE;
          g_free (ssh_agent_path);
          ssh_agent_path = NULL;
        }
      else
        {
          g_unsetenv ("SSH_AGENT_PID");
          g_unsetenv ("SSH_AUTH_SOCK");
        }

      if (ssh_agent_path != NULL)
        {
          cmd = g_strdup_printf ("%s -s", ssh_agent_path);
          /* keep this around for shutdown */
          running_sshagent = xfsm_startup_init_agent (cmd, "ssh-agent", TRUE);
          g_free (cmd);

          /* update dbus environment */
          if (LOGIND_RUNNING())
            {
              cmd = g_strdup_printf ("%s", "dbus-update-activation-environment --systemd SSH_AUTH_SOCK");
            }
          else
            {
              cmd = g_strdup_printf ("%s", "dbus-update-activation-environment SSH_AUTH_SOCK");
            }
          g_spawn_command_line_sync (cmd, &cmdoutput, NULL, NULL, &error);

          if (error)
            {
              g_warning ("failed to call dbus-update-activation-environment. Output was %s, error was %s",
                         cmdoutput, error->message);
            }

          g_free (cmd);
          g_free (ssh_agent_path);
          g_clear_pointer (&cmdoutput, g_free);
          g_clear_error (&error);
        }
    }

  if (G_LIKELY (gpg_agent_path != NULL))
    {
      xfsm_gpg_agent_shutdown (TRUE);
        {
          gboolean want_pid;
          gchar *cmd_tmp;
          gchar *envfile;

          g_unsetenv ("GPG_AGENT_INFO");

          envfile = xfce_resource_save_location (XFCE_RESOURCE_CACHE, "gpg-agent-info", FALSE);

          cmd_tmp = g_strdup_printf ("%s --sh --daemon%s", gpg_agent_path,
                                     gpgagent_ssh_enabled ?
                                     " --enable-ssh-support" : "");

          cmd = cmd_tmp;
          want_pid = FALSE;

          /* keep this around for shutdown */
          running_gpgagent = xfsm_startup_init_agent (cmd, "gpg-agent",
                                                      want_pid);

          g_free (cmd);
          g_free (envfile);
        }

      g_free (gpg_agent_path);
    }
}



void
xfsm_startup_shutdown (void)
{
  if (running_sshagent > 0)
    {
      if (kill (running_sshagent, SIGTERM) == 0)
        {
         /* make sure the env values are unset */
         g_unsetenv ("SSH_AGENT_PID");
         g_unsetenv ("SSH_AUTH_SOCK");
        }
      else
        {
          g_warning ("Failed to kill ssh-agent with pid %d", running_sshagent);
        }
    }

  if (running_gpgagent > 0)
    {
      xfsm_gpg_agent_shutdown (FALSE);
    }
}



static void
xfsm_startup_autostart (XfsmManager *manager)
{
  xfsm_launch_desktop_files_on_login (FALSE);
}



void
xfsm_startup_foreign (XfsmManager *manager)
{
  if (xfsm_manager_get_compat_startup(manager, XFSM_MANAGER_COMPAT_KDE))
    xfsm_compat_kde_startup ();

  if (xfsm_manager_get_compat_startup(manager, XFSM_MANAGER_COMPAT_GNOME))
    xfsm_compat_gnome_startup ();
}



static void
xfsm_startup_at_set_gtk_modules (void)
{
  const gchar  *old;
  gchar       **modules;
  guint         i;
  gboolean      found_gail = FALSE;
  gboolean      found_atk_bridge = FALSE;
  GString      *new;

  old = g_getenv ("GTK_MODULES");
  if (old != NULL && *old != '\0')
    {
      /* check which modules are already loaded */
      modules = g_strsplit (old, ":", -1);
      for (i = 0; modules[i] != NULL; i++)
        {
          if (strcmp (modules[i], "gail") == 0)
            found_gail = TRUE;
          else if (strcmp (modules[i], "atk-bridge") == 0)
            found_atk_bridge = TRUE;
        }
      g_strfreev (modules);

      if (!found_gail || !found_atk_bridge)
        {
          /* append modules to old value */
          new = g_string_new (old);
          if (!found_gail)
            new = g_string_append (new, ":gail");
          if (!found_atk_bridge)
            new = g_string_append (new, ":atk-bridge");

          g_setenv ("GTK_MODULES", new->str, TRUE);
          g_string_free (new, TRUE);
        }
    }
  else
    {
      g_setenv ("GTK_MODULES", "gail:atk-bridge", TRUE);
    }
}


#ifdef ENABLE_X11
static gboolean
xfsm_startup_at_spi_ior_set (void)
{
  Atom        AT_SPI_IOR;
  GdkDisplay *display;
  Atom        actual_type;
  gint        actual_format;
  guchar    *data = NULL;
  gulong     nitems;
  gulong     leftover;

  display = gdk_display_get_default ();
  AT_SPI_IOR = XInternAtom (GDK_DISPLAY_XDISPLAY (display), "AT_SPI_IOR", False);
  XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display),
                      XDefaultRootWindow (GDK_DISPLAY_XDISPLAY (display)),
                      AT_SPI_IOR, 0L,
                      (long) BUFSIZ, False,
                      (Atom) 31, &actual_type, &actual_format,
                      &nitems, &leftover, &data);

  if (data == NULL)
    return FALSE;
  XFree (data);

  return TRUE;
}
#endif


static void
xfsm_startup_at (XfsmManager *manager)
{
  gint n;

  /* start at-spi-dbus-bus and/or at-spi-registryd */
  n = xfsm_launch_desktop_files_on_login (TRUE);

  if (n > 0)
    {
      xfsm_startup_at_set_gtk_modules ();

#ifdef ENABLE_X11
      if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
        {
          /* wait for 2 seconds until the at-spi registered, not very nice
           * but required to properly start an accessible desktop */
          for (gint i = 0; i < 10; i++)
            {
              if (xfsm_startup_at_spi_ior_set ())
                break;
              xfsm_verbose ("Waiting for at-spi to register...\n");
              g_usleep (G_USEC_PER_SEC / 5);
            }
        }
#endif
    }
  else
    {
      g_warning ("No assistive technology service provider was started!");
    }
}


void
xfsm_startup_begin (XfsmManager *manager)
{
  /* start assistive technology before anything else */
  if (xfsm_manager_get_start_at (manager))
    xfsm_startup_at (manager);

  if (xfsm_manager_get_use_failsafe_mode (manager))
    xfsm_verbose ("Starting the session in failsafe mode.\n");

  xfsm_startup_session_continue (manager);
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
  GError          *error = NULL;

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

  if (!g_spawn_async (current_directory,
                      argv, NULL,
                      G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
                      NULL, NULL,
                      &pid, &error))
    {
      g_warning ("Unable to launch \"%s\": %s",
                 *argv, error->message);
      g_error_free (error);
      g_strfreev (argv);

      return FALSE;
    }

  /* Don't waste time if we're not debugging, but if we are print the
   * command + the arguments */
  if (xfsm_is_verbose_enabled ())
    {
      gchar *command = g_strjoinv (" ", argv);
      xfsm_verbose ("Launching command \"%s\" with PID %d\n", command, (gint) pid);
      g_free (command);
    }

  g_strfreev (argv);

  properties->pid = pid;

  /* set a watch to make sure the child doesn't quit before registering */
  child_watch_data = g_new0 (XfsmStartupData, 1);
  child_watch_data->manager = g_object_ref (manager);
  child_watch_data->properties = properties;
  child_watch_data->properties->child_watch_id =
      g_child_watch_add_full (G_PRIORITY_LOW, properties->pid,
                              xfsm_startup_child_watch, child_watch_data,
                              (GDestroyNotify) xfsm_startup_data_free);

  /* set a timeout -- client must register in a a certain amount of time
   * or it's assumed to be broken/have issues. */
  startup_timeout_data = g_new (XfsmStartupData, 1);
  startup_timeout_data->manager = g_object_ref (manager);
  startup_timeout_data->properties = properties;
  properties->startup_timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT,
                                                       WINDOWING_IS_X11 () ? STARTUP_TIMEOUT : STARTUP_TIMEOUT_WAYLAND,
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
      xfsm_verbose ("Nothing started and nothing to start, moving to autostart items\n");
      xfsm_startup_autostart (manager);
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

  xfsm_verbose ("Starting apps in prio group %d\n (%d)", cur_prio_group, g_queue_get_length (pending_properties));

  while ((properties = g_queue_pop_head (pending_properties)))
    {
      /* quit if we've hit all the clients in the current prio group */
      if (xfsm_properties_get_uchar (properties, GsmPriority, 50) != cur_prio_group)
        {
          /* we're not starting this one yet; put it back */
          g_queue_push_head (pending_properties, properties);
          break;
        }

      /* as clients cannot be uniquely identified in failsafe mode we at least count
         how many per priority group have registered */
      if (xfsm_manager_get_use_failsafe_mode (manager))
        xfsm_manager_increase_failsafe_pending_clients (manager);

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

  if (WINDOWING_IS_X11 ())
    xfsm_verbose ("Client Id = %s failed to register in time\n", stdata->properties->client_id);
  else
    /* no XfsmClient on Wayland, so just let handle_failed_startup() act as a cleanup func below */
    xfsm_verbose ("Client pid = %d seems to have started correctly\n", stdata->properties->pid);

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
