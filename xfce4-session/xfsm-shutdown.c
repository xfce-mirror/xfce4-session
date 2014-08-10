/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2011      Nick Schermer <nick@xfce.org>
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
#include <config.h>
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
#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <libxfce4util/libxfce4util.h>
#include <gtk/gtk.h>
#ifdef HAVE_UPOWER
#include <upower.h>
#endif
#ifdef HAVE_POLKIT
#include <polkit/polkit.h>
#endif

#include <libxfsm/xfsm-util.h>

#include <xfce4-session/xfsm-shutdown.h>
#include <xfce4-session/xfsm-compat-gnome.h>
#include <xfce4-session/xfsm-compat-kde.h>
#include <xfce4-session/xfsm-consolekit.h>
#include <xfce4-session/xfsm-fadeout.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-legacy.h>
#include <xfce4-session/xfsm-upower.h>
#include <xfce4-session/xfsm-systemd.h>



#define POLKIT_AUTH_SHUTDOWN_XFSM  "org.xfce.session.xfsm-shutdown-helper"
#define POLKIT_AUTH_RESTART_XFSM   "org.xfce.session.xfsm-shutdown-helper"
#define POLKIT_AUTH_SUSPEND_XFSM   "org.xfce.session.xfsm-shutdown-helper"
#define POLKIT_AUTH_HIBERNATE_XFSM "org.xfce.session.xfsm-shutdown-helper"



static void xfsm_shutdown_finalize  (GObject      *object);
static gboolean xfsm_shutdown_fallback_auth_shutdown  (void);
static gboolean xfsm_shutdown_fallback_auth_restart   (void);
static gboolean xfsm_shutdown_fallback_can_hibernate  (void);
static gboolean xfsm_shutdown_fallback_can_suspend    (void);
static gboolean xfsm_shutdown_fallback_auth_hibernate (void);
static gboolean xfsm_shutdown_fallback_auth_suspend   (void);



struct _XfsmShutdownClass
{
  GObjectClass __parent__;
};


struct _XfsmShutdown
{
  GObject __parent__;

  XfsmSystemd    *systemd;
  XfsmConsolekit *consolekit;
  XfsmUPower     *upower;

  /* kiosk settings */
  gboolean        kiosk_can_shutdown;
  gboolean        kiosk_can_save_session;
};



G_DEFINE_TYPE (XfsmShutdown, xfsm_shutdown, G_TYPE_OBJECT)



static void
xfsm_shutdown_class_init (XfsmShutdownClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfsm_shutdown_finalize;
}



static void
xfsm_shutdown_init (XfsmShutdown *shutdown)
{
  XfceKiosk *kiosk;

  shutdown->consolekit = NULL;
  shutdown->systemd = NULL;
  if (LOGIND_RUNNING())
    shutdown->systemd = xfsm_systemd_get ();
  else
    shutdown->consolekit = xfsm_consolekit_get ();

  shutdown->upower = xfsm_upower_get ();

  /* check kiosk */
  kiosk = xfce_kiosk_new ("xfce4-session");
  shutdown->kiosk_can_shutdown = xfce_kiosk_query (kiosk, "Shutdown");
  shutdown->kiosk_can_save_session = xfce_kiosk_query (kiosk, "SaveSession");
  xfce_kiosk_free (kiosk);
}



static void
xfsm_shutdown_finalize (GObject *object)
{
  XfsmShutdown *shutdown = XFSM_SHUTDOWN (object);

  if (shutdown->systemd != NULL)
    g_object_unref (G_OBJECT (shutdown->systemd));

  if (shutdown->consolekit != NULL)
    g_object_unref (G_OBJECT (shutdown->consolekit));
  g_object_unref (G_OBJECT (shutdown->upower));

  (*G_OBJECT_CLASS (xfsm_shutdown_parent_class)->finalize) (object);
}



static gboolean
xfsm_shutdown_kiosk_can_shutdown (XfsmShutdown  *shutdown,
                                  GError       **error)
{
  if (!shutdown->kiosk_can_shutdown)
    {
      g_set_error_literal (error, 1, 0, _("Shutdown is blocked by the kiosk settings"));
      return FALSE;
    }

  return TRUE;
}



XfsmShutdown *
xfsm_shutdown_get (void)
{
  static XfsmShutdown *object = NULL;

  if (G_LIKELY (object != NULL))
    {
      g_object_ref (G_OBJECT (object));
    }
  else
    {
      object = g_object_new (XFSM_TYPE_SHUTDOWN, NULL);
      g_object_add_weak_pointer (G_OBJECT (object), (gpointer) &object);
    }

  return object;
}



gboolean
xfsm_shutdown_try_type (XfsmShutdown      *shutdown,
                        XfsmShutdownType   type,
                        GError           **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  switch (type)
    {
    case XFSM_SHUTDOWN_SHUTDOWN:
      return xfsm_shutdown_try_shutdown (shutdown, error);

    case XFSM_SHUTDOWN_RESTART:
      return xfsm_shutdown_try_restart (shutdown, error);

    case XFSM_SHUTDOWN_SUSPEND:
      return xfsm_shutdown_try_suspend (shutdown, error);

    case XFSM_SHUTDOWN_HIBERNATE:
      return xfsm_shutdown_try_hibernate (shutdown, error);

    default:
      g_set_error (error, 1, 0, _("Unknown shutdown method %d"), type);
      break;
    }

  return FALSE;
}



