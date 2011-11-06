/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <gtk/gtk.h>

#include <libxfsm/xfsm-util.h>

#include <xfce4-session/xfsm-shutdown.h>
#include <xfce4-session/xfsm-compat-gnome.h>
#include <xfce4-session/xfsm-compat-kde.h>
#include <xfce4-session/xfsm-fadeout.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-legacy.h>
#include <xfce4-session/xfsm-shutdown-helper.h>

gint
xfsm_shutdown(XfsmShutdownType type)
{
  gboolean            result;
  GError             *error = NULL;
  XfsmShutdownHelper *shutdown_helper;

  /* kludge */
  if (type == XFSM_SHUTDOWN_ASK)
    {
      g_warning ("xfsm_shutdown () passed XFSM_SHUTDOWN_ASK.  This is a bug.");
      type = XFSM_SHUTDOWN_LOGOUT;
    }

  /* these two remember if they were started or not */
  xfsm_compat_gnome_shutdown ();
  xfsm_compat_kde_shutdown ();

  /* kill legacy clients */
  xfsm_legacy_shutdown ();

#if !defined(__NR_ioprio_set) && defined(HAVE_SYNC)
  /* sync disk block in-core status with that on disk.  if
   * we have ioprio_set (), then we've already synced. */
  if (fork () == 0)
    {
# ifdef HAVE_SETSID
      setsid ();
# endif
      sync ();
      _exit (EXIT_SUCCESS);
    }
#endif  /* HAVE_SYNC */

  if (type == XFSM_SHUTDOWN_LOGOUT)
    return EXIT_SUCCESS;

  shutdown_helper = xfsm_shutdown_helper_new ();
  result = xfsm_shutdown_helper_send_command (shutdown_helper, type, &error);
  g_object_unref (shutdown_helper);

  if (!result)
    {
      xfce_message_dialog (NULL, _("Shutdown Failed"),
                           GTK_STOCK_DIALOG_ERROR,
                           _("Unable to perform shutdown"),
                           error->message,
                           GTK_STOCK_QUIT, GTK_RESPONSE_ACCEPT,
                           NULL);
      g_error_free (error);
      return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}
