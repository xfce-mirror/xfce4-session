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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include <xfce4-session/xfsm-shutdown-helper.h>


struct _XfsmShutdownHelper
{
  pid_t    pid;
  FILE    *infile;
  FILE    *outfile;
  gboolean need_password;
};


XfsmShutdownHelper*
xfsm_shutdown_helper_spawn (void)
{
#if defined(SUDO_CMD)
  XfsmShutdownHelper *helper;
  struct              rlimit rlp;
  char                buf[15];
  int                 parent_pipe[2];
  int                 child_pipe[2];
  int                 result;
  int                 n;

  helper = g_new0 (XfsmShutdownHelper, 1);

  result = pipe (parent_pipe);
  if (result < 0)
    goto error0;

  result = pipe (child_pipe);
  if (result < 0)
    goto error1;

  helper->pid = fork ();
  if (helper->pid < 0)
    {
      goto error2;
    }
  else if (helper->pid == 0)
    {
      /* setup signals */
      signal (SIGPIPE, SIG_IGN);

      /* setup environment */
      xfce_setenv ("LC_ALL", "C", TRUE);
      xfce_setenv ("LANG", "C", TRUE);
      xfce_setenv ("LANGUAGE", "C", TRUE);

      /* setup the 3 standard file handles */
      dup2 (child_pipe[0], STDIN_FILENO);
      dup2 (parent_pipe[1], STDOUT_FILENO);
      dup2 (parent_pipe[1], STDERR_FILENO);

      /* Close all other file handles */
      getrlimit (RLIMIT_NOFILE, &rlp);
      for (n = 0; n < (int) rlp.rlim_cur; ++n)
        {
          if (n != STDIN_FILENO && n != STDOUT_FILENO && n != STDERR_FILENO)
            close (n);
        }

      /* execute sudo with the helper */
      execl (SUDO_CMD, "sudo", "-H", "-S", "-p",
             "XFSM_SUDO_PASS ", "--", XFSM_SHUTDOWN_HELPER, NULL);
      _exit (127);
    }

  close (parent_pipe[1]);

  /* read sudo/helper answer */
  n = read (parent_pipe[0], buf, 15);
  if (n < 15)
    goto error2;

  helper->infile = fdopen (parent_pipe[0], "r");
  if (helper->infile == NULL)
    goto error2;

  helper->outfile = fdopen (child_pipe[1], "w");
  if (helper->outfile == NULL)
    goto error3;

  if (memcmp (buf, "XFSM_SUDO_PASS ", 15) == 0)
    {
      helper->need_password = TRUE;
    }
  else if (memcmp (buf, "XFSM_SUDO_DONE ", 15) == 0)
    {
      helper->need_password = FALSE;
    }
  else
    goto error3;

  close (parent_pipe[1]);
  close (child_pipe[0]);

  return helper;

error3:
  if (helper->infile != NULL)
    fclose (helper->infile);
  if (helper->outfile != NULL)
    fclose (helper->outfile);

error2:
  close (child_pipe[0]);
  close (child_pipe[1]);

error1:
  close (parent_pipe[0]);
  close (parent_pipe[1]);

error0:
  g_free (helper);
  return NULL;
#else
  /* won't work without sudo */
  return NULL;
#endif
}


gboolean
xfsm_shutdown_helper_need_password (const XfsmShutdownHelper *helper)
{
  return helper->need_password;
}


gboolean
xfsm_shutdown_helper_send_password (XfsmShutdownHelper *helper,
                                    const gchar        *password)
{
  char buffer[1024];
  ssize_t result;
  size_t failed;
  size_t length;
  size_t bytes;
  int fd;

  g_return_val_if_fail (helper != NULL, FALSE);
  g_return_val_if_fail (password != NULL, FALSE);
  g_return_val_if_fail (helper->need_password, FALSE);

  g_snprintf (buffer, 1024, "%s\n", password);
  length = strlen (buffer);
  bytes = fwrite (buffer, 1, length, helper->outfile);
  fflush (helper->outfile);
  bzero (buffer, length);

  if (bytes != length)
    {
      fprintf (stderr, "Failed to write password (bytes=%u, length=%u)\n",
               bytes, length);
    return FALSE;
    }

  if (ferror (helper->outfile))
    {
      fprintf (stderr, "Pipe error\n");
      return FALSE;
    }

  fd = fileno (helper->infile);

  for (failed = length = 0;;)
    {
      result = read (fd, buffer + length, 256 - length);

      if (result < 0)
        {
          perror ("read");
          return FALSE;
        }
      else if (result == 0)
        {
          if (++failed > 20)
            return FALSE;
          continue;
        }
      else if (result + length >= 1024)
        {
          fprintf (stderr, "Too much output from sudo!\n");
          return FALSE;
        }

      length += result;
      buffer[length] = 0;

      if (length >= 15)
        {
          if (strncmp (buffer + (length - 15), "XFSM_SUDO_PASS ", 15) == 0)
            {
              return FALSE;
            }
          else if (strncmp (buffer + (length - 15), "XFSM_SUDO_DONE ", 15) == 0)
            {
              helper->need_password = FALSE;
              break;
            }
        } 
    }

  return TRUE;
}


gboolean
xfsm_shutdown_helper_send_command (XfsmShutdownHelper *helper,
                                   XfsmShutdownCommand command)
{
  static char *command_table[] = { "POWEROFF", "REBOOT" };
  char response[256];

  g_return_val_if_fail (helper != NULL, FALSE);
  g_return_val_if_fail (!helper->need_password, FALSE);

  fprintf (helper->outfile, "%s\n", command_table[command]);
  fflush (helper->outfile);

  if (ferror (helper->outfile))
    {
      fprintf (stderr, "Error sending command to helper\n");
      return FALSE;
    }

  if (fgets (response, 256, helper->infile) == NULL)
    {
      fprintf (stderr, "No response from helper\n");
      return FALSE;
    }

  if (strncmp (response, "SUCCEED", 7) != 0)
    {
      fprintf (stderr, "Command failed\n");
      return FALSE;
    }

  return TRUE;
}


void
xfsm_shutdown_helper_destroy (XfsmShutdownHelper *helper)
{
  int status;

  g_return_if_fail (helper != NULL);

  if (helper->infile != NULL)
    fclose (helper->infile);
  if (helper->outfile != NULL)
    fclose (helper->outfile);

  if (helper->pid > 0)
    waitpid (helper->pid, &status, 0);

  g_free (helper);
}


