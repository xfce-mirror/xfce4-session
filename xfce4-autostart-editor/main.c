/* $Id$ */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 * Coprright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <xfce4-autostart-editor/xfae-window.h>



static GdkNativeWindow opt_socket_id = 0;
static GOptionEntry    entries[] = 
{
  { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
  { NULL }
};



int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *plug;
  GtkWidget *plug_child;
  GError    *error = NULL;

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  if (G_UNLIKELY (!gtk_init_with_args (&argc, &argv, "", entries, PACKAGE, &error)))
    {
      if (G_LIKELY (error != NULL))
        {
          g_print ("%s: %s.\n", G_LOG_DOMAIN, error->message);
          g_print (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
          g_print ("\n");

          g_error_free (error);
        }

      return EXIT_FAILURE;
    }

  gtk_window_set_default_icon_name ("xfce4-autostart-editor");

  window = xfae_window_new ();

  if (opt_socket_id == 0)
    {
      g_signal_connect (G_OBJECT (window), "destroy",
                        G_CALLBACK (gtk_main_quit), NULL);
      gtk_widget_show (window);
    }
  else
    {
      /* Create plug widget */
      plug = gtk_plug_new (opt_socket_id);
      gtk_widget_show (plug);

      /* Embed plug child widget */
      plug_child = xfae_window_create_plug_child (XFAE_WINDOW (window));
      gtk_widget_reparent (plug_child, plug);
      gtk_widget_show (plug_child);
    }

  gtk_main ();

  return EXIT_SUCCESS;
}