static gboolean
xfsm_shutdown_fallback_try_action (XfsmShutdown      *shutdown,
                                   XfsmShutdownType   type,
                                   GError           **error)
{
  const gchar *action;
  gboolean ret;
  gint exit_status = 0;
  gchar *command = NULL;

  if (type == XFSM_SHUTDOWN_SHUTDOWN)
    action = "shutdown";
  if (type == XFSM_SHUTDOWN_RESTART)
    action = "restart";
  else if (type == XFSM_SHUTDOWN_SUSPEND)
    action = "suspend";
  else if (type == XFSM_SHUTDOWN_HIBERNATE)
    action = "hibernate";
  else
      return FALSE;

  command = g_strdup_printf ("pkexec " XFSM_SHUTDOWN_HELPER_CMD " --%s", action);
  ret = g_spawn_command_line_sync (command, NULL, NULL, &exit_status, error);

  g_free (command);
  return ret;
}



gboolean
xfsm_shutdown_try_restart (XfsmShutdown  *shutdown,
                           GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!xfsm_shutdown_kiosk_can_shutdown (shutdown, error))
    return FALSE;

  if (shutdown->systemd != NULL)
    return xfsm_systemd_try_restart (shutdown->systemd, error);

  if (shutdown->consolekit != NULL)
    return xfsm_consolekit_try_restart (shutdown->consolekit, error);

  return xfsm_shutdown_fallback_try_action (shutdown, XFSM_SHUTDOWN_RESTART, error);
}



gboolean
xfsm_shutdown_try_shutdown (XfsmShutdown  *shutdown,
                            GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!xfsm_shutdown_kiosk_can_shutdown (shutdown, error))
    return FALSE;

  if (shutdown->systemd != NULL)
    return xfsm_systemd_try_shutdown (shutdown->systemd, error);

  if (shutdown->consolekit != NULL)
    return xfsm_consolekit_try_shutdown (shutdown->consolekit, error);

  return xfsm_shutdown_fallback_try_action (shutdown, XFSM_SHUTDOWN_SHUTDOWN, error);
}



gboolean
xfsm_shutdown_try_suspend (XfsmShutdown  *shutdown,
                           GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  if (shutdown->systemd != NULL)
    return xfsm_systemd_try_suspend (shutdown->systemd, error);

#ifdef HAVE_UPOWER
#if !UP_CHECK_VERSION(0, 99, 0)
  if (shutdown->upower != NULL)
    return xfsm_upower_try_suspend (shutdown->upower, error);
#endif /* UP_CHECK_VERSION */
#endif /* HAVE_UPOWER */

  xfsm_upower_lock_screen (shutdown->upower, "Suspend", error);
  return xfsm_shutdown_fallback_try_action (shutdown, XFSM_SHUTDOWN_SUSPEND, error);
}



gboolean
xfsm_shutdown_try_hibernate (XfsmShutdown  *shutdown,
                             GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  if (shutdown->systemd != NULL)
    return xfsm_systemd_try_hibernate (shutdown->systemd, error);

#ifdef HAVE_UPOWER
#if !UP_CHECK_VERSION(0, 99, 0)
  if (shutdown->upower != NULL)
    return xfsm_upower_try_hibernate (shutdown->upower, error);
#endif /* UP_CHECK_VERSION */
#endif /* HAVE_UPOWER */

  xfsm_upower_lock_screen (shutdown->upower, "Hibernate", error);
  return xfsm_shutdown_fallback_try_action (shutdown, XFSM_SHUTDOWN_HIBERNATE, error);
}



gboolean
xfsm_shutdown_can_restart (XfsmShutdown  *shutdown,
                           gboolean      *can_restart,
                           GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!xfsm_shutdown_kiosk_can_shutdown (shutdown, NULL))
    {
      *can_restart = FALSE;
      return TRUE;
    }

  if (shutdown->systemd != NULL)
    {
      if (xfsm_systemd_can_restart (shutdown->systemd, can_restart, error))
        return TRUE;
    }
  else if (shutdown->consolekit != NULL)
    {
      if (xfsm_consolekit_can_restart (shutdown->consolekit, can_restart, error))
        return TRUE;
    }

  *can_restart = xfsm_shutdown_fallback_auth_restart ();
  return TRUE;
}



