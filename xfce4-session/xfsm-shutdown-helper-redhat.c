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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * The permission check was taken from gnome-session/logout.c, originally
 * written by Owen Taylor <otaylor@redhat.com>.
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
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <xfce4-session/xfsm-shutdown-helper.h>


struct _XfsmShutdownHelper
{
  gint dummy;
};


static char *halt_command[] = { "/usr/bin/halt", "halt", NULL };
static char *reboot_command[] = { "/usr/bin/reboot", "reboot", NULL };


XfsmShutdownHelper*
xfsm_shutdown_helper_spawn (void)
{
  XfsmShutdownHelper *helper = NULL;
  gchar              *s;
  gchar              *t;

  s = g_build_filename ("/var/lock/console", g_get_user_name (), NULL);
  t = g_build_filename ("/var/run/console", g_get_user_name (), NULL);

  if (((geteuid () == 0)
        || g_file_test (s, G_FILE_TEST_EXISTS)
        || g_file_test (t, G_FILE_TEST_EXISTS))
      && access (halt_command[0], X_OK) == 0)
    {
      helper = g_new0 (XfsmShutdownHelper, 1);
    }
  
  g_free (s);
  g_free (t);

  return helper;
}


gboolean
xfsm_shutdown_helper_need_password (const XfsmShutdownHelper *helper)
{
  return FALSE;
}


gboolean
xfsm_shutdown_helper_send_password (XfsmShutdownHelper *helper,
                                    const gchar        *password)
{
  return TRUE;
}


gboolean
xfsm_shutdown_helper_send_command (XfsmShutdownHelper  *helper,
                                   XfsmShutdownCommand  command)
{
#ifdef HAVE_SIGPROCMASK
  sigset_t      sigset;
#endif
  struct rlimit rlp;
  int           result;
  int           status;
  int           fd;
  char        **argv;
  pid_t         pid;

  if (command == XFSM_SHUTDOWN_POWEROFF)
    argv = halt_command;
  else
    argv = reboot_command;

  pid = fork ();
  if (pid < 0)
    {
      return FALSE;
    }
  else if (pid == 0)
    {
#ifdef HAVE_SETSID
      setsid ();
#endif

#ifdef HAVE_SIGPROCMASK
      sigemptyset (&sigset);
      sigaddset (&sigset, SIGHUP);
      sigaddset (&sigset, SIGINT);
      sigaddset (&sigset, SIGPIPE);
      sigprocmask (SIG_BLOCK, &sigset, NULL);
#endif

      /* setup the 3 standard file handles */
      fd = open ("/dev/null", O_RDWR, 0);
      if (fd >= 0)
        {
          dup2 (fd, STDIN_FILENO);
          dup2 (fd, STDOUT_FILENO);
          dup2 (fd, STDERR_FILENO);
          close (fd);
        }

      /* Close all other file handles */
      getrlimit (RLIMIT_NOFILE, &rlp);
      for (fd = 0; fd < (int) rlp.rlim_cur; ++fd)
        {
          if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO)
            close (fd);
        }

      execv (argv[0], argv + 1);
      _exit (127);
    }

  /* check if the shutdown command failed */
  result = waitpid (pid, &status, 0);
  if ((result == pid && status != 0) || (result < 0 && errno == ECHILD))
    return FALSE;

  return TRUE;
}


void
xfsm_shutdown_helper_destroy (XfsmShutdownHelper *helper)
{
  g_free (helper);
}


