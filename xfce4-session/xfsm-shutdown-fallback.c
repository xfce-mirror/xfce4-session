/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2011      Nick Schermer <nick@xfce.org>
 * Copyright (c) 2014      Xfce Development Team <xfce4-dev@xfce.org>
 * Copyright (c) 2018      Ali Abdallah <ali@xfce.org>
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
 *
 * Parts of this file where taken from gnome-session/logout.c, which
 * was written by Owen Taylor <otaylor@redhat.com>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define __BACKEND_TYPE_BSD__ 1
#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef HAVE_POLKIT
#include <polkit/polkit.h>
#endif

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <grp.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <pwd.h>

#include "libxfsm/xfsm-shutdown-common.h"
#include "libxfsm/xfsm-util.h"

#include "xfsm-shutdown-fallback.h"

#define POLKIT_AUTH_SHUTDOWN_XFSM "org.xfce.session.xfsm-shutdown-helper"
#define POLKIT_AUTH_RESTART_XFSM "org.xfce.session.xfsm-shutdown-helper"
#define POLKIT_AUTH_SUSPEND_XFSM "org.xfce.session.xfsm-shutdown-helper"
#define POLKIT_AUTH_HIBERNATE_XFSM "org.xfce.session.xfsm-shutdown-helper"
#define POLKIT_AUTH_HYBRID_SLEEP_XFSM "org.xfce.session.xfsm-shutdown-helper"



#ifdef BACKEND_TYPE_FREEBSD
static gchar *
get_string_sysctl (GError **err, const gchar *format, ...)
{
  va_list args;
  gchar *name;
  size_t value_len;
  gchar *str = NULL;

  g_return_val_if_fail (format != NULL, FALSE);

  va_start (args, format);
  name = g_strdup_vprintf (format, args);
  va_end (args);

  if (sysctlbyname (name, NULL, &value_len, NULL, 0) == 0)
    {
      str = g_new (char, value_len + 1);
      if (sysctlbyname (name, str, &value_len, NULL, 0) == 0)
        str[value_len] = 0;
      else
        {
          g_free (str);
          str = NULL;
        }
    }

  if (!str)
    g_set_error (err, 0, 0, "%s", g_strerror (errno));

  g_free (name);
  return str;
}



static gboolean
freebsd_supports_sleep_state (const gchar *state)
{
  gboolean ret = FALSE;
  gchar *sleep_states;

#if defined(__FreeBSD__)
  gboolean status;
  gint v;
  size_t value_len = sizeof (int);
#endif

  sleep_states = get_string_sysctl (NULL, "hw.acpi.supported_sleep_state");
  if (sleep_states != NULL)
    {
      if (strstr (sleep_states, state) != NULL)
        ret = TRUE;
    }

#if defined(__FreeBSD__)
  /* On FreeBSD, S4 is not supported unless S4BIOS is available.
   * If S4 will ever be implemented on FreeBSD, we can disable S4bios
   * check below, using #if __FreeBSD_version >= XXXXXX.
   **/
  if (g_strcmp0 (state, "S4") == 0)
    {
      status = sysctlbyname ("hw.acpi.s4bios", &v, &value_len, NULL, 0) == 0;
      if (G_UNLIKELY (!status))
        {
          g_warning ("sysctl failed on 'hw.acpi.s4bios'");
        }
      else
        {
          /* No S4Bios support */
          if (v == 0)
            ret = FALSE;
        }
    }
#endif /* __FreeBSD__ */

  g_free (sleep_states);

  return ret;
}
#endif /* BACKEND_TYPE_FREEBSD */



