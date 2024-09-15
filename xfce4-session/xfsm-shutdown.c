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
#include "config.h"
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

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

#include "libxfsm/xfsm-util.h"

#include "xfsm-compat-gnome.h"
#include "xfsm-compat-kde.h"
#include "xfsm-fadeout.h"
#include "xfsm-global.h"
#include "xfsm-inhibition.h"
#include "xfsm-inhibitor.h"
#include "xfsm-legacy.h"
#include "xfsm-packagekit.h"
#include "xfsm-shutdown-fallback.h"
#include "xfsm-shutdown.h"



static void xfsm_shutdown_finalize  (GObject      *object);



struct _XfsmShutdownClass
{
  GObjectClass __parent__;
};


struct _XfsmShutdown
{
  GObject __parent__;

  XfceSystemd    *systemd;
  XfceConsolekit *consolekit;

  XfsmInhibitor   *inhibitions;
  XfceScreensaver *screensaver;
  XfsmPackagekit  *packagekit;

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
    shutdown->systemd = xfce_systemd_get ();
  else
    shutdown->consolekit = xfce_consolekit_get ();

  shutdown->inhibitions = xfsm_inhibitor_get ();
  shutdown->screensaver = xfce_screensaver_new ();
  shutdown->packagekit = xfsm_packagekit_get ();

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

  if (shutdown->inhibitions != NULL)
    g_object_unref (G_OBJECT (shutdown->inhibitions));

  g_object_unref (G_OBJECT (shutdown->screensaver));
  g_object_unref (G_OBJECT (shutdown->packagekit));

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

    case XFSM_SHUTDOWN_HYBRID_SLEEP:
      return xfsm_shutdown_try_hybrid_sleep (shutdown, error);

    case XFSM_SHUTDOWN_SWITCH_USER:
      return xfsm_shutdown_try_switch_user (shutdown, error);

    default:
      g_set_error (error, 1, 0, _("Unknown shutdown method %d"), type);
      break;
    }

  return FALSE;
}




gboolean
xfsm_shutdown_try_restart (XfsmShutdown  *shutdown,
                           GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!xfsm_shutdown_kiosk_can_shutdown (shutdown, error))
    return FALSE;

  if (shutdown->systemd != NULL)
    {
      gboolean has_updates = FALSE;
      if (xfsm_packagekit_has_update_prepared (shutdown->packagekit, &has_updates, NULL)
          && has_updates)
        {
          xfsm_packagekit_try_trigger_restart (shutdown->packagekit, NULL);
        }

      if (xfce_systemd_reboot (shutdown->systemd, TRUE, NULL))
        {
          return TRUE;
        }
    }
  else if (shutdown->consolekit != NULL)
    {
      if (xfce_consolekit_reboot (shutdown->consolekit, TRUE, NULL))
        {
          return TRUE;
        }
    }

  return xfsm_shutdown_fallback_try_action (XFSM_SHUTDOWN_RESTART, error);
}



gboolean
xfsm_shutdown_try_shutdown (XfsmShutdown  *shutdown,
                            GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!xfsm_shutdown_kiosk_can_shutdown (shutdown, error))
    return FALSE;

  if (shutdown->systemd != NULL)
    {
      gboolean has_updates = FALSE;
      if (xfsm_packagekit_has_update_prepared (shutdown->packagekit, &has_updates, NULL)
          && has_updates
          && xfsm_packagekit_try_trigger_shutdown (shutdown->packagekit, NULL))
        {
          // To actually trigger the offline update, we need to
          // reboot to do the upgrade. When the upgrade is complete,
          // the computer will shut down automatically.
          if (xfce_systemd_reboot (shutdown->systemd, TRUE, NULL))
            {
              return TRUE;
            }
        }

      if (xfce_systemd_power_off (shutdown->systemd, TRUE, NULL))
        {
          return TRUE;
        }
    }
  else if (shutdown->consolekit != NULL)
    {
      if (xfce_consolekit_power_off (shutdown->consolekit, TRUE, NULL))
        {
          return TRUE;
        }
    }

  return xfsm_shutdown_fallback_try_action (XFSM_SHUTDOWN_SHUTDOWN, error);
}



