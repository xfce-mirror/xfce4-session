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
 * XXX - since this program is executed with root permissions, it may not
 *       be a good idea to trust glib!!
 */

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
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

#include <glib.h>
#include <stdio.h>

#include "libxfsm/xfsm-shutdown-common.h"

/* XXX */
#define EXIT_CODE_SUCCESS 0
#define EXIT_CODE_FAILED 1
#define EXIT_CODE_ARGUMENTS_INVALID 3
#define EXIT_CODE_INVALID_USER 4


static gboolean
run (const gchar *command)
{
#if defined(HAVE_SIGPROCMASK)
  sigset_t sigset;
#endif
  gboolean result;
  gchar **argv;
  gchar **envp;
  GError *err = NULL;
  gint status;
  gint argc;

#if defined(HAVE_SETSID)
  setsid ();
#endif

#if defined(HAVE_SIGPROCMASK)
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
                             G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                             NULL, NULL, NULL, NULL, &status, &err);

      g_strfreev (envp);
      g_strfreev (argv);
    }

  if (!result)
    {
      puts (err->message);
      g_error_free (err);
      return FALSE;
    }

  return (WIFEXITED (status) && WEXITSTATUS (status) == 0);
}


int
main (int argc, char **argv)
{
  GOptionContext *context;
  gint uid;
  gint euid;
  const gchar *pkexec_uid_str;
  gboolean shutdown = FALSE;
  gboolean restart = FALSE;
  gboolean suspend = FALSE;
  gboolean hibernate = FALSE;

  const GOptionEntry options[] = {
    { "shutdown", '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &shutdown, "Shutdown the system", NULL },
    { "restart", '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &restart, "Restart the system", NULL },
    { "suspend", '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &suspend, "Suspend the system", NULL },
    { "hibernate", '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &hibernate, "Hibernate the system", NULL },
    { NULL }
  };

  context = g_option_context_new (NULL);
  g_option_context_set_summary (context, "XFCE Session Helper");
  g_option_context_add_main_entries (context, options, NULL);
  g_option_context_parse (context, &argc, &argv, NULL);
  g_option_context_free (context);

  /* no input */
  if (!shutdown && !restart && !suspend && !hibernate)
    {
      puts ("No valid option was specified");
      return EXIT_CODE_ARGUMENTS_INVALID;
    }

  /* get calling process */
  uid = getuid ();
  euid = geteuid ();
  if (uid != 0 || euid != 0)
    {
      puts ("This program can only be used by the root user");
      return EXIT_CODE_ARGUMENTS_INVALID;
    }

  /* check we're not being spoofed */
  pkexec_uid_str = g_getenv ("PKEXEC_UID");
  if (pkexec_uid_str == NULL)
    {
      puts ("This program must only be run through pkexec");
      return EXIT_CODE_INVALID_USER;
    }

  /* run the command */
  if (shutdown)
    {
      if (run (POWEROFF_CMD))
        {
          return EXIT_CODE_SUCCESS;
        }
      else
        {
          return EXIT_CODE_FAILED;
        }
    }
  else if (restart)
    {
      if (run (REBOOT_CMD))
        {
          return EXIT_CODE_SUCCESS;
        }
      else
        {
          return EXIT_CODE_FAILED;
        }
    }
  else if (suspend)
    {
      if (run (UP_BACKEND_SUSPEND_COMMAND))
        {
          return EXIT_CODE_SUCCESS;
        }
      else
        {
          return EXIT_CODE_FAILED;
        }
    }
  else if (hibernate)
    {
      if (run (UP_BACKEND_HIBERNATE_COMMAND))
        {
          return EXIT_CODE_SUCCESS;
        }
      else
        {
          return EXIT_CODE_FAILED;
        }
    }

  /* how did we get here? */
  return EXIT_CODE_FAILED;
}
