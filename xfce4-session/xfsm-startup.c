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
#include <gdk/gdkx.h>
#include <libxfce4ui/libxfce4ui.h>

#include <libxfsm/xfsm-util.h>

#include <xfce4-session/xfsm-compat-gnome.h>
#include <xfce4-session/xfsm-compat-kde.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-manager.h>
#include <xfce4-session/xfsm-splash-screen.h>

#include <xfce4-session/xfsm-startup.h>


typedef struct
{
  XfsmManager    *manager;
  XfsmProperties *properties;
} XfsmStartupData;

static void     xfsm_startup_failsafe                (XfsmManager *manager);

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
                         const gchar *agent)
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
          if (pid <= 0)
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

  if (pid <= 0)
    g_warning ("%s returned no PID in the variables", agent);

  return pid;
}



void
xfsm_startup_init (XfconfChannel *channel)
{
  gchar       *ssh_agent;
  gchar       *ssh_agent_path = NULL;
  gchar       *gpg_agent_path = NULL;
  gchar       *cmd;
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
          g_print ("xfce4-session: %s\n",
                   "GNOME compatibility is enabled and gnome-keyring-daemon is "
                   "found on the system. Skipping gpg/ssh-agent startup.");
          return;
        }
    }

  if (xfconf_channel_get_bool (channel, "/startup/gpg-agent/enabled", TRUE))
    {
      gpg_agent_path = g_find_program_in_path ("gpg-agent");
      if (gpg_agent_path == NULL)
        g_printerr ("xfce4-session: %s\n",
                    "No GPG agent found");
    }

  if (xfconf_channel_get_bool (channel, "/startup/ssh-agent/enabled", TRUE))
    {
      ssh_agent = xfconf_channel_get_string (channel, "/startup/ssh-agent/type", NULL);

      if (ssh_agent == NULL
          || g_strcmp0 (ssh_agent, "ssh-agent") == 0)
        {
          ssh_agent_path = g_find_program_in_path ("ssh-agent");
          if (ssh_agent_path == NULL)
            g_printerr ("xfce4-session: %s\n",
                        "No SSH authentication agent found");
        }
      else if (g_strcmp0 (ssh_agent, "gpg-agent") == 0)
        {
          if (gpg_agent_path != NULL)
             gpgagent_ssh_enabled = TRUE;
           else
               g_printerr ("xfce4-session: %s\n", "gpg-agent is configured as SSH agent, "
                          "but gpg-agent is disabled or not found");
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
          running_sshagent = xfsm_startup_init_agent (cmd, "ssh-agent");
          g_free (cmd);
          g_free (ssh_agent_path);
        }
    }

  if (G_LIKELY (gpg_agent_path != NULL))
    {
      agentpid = xfsm_gpg_agent_pid (g_getenv ("GPG_AGENT_INFO"));

      /* check if the pid is still responding (ie not stale) */
      if (agentpid > 0 && kill (agentpid, 0) == 0)
        {
          g_message ("GPG agent is already running");
        }
      else
        {
          gchar *envfile;

          g_unsetenv ("GPG_AGENT_INFO");

          envfile = xfce_resource_save_location (XFCE_RESOURCE_CACHE, "gpg-agent-info", FALSE);

          if (gpgagent_ssh_enabled)
            {
              cmd = g_strdup_printf ("%s --sh --daemon --enable-ssh-support "
                                     "--write-env-file '%s'", gpg_agent_path, envfile);
            }
          else
            {
              cmd = g_strdup_printf ("%s --sh --daemon --write-env-file '%s'", gpg_agent_path, envfile);
            }

          /* keep this around for shutdown */
          running_gpgagent = xfsm_startup_init_agent (cmd, "gpg-agent");

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
      gchar *envfile;
      if (kill (running_gpgagent, SIGINT) == 0)
        {
         /* make sure the env values are unset */
         g_unsetenv ("GPG_AGENT_INFO");
         if (gpgagent_ssh_enabled)
           {
            g_unsetenv ("SSH_AGENT_PID");
            g_unsetenv ("SSH_AUTH_SOCK");
           }
        }
      else
        {
          g_warning ("Failed to kill gpg-agent with pid %d", running_gpgagent);
        }

      /* drop the info file from gpg-agent */
      envfile = xfce_resource_lookup (XFCE_RESOURCE_CACHE, "gpg-agent-info");
      if (G_LIKELY (envfile != NULL))
        g_unlink (envfile);
      g_free (envfile);
    }
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
xfsm_startup_autostart_xdg (XfsmManager *manager,
                            gboolean     start_at_spi)
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
  gchar       *filename;
  const gchar *pattern;

  /* migrate the old autostart location (if still present) */
  xfsm_startup_autostart_migrate ();

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



static void
xfsm_startup_autostart (XfsmManager *manager)
{
  gint n;

  n = xfsm_startup_autostart_xdg (manager, FALSE);

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


static void
xfsm_startup_at (XfsmManager *manager)
{
  gint n, i;

  /* start at-spi-dbus-bus and/or at-spi-registryd */
  n = xfsm_startup_autostart_xdg (manager, TRUE);

  if (n > 0)
    {
      if (G_LIKELY (splash_screen != NULL))
        xfsm_splash_screen_next (splash_screen, _("Starting Assistive Technologies"));

      xfsm_startup_at_set_gtk_modules ();

      /* wait for 2 seconds until the at-spi registered, not very nice
       * but required to properly start an accessible desktop */
      for (i = 0; i < 10; i++)
        {
          if (xfsm_startup_at_spi_ior_set ())
            break;
          xfsm_verbose ("Waiting for at-spi to register...\n");
          g_usleep (G_USEC_PER_SEC / 5);
        }
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
    {
      xfsm_startup_failsafe (manager);
      xfsm_startup_autostart (manager);
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
      xfsm_verbose ("Launched command \"%s\" with PID %d\n", command, (gint) pid);
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
          xfsm_verbose ("client id %s started\n", properties->client_id);
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

  xfsm_verbose ("Client Id = %s failed to start\n", properties->client_id);

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