typedef gboolean (*SleepFunc) (gpointer object, gboolean polkit_interactive, GError **error);

static gboolean
try_sleep_method (gpointer  object,
                  SleepFunc func)
{
  if (object == NULL)
    return FALSE;

  return func (object, TRUE, NULL);
}



static gboolean
lock_screen (XfsmShutdown *shutdown,
             GError **error)
{
  XfconfChannel *channel;
  gboolean ret = TRUE;

  channel = xfconf_channel_get (SETTINGS_CHANNEL);
  if (xfconf_channel_get_bool (channel, "/shutdown/LockScreen", FALSE))
    ret = xfce_screensaver_lock (shutdown->screensaver);

  if (!ret)
    g_set_error (error, 1, 0, "Failed to lock the screen.");

  return ret;
}



gboolean
xfsm_shutdown_try_suspend (XfsmShutdown  *shutdown,
                           GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!lock_screen (shutdown, error))
    return FALSE;

  /* Try each way to suspend - it will handle NULL.
   */

  if (try_sleep_method (shutdown->systemd, (SleepFunc)xfce_systemd_suspend))
    return TRUE;

  if (try_sleep_method (shutdown->consolekit, (SleepFunc)xfce_consolekit_suspend))
    return TRUE;

  return xfsm_shutdown_fallback_try_action (XFSM_SHUTDOWN_SUSPEND, error);
}



gboolean
xfsm_shutdown_try_hibernate (XfsmShutdown  *shutdown,
                             GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!lock_screen (shutdown, error))
    return FALSE;

  /* Try each way to hibernate - it will handle NULL.
   */

  if (try_sleep_method (shutdown->systemd, (SleepFunc)xfce_systemd_hibernate))
    return TRUE;

  if (try_sleep_method (shutdown->consolekit, (SleepFunc)xfce_consolekit_hibernate))
    return TRUE;

  return xfsm_shutdown_fallback_try_action (XFSM_SHUTDOWN_HIBERNATE, error);
}



gboolean
xfsm_shutdown_try_hybrid_sleep (XfsmShutdown  *shutdown,
                                GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!lock_screen (shutdown, error))
    return FALSE;

  /* Try each way to hybrid-sleep - it will handle NULL.
   */

  if (try_sleep_method (shutdown->systemd, (SleepFunc)xfce_systemd_hybrid_sleep))
    return TRUE;

  if (try_sleep_method (shutdown->consolekit, (SleepFunc)xfce_consolekit_hybrid_sleep))
    return TRUE;

  return xfsm_shutdown_fallback_try_action (XFSM_SHUTDOWN_HYBRID_SLEEP, error);
}

gboolean
xfsm_shutdown_try_switch_user (XfsmShutdown  *shutdown,
                               GError       **error)
{
  GDBusProxy  *display_proxy;
  GVariant    *unused = NULL;
  const gchar *DBUS_NAME = "org.freedesktop.DisplayManager";
  const gchar *DBUS_INTERFACE = "org.freedesktop.DisplayManager.Seat";
  const gchar *DBUS_OBJECT_PATH = g_getenv ("XDG_SEAT_PATH");

  xfsm_verbose ("entering\n");

  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  display_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                 G_DBUS_PROXY_FLAGS_NONE,
                                                 NULL,
                                                 DBUS_NAME,
                                                 DBUS_OBJECT_PATH,
                                                 DBUS_INTERFACE,
                                                 NULL,
                                                 error);

  if (display_proxy == NULL || *error != NULL)
    {
      xfsm_verbose ("display proxy == NULL or an error was set\n");
      return FALSE;
    }

  xfsm_verbose ("calling SwitchToGreeter\n");
  unused = g_dbus_proxy_call_sync (display_proxy,
                                  "SwitchToGreeter",
                                  g_variant_new ("()"),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  5000,
                                  NULL,
                                  error);

  if (unused != NULL)
    {
      g_variant_unref (unused);
    }

  g_object_unref (display_proxy);

  return (*error == NULL);
}