#ifdef BACKEND_TYPE_LINUX
static gboolean
linux_supports_sleep_state (const gchar *state)
{
  gboolean ret = FALSE;
  gchar *command;
  GError *error = NULL;
  gint exit_status;

  /* run script from pm-utils */
  command = g_strdup_printf ("/usr/bin/pm-is-supported --%s", state);

  ret = g_spawn_command_line_sync (command, NULL, NULL, &exit_status, &error);
  if (!ret)
    {
      g_warning ("failed to run script: %s", error->message);
      g_error_free (error);
      goto out;
    }
  ret = (WIFEXITED (exit_status) && (WEXITSTATUS (exit_status) == EXIT_SUCCESS));

out:
  g_free (command);

  return ret;
}
#endif /* BACKEND_TYPE_LINUX */



#ifdef __BACKEND_TYPE_BSD__
static gboolean
xfsm_shutdown_fallback_user_is_operator (void)
{
  struct passwd *pw;
  int max_grp = sysconf (_SC_NGROUPS_MAX);
  gid_t *groups;
  int i = 0;
  int ret;
  static gboolean is_operator = FALSE;
  static gboolean once = FALSE;

  /* Only check once */
  if (once == TRUE)
    goto out;

  if (max_grp == -1)
    {
      fprintf (stderr, "sysconf(_SC_NGROUPS_MAX) failed");
      return FALSE;
    }

  groups = g_new (gid_t, max_grp);
  pw = getpwuid (getuid ());

  ret = getgrouplist (pw->pw_name, pw->pw_gid, groups, &max_grp);

  if (ret < 0)
    {
      fprintf (stderr,
               "Failed to get user group list, user belongs to more than %u groups?\n",
               max_grp);
      g_free (groups);
      goto out;
    }

  once = TRUE;

  for (i = 0; i < max_grp; i++)
    {
      struct group *gr;

      gr = getgrgid (groups[i]);
      if (gr != NULL && strncmp (gr->gr_name, "operator", 8) == 0)
        {
          is_operator = TRUE;
          break;
        }
    }
  g_free (groups);
out:
  return is_operator;
}
#endif /* __BACKEND_TYPE_BSD__ */



#ifdef __BACKEND_TYPE_BSD__
static gboolean
xfsm_shutdown_fallback_bsd_check_auth (XfsmShutdownType shutdown_type)
{
  gboolean auth_result = FALSE;
  switch (shutdown_type)
    {
    /* On the BSDs users of the operator group are allowed to shutdown/restart the system */
    case XFSM_SHUTDOWN_SHUTDOWN:
    case XFSM_SHUTDOWN_RESTART:
      auth_result = xfsm_shutdown_fallback_user_is_operator ();
      break;
    case XFSM_SHUTDOWN_SUSPEND:
    case XFSM_SHUTDOWN_HIBERNATE:
    case XFSM_SHUTDOWN_HYBRID_SLEEP:
      /* Check rw access on '/var/run/apmdev' on OpenBSD and to /dev/acpi'
       * for the other BSDs */
      auth_result = g_access (BSD_SLEEP_ACCESS_NODE, R_OK | W_OK) == 0;
      break;
    default:
      g_warning ("Unexpected shutdow id '%d'\n", shutdown_type);
      break;
    }

  return auth_result;
}
#endif /* __BACKEND_TYPE_BSD__ */



static gboolean
xfsm_shutdown_fallback_check_auth_polkit (const gchar *action_id)
{
  gboolean auth_result = FALSE;
#ifdef HAVE_POLKIT
  GDBusConnection *bus;
  PolkitAuthority *polkit;
  PolkitAuthorizationResult *polkit_result;
  PolkitSubject *polkit_subject;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
  polkit = polkit_authority_get_sync (NULL, NULL);
  if (polkit != NULL && bus != NULL)
    {
      polkit_subject = polkit_system_bus_name_new (g_dbus_connection_get_unique_name (bus));
      polkit_result = polkit_authority_check_authorization_sync (polkit,
                                                                 polkit_subject,
                                                                 action_id,
                                                                 NULL,
                                                                 POLKIT_CHECK_AUTHORIZATION_FLAGS_NONE,
                                                                 NULL,
                                                                 NULL);
      if (polkit_result != NULL)
        {
          auth_result = polkit_authorization_result_get_is_authorized (polkit_result);
          g_object_unref (polkit_result);
        }

      g_object_unref (polkit);
      g_object_unref (bus);
    }
#endif /* HAVE_POLKIT */

  return auth_result;
}