gboolean
xfsm_shutdown_can_shutdown (XfsmShutdown  *shutdown,
                            gboolean      *can_shutdown,
                            GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!xfsm_shutdown_kiosk_can_shutdown (shutdown, NULL))
    {
      *can_shutdown = FALSE;
      return TRUE;
    }

  if (shutdown->systemd != NULL)
    {
      if (xfsm_systemd_can_shutdown (shutdown->systemd, can_shutdown, error))
        return TRUE;
    }

  if (shutdown->consolekit != NULL)
    {
      if (xfsm_consolekit_can_shutdown (shutdown->consolekit, can_shutdown, error))
        return TRUE;
    }

  *can_shutdown = xfsm_shutdown_fallback_auth_shutdown ();
  return TRUE;
}



gboolean
xfsm_shutdown_can_suspend (XfsmShutdown  *shutdown,
                           gboolean      *can_suspend,
                           gboolean      *auth_suspend,
                           GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!xfsm_shutdown_kiosk_can_shutdown (shutdown, NULL))
    {
      *can_suspend = FALSE;
      return TRUE;
    }

  if (shutdown->systemd != NULL)
    return xfsm_systemd_can_suspend (shutdown->systemd, can_suspend,
                                     auth_suspend, error);

#ifdef HAVE_UPOWER
#if !UP_CHECK_VERSION(0, 99, 0)
  if (shutdown->upower)
    return xfsm_upower_can_suspend (shutdown->upower, can_suspend,
                                    auth_suspend, error);
#endif /* UP_CHECK_VERSION */
#endif /* HAVE_UPOWER */

  *can_suspend = xfsm_shutdown_fallback_can_suspend ();
  *auth_suspend = xfsm_shutdown_fallback_auth_suspend ();
  return TRUE;
}



gboolean
xfsm_shutdown_can_hibernate (XfsmShutdown  *shutdown,
                             gboolean      *can_hibernate,
                             gboolean      *auth_hibernate,
                             GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!xfsm_shutdown_kiosk_can_shutdown (shutdown, NULL))
    {
      *can_hibernate = FALSE;
      return TRUE;
    }

  if (shutdown->systemd != NULL)
    return xfsm_systemd_can_hibernate (shutdown->systemd, can_hibernate,
                                       auth_hibernate, error);

#ifdef HAVE_UPOWER
#if !UP_CHECK_VERSION(0, 99, 0)
  if (shutdown->upower != NULL)
    return xfsm_upower_can_hibernate (shutdown->upower, can_hibernate,
                                      auth_hibernate, error);
#endif /* UP_CHECK_VERSION */
#endif /* HAVE_UPOWER */

  *can_hibernate = xfsm_shutdown_fallback_can_hibernate ();
  *auth_hibernate = xfsm_shutdown_fallback_auth_hibernate ();
  return TRUE;
}



gboolean
xfsm_shutdown_can_save_session (XfsmShutdown *shutdown)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);
  return shutdown->kiosk_can_save_session;
}



#ifdef BACKEND_TYPE_FREEBSD
static gchar *
get_string_sysctl (GError **err, const gchar *format, ...)
{
        va_list args;
        gchar *name;
        size_t value_len;
        gchar *str = NULL;

        g_return_val_if_fail(format != NULL, FALSE);

        va_start (args, format);
        name = g_strdup_vprintf (format, args);
        va_end (args);

        if (sysctlbyname (name, NULL, &value_len, NULL, 0) == 0) {
                str = g_new (char, value_len + 1);
                if (sysctlbyname (name, str, &value_len, NULL, 0) == 0)
                        str[value_len] = 0;
                else {
                        g_free (str);
                        str = NULL;
                }
        }

        if (!str)
                g_set_error (err, 0, 0, "%s", g_strerror(errno));

        g_free(name);
        return str;
}



static gboolean
freebsd_supports_sleep_state (const gchar *state)
{
  gboolean ret = FALSE;
  gchar *sleep_states;

  sleep_states = get_string_sysctl (NULL, "hw.acpi.supported_sleep_state");
  if (sleep_states != NULL)
    {
      if (strstr (sleep_states, state) != NULL)
          ret = TRUE;
    }

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
  ret = (WIFEXITED(exit_status) && (WEXITSTATUS(exit_status) == EXIT_SUCCESS));

out:
  g_free (command);

  return ret;
}
#endif /* BACKEND_TYPE_LINUX */



static gboolean
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

  return FALSE;
}



static gboolean
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

  return FALSE;
}



static gboolean
xfsm_shutdown_fallback_check_auth (const gchar *action_id)
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



static gboolean
xfsm_shutdown_fallback_auth_shutdown (void)
{
  return xfsm_shutdown_fallback_check_auth (POLKIT_AUTH_SHUTDOWN_XFSM);
}



static gboolean
xfsm_shutdown_fallback_auth_restart (void)
{
  return xfsm_shutdown_fallback_check_auth (POLKIT_AUTH_RESTART_XFSM);
}



static gboolean
xfsm_shutdown_fallback_auth_suspend (void)
{
  return xfsm_shutdown_fallback_check_auth (POLKIT_AUTH_SUSPEND_XFSM);
}



static gboolean
xfsm_shutdown_fallback_auth_hibernate (void)
{
  return xfsm_shutdown_fallback_check_auth (POLKIT_AUTH_HIBERNATE_XFSM);
}