void
xfsm_shutdown_can_restart (XfsmShutdown  *shutdown,
                           gboolean      *can_restart,
                           gboolean      *auth_restart)
{
  g_return_if_fail (XFSM_IS_SHUTDOWN (shutdown));

  if (xfsm_inhibitor_has_flags (shutdown->inhibitions, XFSM_INHIBITON_FLAG_LOGOUT))
    {
      *can_restart = FALSE;
      return;
    }

  if (!xfsm_shutdown_kiosk_can_shutdown (shutdown, NULL))
    {
      *can_restart = FALSE;
      return;
    }

  if (shutdown->systemd != NULL)
    {
      if (xfce_systemd_can_reboot (shutdown->systemd, can_restart, auth_restart, NULL))
        return;
    }
  else if (shutdown->consolekit != NULL)
    {
      if (xfce_consolekit_can_reboot (shutdown->consolekit, can_restart, auth_restart, NULL))
        return;
    }

  *can_restart = xfsm_shutdown_fallback_auth_restart ();
  if (auth_restart != NULL)
    *auth_restart = *can_restart;
}



void
xfsm_shutdown_can_shutdown (XfsmShutdown  *shutdown,
                            gboolean      *can_shutdown,
                            gboolean      *auth_shutdown)
{
  g_return_if_fail (XFSM_IS_SHUTDOWN (shutdown));

  if (xfsm_inhibitor_has_flags (shutdown->inhibitions, XFSM_INHIBITON_FLAG_LOGOUT))
    {
      *can_shutdown = FALSE;
      return;
    }

  if (!xfsm_shutdown_kiosk_can_shutdown (shutdown, NULL))
    {
      *can_shutdown = FALSE;
      return;
    }

  if (shutdown->systemd != NULL)
    {
      if (xfce_systemd_can_power_off (shutdown->systemd, can_shutdown, auth_shutdown, NULL))
        return;
    }
  else if (shutdown->consolekit != NULL)
    {
      if (xfce_consolekit_can_power_off (shutdown->consolekit, can_shutdown, auth_shutdown, NULL))
        return;
    }

  *can_shutdown = xfsm_shutdown_fallback_auth_shutdown ();
  if (auth_shutdown != NULL)
    *auth_shutdown = *can_shutdown;
}



void
xfsm_shutdown_can_suspend (XfsmShutdown  *shutdown,
                           gboolean      *can_suspend,
                           gboolean      *auth_suspend)
{
  g_return_if_fail (XFSM_IS_SHUTDOWN (shutdown));

  if (xfsm_inhibitor_has_flags (shutdown->inhibitions, XFSM_INHIBITON_FLAG_SUSPEND))
    {
      *can_suspend = FALSE;
      return;
    }

  if (!xfsm_shutdown_kiosk_can_shutdown (shutdown, NULL))
    {
      *can_suspend = FALSE;
      return;
    }

  if (shutdown->systemd != NULL)
    {
      if (xfce_systemd_can_suspend (shutdown->systemd, can_suspend, auth_suspend, NULL))
        {
          return;
        }
    }
  else if (shutdown->consolekit != NULL)
    {
      if (xfce_consolekit_can_suspend (shutdown->consolekit, can_suspend, auth_suspend, NULL))
        {
          return;
        }
    }

  *can_suspend = xfsm_shutdown_fallback_can_suspend ();
  *auth_suspend = xfsm_shutdown_fallback_auth_suspend ();
}



void
xfsm_shutdown_can_hibernate (XfsmShutdown  *shutdown,
                             gboolean      *can_hibernate,
                             gboolean      *auth_hibernate)
{
  g_return_if_fail (XFSM_IS_SHUTDOWN (shutdown));

  if (xfsm_inhibitor_has_flags (shutdown->inhibitions, XFSM_INHIBITON_FLAG_SUSPEND))
    {
      *can_hibernate = FALSE;
      return;
    }

  if (!xfsm_shutdown_kiosk_can_shutdown (shutdown, NULL))
    {
      *can_hibernate = FALSE;
      return;
    }

  if (shutdown->systemd != NULL)
    {
      if (xfce_systemd_can_hibernate (shutdown->systemd, can_hibernate, auth_hibernate, NULL))
        {
          return;
        }
    }
  else if (shutdown->consolekit != NULL)
    {
      if (xfce_consolekit_can_hibernate (shutdown->consolekit, can_hibernate, auth_hibernate, NULL))
        {
          return;
        }
    }

  *can_hibernate = xfsm_shutdown_fallback_can_hibernate ();
  *auth_hibernate = xfsm_shutdown_fallback_auth_hibernate ();
}



