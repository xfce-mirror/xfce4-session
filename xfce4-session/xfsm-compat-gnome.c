/* $Id$ */
/*-
 * Copyright (c) 2004-2005 Benedikt Meurer <benny@xfce.org>
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
 * Most parts of this file where taken from gnome-session.
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

#ifdef HAVE_SIGNAL_H
#include <signal.h>
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
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <gdk/gdkx.h>

#include <libxfce4util/libxfce4util.h>

#include <xfce4-session/xfsm-compat-gnome.h>

#define GNOME_KEYRING_DAEMON "gnome-keyring-daemon"


static gboolean gnome_compat_started = FALSE;
static int keyring_lifetime_pipe[2];
static pid_t gnome_keyring_daemon_pid = 0;
static Window gnome_smproxy_window = None;

static void
child_setup (gpointer user_data)
{
  gint open_max;
  gint fd;
  char *fd_str;

  open_max = sysconf (_SC_OPEN_MAX);
  for (fd = 3; fd < open_max; fd++)
    {
      if (fd != keyring_lifetime_pipe[0])
        fcntl (fd, F_SETFD, FD_CLOEXEC);
    }

  fd_str = g_strdup_printf ("%d", keyring_lifetime_pipe[0]);
  g_setenv ("GNOME_KEYRING_LIFETIME_FD", fd_str, TRUE);
  g_free (fd_str);
}


static void
gnome_keyring_daemon_startup (void)
{
  GError *error = NULL;
  gchar  *sout;
  gchar **lines;
  gsize   lineno;
  gint    status;
  long    pid;
  gchar  *pid_str;
  gchar  *end;
  char *argv[3];

  /* Pipe to slave keyring lifetime to */
  if (pipe (keyring_lifetime_pipe))
    {
      g_warning ("Failed to set up pipe for gnome-keyring: %s", strerror (errno));
      return;
    }

  error = NULL;
  argv[0] = GNOME_KEYRING_DAEMON;
  argv[1] = "--start";
  argv[2] = NULL;
  g_spawn_sync (NULL, argv, NULL,
                G_SPAWN_SEARCH_PATH | G_SPAWN_LEAVE_DESCRIPTORS_OPEN,
                child_setup, NULL,
                &sout, NULL, &status, &error);

  close (keyring_lifetime_pipe[0]);
  /* We leave keyring_lifetime_pipe[1] open for the lifetime of the session,
     in order to slave the keyring daemon lifecycle to the session. */

  if (error != NULL)
    {
      g_printerr ("Failed to run gnome-keyring-daemon: %s\n",
                  error->message);
      g_error_free (error);
    }
  else
    {
      if (WIFEXITED (status) && WEXITSTATUS (status) == 0 && sout != NULL)
        {
          lines = g_strsplit (sout, "\n", 0);

          for (lineno = 0; lines[lineno] != NULL; lineno++)
            {
              xfce_putenv (lines[lineno]);

              if (g_str_has_prefix (lines[lineno], "GNOME_KEYRING_PID="))
                {
                  pid_str = lines[lineno] + strlen ("GNOME_KEYRING_PID=");
                  pid = strtol (pid_str, &end, 10);
                  if (end != pid_str)
                    {
                      gnome_keyring_daemon_pid = pid;
                    }
                }
            }

          g_strfreev (lines);
        }
      else
        {
          /* daemon failed for some reason */
          g_printerr ("gnome-keyring-daemon failed to start correctly, "
                      "exit code: %d\n", WEXITSTATUS (status));
        }

      g_free (sout);
    }
}

static void
gnome_keyring_daemon_shutdown (void)
{
  if (gnome_keyring_daemon_pid != 0)
    {
      kill (gnome_keyring_daemon_pid, SIGTERM);
      gnome_keyring_daemon_pid = 0;
    }
}



static void
xfsm_compat_gnome_smproxy_startup (void)
{
  Atom gnome_sm_proxy;
  Display *dpy;
  Window root;

  gdk_error_trap_push ();

  /* Set GNOME_SM_PROXY property, since some apps (like OOo) seem to require
   * it to behave properly. Thanks to Jasper/Francois for reporting this.
   * This has another advantage, since it prevents people from running
   * gnome-smproxy in xfce4, which would cause trouble otherwise.
   */
  dpy = gdk_display;
  root = RootWindow (dpy, 0);

  if (gnome_smproxy_window != None)
    XDestroyWindow (dpy, gnome_smproxy_window);

  gnome_sm_proxy = XInternAtom (dpy, "GNOME_SM_PROXY", False);
  gnome_smproxy_window = XCreateSimpleWindow (dpy, root, 1, 1, 1, 1, 0, 0, 0);

  XChangeProperty (dpy, gnome_smproxy_window, gnome_sm_proxy,
                   XA_CARDINAL, 32, PropModeReplace,
                   (unsigned char *) (void *) &gnome_smproxy_window, 1);
  XChangeProperty (dpy, root, gnome_sm_proxy,
                   XA_CARDINAL, 32, PropModeReplace,
                   (unsigned char *) (void *) &gnome_smproxy_window, 1);

  XSync (dpy, False);

  gdk_error_trap_pop ();
}


static void
xfsm_compat_gnome_smproxy_shutdown (void)
{
  gdk_error_trap_push ();

  if (gnome_smproxy_window != None)
    {
      XDestroyWindow (gdk_display, gnome_smproxy_window);
      XSync (gdk_display, False);
      gnome_smproxy_window = None;
    }

  gdk_error_trap_pop ();
}


void
xfsm_compat_gnome_startup (XfsmSplashScreen *splash)
{
  if (G_UNLIKELY (gnome_compat_started))
    return;

  xfsm_compat_gnome_smproxy_startup ();

  /* fire up the keyring daemon */
  if (G_LIKELY (splash != NULL))
    xfsm_splash_screen_next (splash, _("Starting The Gnome Keyring Daemon"));
  gnome_keyring_daemon_startup ();

  gnome_compat_started = TRUE;
}


void
xfsm_compat_gnome_shutdown (void)
{
  if (G_UNLIKELY (!gnome_compat_started))
    return;

  /* shutdown the keyring daemon */
  gnome_keyring_daemon_shutdown ();

  xfsm_compat_gnome_smproxy_shutdown ();

  gnome_compat_started = FALSE;
}