/**
 * xfsm_shutdown_fallback_try_action:
 * @type:  The @XfsmShutdownType action to perform (shutdown, suspend, etc).
 * @error: Returns a @GError if an error was encountered.
 *
 * Performs the @XfsmShutdownType action requested.
 *
 * Return value: Returns FALSE if an invalid @XfsmShutdownType action was
 *               requested. Otherwise returns the status of the pkexec
 *               call to the helper command.
 **/
gboolean
xfsm_shutdown_fallback_try_action (XfsmShutdownType type,
                                   GError **error)
{
  const gchar *xfsm_helper_action;
  const gchar *cmd __attribute__ ((unused));
  gboolean ret = FALSE;
#ifdef HAVE_POLKIT
  gchar *command = NULL;
#endif

  switch (type)
    {
    case XFSM_SHUTDOWN_SHUTDOWN:
      xfsm_helper_action = "shutdown";
      cmd = POWEROFF_CMD;
      break;
    case XFSM_SHUTDOWN_RESTART:
      xfsm_helper_action = "restart";
      cmd = REBOOT_CMD;
      break;
    case XFSM_SHUTDOWN_SUSPEND:
      xfsm_helper_action = "suspend";
      cmd = UP_BACKEND_SUSPEND_COMMAND;
      break;
    case XFSM_SHUTDOWN_HIBERNATE:
      xfsm_helper_action = "hibernate";
      cmd = UP_BACKEND_HIBERNATE_COMMAND;
      break;
    case XFSM_SHUTDOWN_HYBRID_SLEEP:
      xfsm_helper_action = "hybrid-sleep";
      cmd = UP_BACKEND_HIBERNATE_COMMAND;
      break;
    default:
      g_set_error (error, 1, 0, "Unknown shutdown type %d", type);
      return FALSE;
    }

#ifdef __BACKEND_TYPE_BSD__
  /* Make sure we can use native shutdown commands */
  if (xfsm_shutdown_fallback_bsd_check_auth (type))
    {
      return g_spawn_command_line_sync (cmd, NULL, NULL, NULL, error);
    }
#endif

    /* xfsm-shutdown-helper requires polkit to run */
#ifdef HAVE_POLKIT
  command = g_strdup_printf ("pkexec " XFSM_SHUTDOWN_HELPER_CMD " --%s", xfsm_helper_action);

  ret = g_spawn_command_line_sync (command, NULL, NULL, NULL, error);

  g_free (command);
#endif
  if (!ret)
    {
      g_set_error (error, 1, 0, "Failed to %s (%s)", xfsm_helper_action, cmd);
    }
  return ret;
}



/**
 * xfsm_shutdown_fallback_can_suspend:
 *
 * Return value: Returns whether the *system* is capable suspending.
 **/
gboolean
xfsm_shutdown_fallback_can_suspend (void)
{
#ifdef BACKEND_TYPE_FREEBSD
  return freebsd_supports_sleep_state ("S3");
#endif
#ifdef BACKEND_TYPE_LINUX
  return linux_supports_sleep_state ("suspend");
#endif
#ifdef BACKEND_TYPE_OPENBSD
  return TRUE;
#endif
#ifdef BACKEND_TYPE_SOLARIS
  return TRUE;
#endif

  return FALSE;
}



/**
 * xfsm_shutdown_fallback_can_hibernate:
 *
 * Return value: Returns whether the *system* is capable hibernating.
 **/
gboolean
xfsm_shutdown_fallback_can_hibernate (void)
{
#ifdef BACKEND_TYPE_FREEBSD
  return freebsd_supports_sleep_state ("S4");
#endif
#ifdef BACKEND_TYPE_LINUX
  return linux_supports_sleep_state ("hibernate");
#endif
#ifdef BACKEND_TYPE_OPENBSD
  return TRUE;
#endif
#ifdef BACKEND_TYPE_SOLARIS
  return FALSE;
#endif

  return FALSE;
}



