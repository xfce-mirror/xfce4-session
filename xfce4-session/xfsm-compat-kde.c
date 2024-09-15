/* $Id$ */
/*-
 * Copyright (c) 2004 Benedikt Meurer <benny@xfce.org>
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
#include "config.h"
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

#include "xfsm-compat-kde.h"


static gboolean kde_compat_started = FALSE;


static gboolean
run_timeout (gpointer user_data)
{
  int status;
  int result;
  pid_t pid = *((pid_t *) user_data);

  result = waitpid (pid, &status, WNOHANG);

  if (result == pid)
    {
      gtk_main_quit ();
    }
  else if (result == -1)
    {
      g_warning ("Failed to wait for process %d: %s",
                 (int)pid, g_strerror (errno));
      gtk_main_quit ();
    }

  return TRUE;
}


static void
run (const gchar *command)
{
  gchar buffer[2048];
  GError *error = NULL;
  gchar **argv;
  gint    argc;
  pid_t   pid;

  g_snprintf (buffer, 2048, "env DYLD_FORCE_FLAT_NAMESPACE= LD_BIND_NOW=true "
              "SESSION_MANAGER= %s", command);

  if (!g_shell_parse_argv (buffer, &argc, &argv, &error))
    {
      g_warning ("Unable to parse \"%s\": %s", buffer, error->message);
      g_error_free (error);
      return;
    }

  if (g_spawn_async (NULL, argv, NULL,
                     G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
                     NULL, NULL, &pid, &error))
    {
      guint id = g_timeout_add (300, run_timeout, &pid);
      gtk_main ();
      g_source_remove (id);
    }
  else
    {
      g_warning ("Unable to exec \"%s\": %s", buffer, error->message);
      g_error_free (error);
    }

  g_strfreev (argv);
}


void
xfsm_compat_kde_startup (void)
{
  gchar command[256];

  if (G_UNLIKELY (kde_compat_started))
    return;

  run ("kdeinit4");

  /* tell klauncher about the session manager */
  g_snprintf (command, 256, "qdbus org.kde.klauncher /KLauncher setLaunchEnv "
                            "SESSION_MANAGER \"%s\"",
                            g_getenv ("SESSION_MANAGER"));
  run (command);

  /* tell kde if we are running multi-head */
  if (gdk_display_get_n_screens (gdk_display_get_default ()) > 1)
    {
      g_snprintf (command, 256, "qdbus org.kde.klauncher /KLauncher setLaunchEnv "
                                "KDE_MULTIHEAD \"true\"");
      run (command);
    }

  kde_compat_started = TRUE;
}


void
xfsm_compat_kde_shutdown (void)
{
  if (G_UNLIKELY (!kde_compat_started))
    return;

  /* shutdown KDE services */
  run ("kdeinit4_shutdown");

  kde_compat_started = FALSE;
}
