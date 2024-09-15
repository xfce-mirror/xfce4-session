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
 *
 * Parts of this file where taken from gnome-session/main.c, which
 * was written by Tom Tromey.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>

#include "libxfsm/xfsm-util.h"

#include "xfsm-dns.h"
#include "xfsm-global.h"


static gchar *
queryhostname (gchar *buffer, gsize length, gboolean readable)
{
#ifdef HAVE_GETHOSTNAME
  if (gethostname (buffer, length) == 0)
    return buffer;
#else
  struct utsname utsname;
  if (uname (&utsname) == 0)
    {
      g_strlcpy (buffer, utsname.nodename, length);
      return buffer;
    }
#endif
  if (readable)
    {
      g_strlcpy (buffer, _("(Unknown)"), length);
      return buffer;
    }
  return NULL;
}


static gboolean
check_for_dns (void)
{
#ifdef HAVE_GETADDRINFO
  struct addrinfo *result = NULL;
  struct addrinfo hints;
#endif
  char buffer[256];
  gchar *hostname;

  hostname = queryhostname (buffer, 256, FALSE);
  if (hostname == NULL)
    return FALSE;

#ifdef HAVE_GETADDRINFO
  bzero (&hints, sizeof (hints));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_CANONNAME;

  if (getaddrinfo (hostname, NULL, &hints, &result) != 0)
    return FALSE;

  if (g_ascii_strncasecmp (result->ai_canonname, hostname, 0) != 0)
    {
      freeaddrinfo (result);
      return FALSE;
    }

  freeaddrinfo (result);
#else
#ifdef HAVE_GETHOSTBYNAME
  if (gethostbyname (hostname) == NULL)
    {
      return FALSE;
    }
#endif
#endif

  return TRUE;
}


enum
{
  RESPONSE_LOG_IN,
  RESPONSE_TRY_AGAIN,
};


void
xfsm_dns_check (void)
{
  GtkWidget *msgbox = NULL;
  gchar hostname[256];
  gint response;

  while (!check_for_dns ())
    {
      if (msgbox == NULL)
        {
          GdkScreen *screen = xfce_gdk_screen_get_active (NULL);

          queryhostname (hostname, 256, TRUE);

          msgbox = gtk_message_dialog_new (NULL, 0,
                                           GTK_MESSAGE_WARNING,
                                           GTK_BUTTONS_NONE,
                                           _("Could not look up internet address for %s.\n"
                                             "This will prevent Xfce from operating correctly.\n"
                                             "It may be possible to correct the problem by adding\n"
                                             "%s to the file /etc/hosts on your system."),
                                           hostname, hostname);

          gtk_dialog_add_buttons (GTK_DIALOG (msgbox), _("Continue anyway"),
                                  RESPONSE_LOG_IN, _("Try again"), RESPONSE_TRY_AGAIN,
                                  NULL);

          gtk_window_set_screen (GTK_WINDOW (msgbox), screen);
          gtk_container_set_border_width (GTK_CONTAINER (msgbox), 6);
          gtk_window_set_position (GTK_WINDOW (msgbox), GTK_WIN_POS_CENTER);
        }

      gtk_dialog_set_default_response (GTK_DIALOG (msgbox), RESPONSE_TRY_AGAIN);

      response = gtk_dialog_run (GTK_DIALOG (msgbox));
      if (response != RESPONSE_TRY_AGAIN)
        break;

      gtk_widget_hide (msgbox);
    }

  if (msgbox != NULL)
    gtk_widget_destroy (msgbox);
}