void
xfsm_shutdown_can_hybrid_sleep (XfsmShutdown  *shutdown,
                                gboolean      *can_hybrid_sleep,
                                gboolean      *auth_hybrid_sleep)
{
  g_return_if_fail (XFSM_IS_SHUTDOWN (shutdown));

  if (xfsm_inhibitor_has_flags (shutdown->inhibitions, XFSM_INHIBITON_FLAG_SUSPEND))
    {
      *can_hybrid_sleep = FALSE;
      return;
    }

  if (!xfsm_shutdown_kiosk_can_shutdown (shutdown, NULL))
    {
      *can_hybrid_sleep = FALSE;
      return;
    }

  if (shutdown->systemd != NULL)
    {
      if (xfce_systemd_can_hybrid_sleep (shutdown->systemd, can_hybrid_sleep, auth_hybrid_sleep, NULL))
        {
          return;
        }
    }
  else if (shutdown->consolekit != NULL)
    {
      if (xfce_consolekit_can_hybrid_sleep (shutdown->consolekit, can_hybrid_sleep, auth_hybrid_sleep, NULL))
        {
          return;
        }
    }

  *can_hybrid_sleep = xfsm_shutdown_fallback_can_hybrid_sleep ();
  *auth_hybrid_sleep = xfsm_shutdown_fallback_auth_hybrid_sleep ();
}



gboolean
xfsm_shutdown_can_switch_user (XfsmShutdown  *shutdown,
                               gboolean      *can_switch_user,
                               GError       **error)
{
  GDBusProxy  *display_proxy;
  gchar       *owner = NULL;
  const gchar *DBUS_NAME = "org.freedesktop.DisplayManager";
  const gchar *DBUS_INTERFACE = "org.freedesktop.DisplayManager.Seat";
  const gchar *DBUS_OBJECT_PATH = g_getenv ("XDG_SEAT_PATH");

  *can_switch_user = FALSE;

  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);
  if (xfsm_inhibitor_has_flags (shutdown->inhibitions, XFSM_INHIBITON_FLAG_SWITCH))
    {
      return TRUE;
    }

  if (DBUS_OBJECT_PATH == NULL)
    {
      return TRUE;
    }

  display_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                 G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                                 NULL,
                                                 DBUS_NAME,
                                                 DBUS_OBJECT_PATH,
                                                 DBUS_INTERFACE,
                                                 NULL,
                                                 error);

  if (display_proxy == NULL)
    {
      xfsm_verbose ("display proxy returned NULL\n");
      return FALSE;
    }

  /* is there anyone actually providing a service? */
  owner = g_dbus_proxy_get_name_owner (display_proxy);
  if (owner != NULL)
  {
    g_object_unref (display_proxy);
    g_free (owner);
    *can_switch_user = TRUE;
    return TRUE;
  }

  xfsm_verbose ("no owner NULL\n");
  return TRUE;
}



gboolean
xfsm_shutdown_can_save_session (XfsmShutdown *shutdown)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);
  return shutdown->kiosk_can_save_session;
}



gboolean
xfsm_shutdown_can_logout (XfsmShutdown  *shutdown)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);
  return !xfsm_inhibitor_has_flags (shutdown->inhibitions,
                                    XFSM_INHIBITON_FLAG_LOGOUT);
}



gboolean
xfsm_shutdown_has_update_prepared (XfsmShutdown *shutdown)
{
  gboolean  has_updates = FALSE;
  GError   *error = NULL;

  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  if (shutdown->systemd == NULL)
    return FALSE;

  if (!xfsm_packagekit_has_update_prepared (shutdown->packagekit, &has_updates, &error))
    {
      g_warning ("Querying PackageKit updates failed: %s", error->message);
      g_error_free (error);
    }

  return has_updates;
}
