/* $Id$ */
/*-
 * Copyright (c) 2004 Brian Tarricone <kelnos@xfce.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfcegui4/libxfcegui4.h>



static gboolean
quit_timer (gpointer user_data)
{
  SessionClient *client = user_data;

  logout_session (client);

  return FALSE;
}



static void
die_handler (gpointer user_data)
{
  if (gtk_main_level ())
    gtk_main_quit ();
}



static void
usage (int exit_code)
{
  fprintf (stderr,
           "Usage: %s [OPTION...]\n"
           "\n"
           "GTK+\n"
           "  --display=DISPLAY     X display to use\n"
           "\n"
           "Application options\n"
           "  --help                Print this help message and exit\n"
           "  --version             Print version information and exit\n"
           "\n",
           PACKAGE_STRING);
  exit (exit_code);
}



int
main (int argc, char **argv)
{
  SessionClient *client;
  gboolean       managed;

  gtk_init (&argc, &argv);

  for (++argv; --argc > 0; ++argv)
    {
      if (strcmp (*argv, "--version") == 0)
        {
          printf ("%s (Xfce %s)\n\n"
                  "Copyright (c) 2004\n"
                  "        The Xfce development team. All rights reserved.\n\n"
                  "Written for Xfce by Brian Tarricone <kelnos@xfce.org> and\n"
                  "Benedikt Meurer <benny@xfce.org>.\n\n"
                  "Please report bugs to <%s>.\n",
                  PACKAGE_STRING, xfce_version_string(), PACKAGE_BUGREPORT);
          return EXIT_SUCCESS;
        }
      else
        {
          usage (strcmp (*argv, "--help") == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
        }
    }

  client = client_session_new (argc, argv, NULL, SESSION_RESTART_NEVER, 99);
  client->die = die_handler;
  managed = session_init (client);

  if (managed)
    {
      g_idle_add_full (G_PRIORITY_LOW, quit_timer,
                       client, die_handler);
      gtk_main ();
    }
  else
    {
      fprintf (stderr, "xfce4-session-logout: No session manager running.\n");
      return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}