/**
 * xfsm_shutdown_fallback_can_hybrid_sleep:
 *
 * Return value: Returns whether the *system* is capable of hybrid sleep.
 **/
gboolean
xfsm_shutdown_fallback_can_hybrid_sleep (void)
{
#ifdef BACKEND_TYPE_FREEBSD
  return freebsd_supports_sleep_state ("S4");
#endif
#ifdef BACKEND_TYPE_LINUX
  return linux_supports_sleep_state ("hibernate");
#endif
#ifdef BACKEND_TYPE_OPENBSD
  return FALSE;
#endif
#ifdef BACKEND_TYPE_SOLARIS
  return FALSE;
#endif

  return FALSE;
}



/**
 * xfsm_shutdown_fallback_auth_shutdown:
 *
 * Return value: Returns whether the user is authorized to perform a shutdown.
 **/
gboolean
xfsm_shutdown_fallback_auth_shutdown (void)
{
#ifdef __BACKEND_TYPE_BSD__
  gboolean ret_val = xfsm_shutdown_fallback_bsd_check_auth (XFSM_SHUTDOWN_SHUTDOWN);
  if (ret_val)
    return TRUE;
#endif
  return xfsm_shutdown_fallback_check_auth_polkit (POLKIT_AUTH_SHUTDOWN_XFSM);
}



/**
 * xfsm_shutdown_fallback_auth_restart:
 *
 * Return value: Returns whether the user is authorized to perform a restart.
 **/
gboolean
xfsm_shutdown_fallback_auth_restart (void)
{
#ifdef __BACKEND_TYPE_BSD__
  gboolean ret_val = xfsm_shutdown_fallback_bsd_check_auth (XFSM_SHUTDOWN_RESTART);
  if (ret_val)
    return TRUE;
#endif
  return xfsm_shutdown_fallback_check_auth_polkit (POLKIT_AUTH_RESTART_XFSM);
}



/**
 * xfsm_shutdown_fallback_auth_suspend:
 *
 * Return value: Returns whether the user is authorized to perform a suspend.
 **/
gboolean
xfsm_shutdown_fallback_auth_suspend (void)
{
#ifdef __BACKEND_TYPE_BSD__
  gboolean ret_val = xfsm_shutdown_fallback_bsd_check_auth (XFSM_SHUTDOWN_SUSPEND);
  if (ret_val)
    return TRUE;
#endif
  return xfsm_shutdown_fallback_check_auth_polkit (POLKIT_AUTH_SUSPEND_XFSM);
}



/**
 * xfsm_shutdown_fallback_auth_hibernate:
 *
 * Return value: Returns whether the user is authorized to perform a hibernate.
 **/
gboolean
xfsm_shutdown_fallback_auth_hibernate (void)
{
#ifdef __BACKEND_TYPE_BSD__
  gboolean ret_val = xfsm_shutdown_fallback_bsd_check_auth (XFSM_SHUTDOWN_HIBERNATE);
  if (ret_val)
    return TRUE;
#endif
  return xfsm_shutdown_fallback_check_auth_polkit (POLKIT_AUTH_HIBERNATE_XFSM);
}



/**
 * xfsm_shutdown_fallback_auth_hybrid_sleep:
 *
 * Return value: Returns whether the user is authorized to perform a hybrid sleep.
 **/
gboolean
xfsm_shutdown_fallback_auth_hybrid_sleep (void)
{
#ifdef __BACKEND_TYPE_BSD__
  gboolean ret_val = xfsm_shutdown_fallback_bsd_check_auth (XFSM_SHUTDOWN_HYBRID_SLEEP);
  if (ret_val)
    return TRUE;
#endif
  return xfsm_shutdown_fallback_check_auth_polkit (POLKIT_AUTH_HYBRID_SLEEP_XFSM);
}
