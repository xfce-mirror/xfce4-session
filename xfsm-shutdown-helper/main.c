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
 * XXX - since this program is executed with root permissions, it may not
 *       be a good idea to trust glib!!
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib.h>

/* XXX */
#ifdef POWEROFF_CMD
#undef POWEROFF_CMD
#endif
#ifdef REBOOT_CMD
#undef REBOOT_CMD
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define POWEROFF_CMD  "/sbin/shutdown -p now"
#define REBOOT_CMD    "/sbin/shutdown -r now"
#else
#define POWEROFF_CMD  "/sbin/shutdown -h now"
#define REBOOT_CMD    "/sbin/shutdown -r now"
#endif


static gboolean
run (const gchar *command)
{
#if defined(HAVE_SIGPROCMASK)
  sigset_t sigset;
#endif
  gboolean result;
  gchar **argv;
  gchar **envp;
  GError *err;
  gint status;
  gint argc;

#if defined(HAVE_SETSID)
  setsid ();
#endif
  
#if defined (HAVE_SIGPROCMASK)
  sigemptyset (&sigset);
  sigaddset (&sigset, SIGHUP);
  sigaddset (&sigset, SIGINT);
  sigprocmask (SIG_BLOCK, &sigset, NULL);
#endif

  result = g_shell_parse_argv (command, &argc, &argv, &err);

  if (result)
    {
      envp = g_new0 (gchar *, 1);

      result = g_spawn_sync (NULL, argv, envp,
                             G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |
                             G_SPAWN_STDERR_TO_DEV_NULL,
                             NULL, NULL, NULL, NULL, &status, &err);

      g_strfreev (envp);
      g_strfreev (argv);
    }

  if (!result)
    {
      g_error_free (err);
      return FALSE;
    }

  return (WIFEXITED (status) && WEXITSTATUS (status) == 0);
}


int
main (int argc, char **argv)
{
  gboolean succeed = FALSE;
  char action[1024];

  /* display banner */
  fprintf (stdout, "XFSM_SUDO_DONE ");
  fflush (stdout);

  if (fgets (action, 1024, stdin) == NULL)
    {
      fprintf (stdout, "FAILED\n");
      return EXIT_FAILURE;
    }

  if (strncasecmp (action, "POWEROFF", 8) == 0)
    {
      succeed = run (POWEROFF_CMD);
    }
  else if (strncasecmp (action, "REBOOT", 6) == 0)
    {
      succeed = run (REBOOT_CMD);
    }

  if (succeed)
    {
      fprintf (stdout, "SUCCEED\n");
      return EXIT_SUCCESS;
    }

  fprintf (stdout, "FAILED\n");
  return EXIT_FAILURE;
}


