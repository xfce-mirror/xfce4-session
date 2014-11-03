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
#include <xfce4-session/xfsm-shutdown-fallback.h>



static void xfsm_shutdown_finalize  (GObject      *object);



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
  shutdown->upower = NULL;
  if (LOGIND_RUNNING())
    shutdown->systemd = xfsm_systemd_get ();
  else
    shutdown->consolekit = xfsm_consolekit_get ();

#ifdef HAVE_UPOWER
#if !UP_CHECK_VERSION(0, 99, 0)
  shutdown->upower = xfsm_upower_get ();
#endif /* UP_CHECK_VERSION */
#endif /* HAVE_UPOWER */

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




gboolean
xfsm_shutdown_try_restart (XfsmShutdown  *shutdown,
                           GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!xfsm_shutdown_kiosk_can_shutdown (shutdown, error))
    return FALSE;

  if (shutdown->systemd != NULL)
    {
      if (xfsm_systemd_try_restart (shutdown->systemd, NULL))
        {
          return TRUE;
        }
    }
  else if (shutdown->consolekit != NULL)
    {
      if (xfsm_consolekit_try_restart (shutdown->consolekit, NULL))
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
      if (xfsm_systemd_try_shutdown (shutdown->systemd, NULL))
        {
          return TRUE;
        }
    }
  else if (shutdown->consolekit != NULL)
    {
      if (xfsm_consolekit_try_shutdown (shutdown->consolekit, NULL))
        {
          return TRUE;
        }
    }

  return xfsm_shutdown_fallback_try_action (XFSM_SHUTDOWN_SHUTDOWN, error);
}



typedef gboolean (*SleepFunc) (gpointer object, GError **);

static gboolean
try_sleep_method (gpointer  object,
                  SleepFunc func)
{
  if (object == NULL)
    return FALSE;

  return func (object, NULL);
}



gboolean
xfsm_shutdown_try_suspend (XfsmShutdown  *shutdown,
                           GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  /* Try each way to suspend - it will handle NULL.
   * In the future the upower code can go away once everyone is
   * running upower 0.99.0+
   */

  if (try_sleep_method (shutdown->systemd, (SleepFunc)xfsm_systemd_try_suspend))
    return TRUE;

  if (try_sleep_method (shutdown->upower, (SleepFunc)xfsm_upower_try_suspend))
    return TRUE;

  if (try_sleep_method (shutdown->consolekit, (SleepFunc)xfsm_consolekit_try_suspend))
    return TRUE;

  return xfsm_shutdown_fallback_try_action (XFSM_SHUTDOWN_SUSPEND, error);
}



gboolean
xfsm_shutdown_try_hibernate (XfsmShutdown  *shutdown,
                             GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  /* Try each way to hibernate - it will handle NULL.
   * In the future the upower code can go away once everyone is
   * running upower 0.99.0+
   */

  if (try_sleep_method (shutdown->systemd, (SleepFunc)xfsm_systemd_try_hibernate))
    return TRUE;

  if (try_sleep_method (shutdown->upower, (SleepFunc)xfsm_upower_try_hibernate))
    return TRUE;

  if (try_sleep_method (shutdown->consolekit, (SleepFunc)xfsm_consolekit_try_hibernate))
    return TRUE;

  return xfsm_shutdown_fallback_try_action (XFSM_SHUTDOWN_HIBERNATE, error);
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
  else if (shutdown->consolekit != NULL)
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
    {
      if (xfsm_systemd_can_suspend (shutdown->systemd, can_suspend, auth_suspend, NULL))
        {
          return TRUE;
        }
    }
  else if (shutdown->upower != NULL)
    {
      if (xfsm_upower_can_suspend (shutdown->upower, can_suspend, auth_suspend, NULL))
        {
          return TRUE;
        }
    }
  else if (shutdown->consolekit != NULL)
    {
      if (xfsm_consolekit_can_suspend (shutdown->consolekit, can_suspend, auth_suspend, NULL))
        {
          return TRUE;
        }
    }

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
    {
      if (xfsm_systemd_can_hibernate (shutdown->systemd, can_hibernate, auth_hibernate, NULL))
        {
          return TRUE;
        }
    }
  else if (shutdown->upower != NULL)
    {
      if (xfsm_upower_can_hibernate (shutdown->upower, can_hibernate, auth_hibernate, NULL))
        {
          return TRUE;
        }
    }
  else if (shutdown->consolekit != NULL)
    {
      if (xfsm_consolekit_can_hibernate (shutdown->consolekit, can_hibernate, auth_hibernate, NULL))
        {
          return TRUE;
        }
    }

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
