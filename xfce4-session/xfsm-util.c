/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* gsm-multiscreen.c
 *
 * Copyright (C) 2002  Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Mark McLoughlin <mark@skynet.ie>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <X11/Xlib.h>

#include <gdk/gdkx.h>

#include <libxfce4util/libxfce4util.h>

#include <xfce4-session/xfsm-util.h>


GtkWidget *
xfsm_imgbtn_new(const gchar *text, const gchar *icon, GtkWidget **label_ret)
{
	GtkWidget *button;
	GtkWidget *align;
	GtkWidget *image;
	GtkWidget *hbox;
  GtkWidget *label;

	button = gtk_button_new();

	align = gtk_alignment_new(0.5f, 0.5f, 0.0f, 0.0f);
	gtk_container_add(GTK_CONTAINER(button), align);
	gtk_widget_show(align);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(align), hbox);
	gtk_widget_show(hbox);

	image = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 2);
	gtk_widget_show(image);

	label = gtk_label_new_with_mnemonic(text);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
	gtk_widget_show(label);

  if (label_ret != NULL)
    *label_ret = label;

	return button;
}


gchar*
xfsm_display_fullname (GdkDisplay *display)
{
  const gchar *name;
  const gchar *np;
  gchar       *hostname;
  gchar        buffer[256];
  gchar       *bp;
                                                                                
  name = gdk_display_get_name (display);
  if (*name == ':')
    {
      hostname = xfce_gethostname ();
      g_strlcpy (buffer, hostname, 256);
      g_free (hostname);
                                                                                
      bp = buffer + strlen (buffer);
                                                                                
      for (np = name; *np != '\0' && *np != '.' && bp < buffer + 255; )
        *bp++ = *np++;
      *bp = '\0';
    }
  else
    {
      g_strlcpy (buffer, name, 256);
                                                                                
      for (bp = buffer + strlen (buffer) - 1; *bp != ':'; --bp)
        if (*bp == '.')
          {
            *bp = '\0';
            break;
          }
    }
                                                                                
  return g_strdup (buffer);
}


gchar*
xfsm_screen_fullname (GdkScreen *screen)
{
  gchar *display_name;
  gchar *screen_name;
  
  display_name = xfsm_display_fullname (gdk_screen_get_display (screen));
  screen_name = g_strdup_printf ("%s.%d", display_name, gdk_screen_get_number (screen));
  g_free (display_name);
  
  return screen_name;
}


gboolean
xfsm_start_application (gchar      **command,
                        gchar      **environment,
                        GdkScreen   *screen,
                        const gchar *current_directory,
                        const gchar *client_machine,
                        const gchar *user_id)
{
  gboolean result;
  gchar   *screen_name;
  gchar  **argv;
  gint     argc;
  gint     size;

  g_return_val_if_fail (command != NULL && *command != NULL, FALSE);

  argv = g_new (gchar *, 21);
  size = 20;
  argc = 0;

  if (client_machine != NULL)
    {
      /* setting current directory on a remote machine is not supported */
      current_directory = NULL;
      
      argv[argc++] = g_strdup ("xon");
      argv[argc++] = g_strdup (client_machine);
    }
  
  if (screen != NULL)
    {
      if (client_machine != NULL)
        screen_name = xfsm_screen_fullname (screen);
      else
        screen_name = gdk_screen_make_display_name (screen);
      argv[argc++] = g_strdup ("env");
      argv[argc++] = g_strdup_printf ("DISPLAY=%s", screen_name);
      g_free (screen_name);
    }

  for (; *command != NULL; ++command)
    {
      if (argc == size)
        {
          size *= 2;
          argv = g_realloc (argv, (size + 1) * sizeof (*argv));
        }
      
      argv[argc++] = xfce_expand_variables (*command, environment);
    }

  argv[argc] = NULL;
  
  result = g_spawn_async (current_directory,
                          argv,
                          environment,
                          G_SPAWN_LEAVE_DESCRIPTORS_OPEN | G_SPAWN_SEARCH_PATH,
                          NULL,
                          NULL,
                          NULL,
                          NULL);
  
  g_strfreev (argv);
  
  return result;
}


static gboolean
xfsm_screen_contains_pointer (GdkScreen *screen,
                              int       *x,
			                        int       *y)
{
	GdkWindow    *root_window;
	Window        root, child;
	Bool          retval;
	int           rootx, rooty;
	int           winx, winy;
	unsigned int  xmask;

	root_window = gdk_screen_get_root_window (screen);

	retval = XQueryPointer (gdk_display,
        gdk_x11_drawable_get_xid (GDK_DRAWABLE (root_window)),
				&root, &child, &rootx, &rooty, &winx, &winy, &xmask);

	if (x)
		*x = retval ? rootx : -1;
	if (y)
		*y = retval ? rooty : -1;

	return retval;
}


GdkScreen*
xfsm_locate_screen_with_pointer (gint *monitor_ret)
{
	GdkDisplay *display;
	int         n_screens, i;

	display = gdk_display_get_default ();

	n_screens = gdk_display_get_n_screens (display);
	for (i = 0; i < n_screens; i++) {
		GdkScreen  *screen;
		int         x, y;

		screen = gdk_display_get_screen (display, i);

		if (xfsm_screen_contains_pointer (screen, &x, &y)) {
			if (monitor_ret)
				*monitor_ret = gdk_screen_get_monitor_at_point (screen, x, y);

			return screen;
		}
	}

	if (monitor_ret)
		*monitor_ret = 0;

	return NULL;
}


void
xfsm_center_window_on_screen (GtkWindow *window,
			                        GdkScreen *screen,
                    			    gint       monitor)
{
	GtkRequisition requisition;
	GdkRectangle   geometry;
	gint           x, y;

	gdk_screen_get_monitor_geometry (screen, monitor, &geometry);
	gtk_widget_size_request (GTK_WIDGET (window), &requisition);

	x = geometry.x + (geometry.width - requisition.width) / 2;
	y = geometry.y + (geometry.height - requisition.height) / 2;

	gtk_window_move (window, x, y);
}
