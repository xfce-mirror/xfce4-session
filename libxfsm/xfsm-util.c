/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2004 Jasper Huijsmans <jasper@xfce.org>
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
 * Parts of this file where taken from gnome-session/gsm-multiscreen.c,
 * which was written by Mark McLoughlin <mark@skynet.ie>.
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

#include <libxfcegui4/libxfcegui4.h>

#include <libxfsm/xfsm-util.h>


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
        screen_name = xfce_gdk_screen_get_fullname (screen);
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


void
xfsm_place_trash_window (GtkWindow *window,
                         GdkScreen *screen,
                         gint       monitor)
{
  GtkRequisition requisition;
  GdkRectangle   geometry;

  gdk_screen_get_monitor_geometry (screen, monitor, &geometry);
  gtk_widget_size_request (GTK_WIDGET (window), &requisition);

  gtk_window_move (window, 0, geometry.height - requisition.height);
}


gchar**
xfsm_strv_copy (gchar **v)
{
  gchar **rv;
  gsize len;
  gsize n;

  for (len = 0; v[len] != NULL; ++len) ;

  rv = g_new (gchar *, len + 1);
  for (n = 0; n < len; ++n)
    rv[n] = g_strdup (v[n]);
  rv[n] = NULL;

  return rv;
}


gboolean
xfsm_strv_equal (gchar **a, gchar **b)
{
  if ((a == NULL && b != NULL) || (a != NULL && b == NULL))
    return FALSE;
  else if (a == NULL || b == NULL)
    return TRUE;

  while (*a != NULL && *b != NULL)
    {
      if (strcmp (*a, *b) != 0)
        return FALSE;

      ++a;
      ++b;
    }

  return (*a == NULL && *b == NULL);
}


void
xfsm_window_add_border (GtkWindow *window)
{
  GtkWidget *box1, *box2;
  
  gtk_widget_realize(GTK_WIDGET(window));

  box1 = gtk_event_box_new ();
  gtk_widget_modify_bg (box1, GTK_STATE_NORMAL, 
                        &(GTK_WIDGET(window)->style->bg [GTK_STATE_SELECTED]));
  gtk_widget_show (box1);

  box2 = gtk_event_box_new ();
  gtk_widget_show (box2);
  gtk_container_add (GTK_CONTAINER (box1), box2);

  gtk_container_set_border_width (GTK_CONTAINER (box2), 3);
  gtk_widget_reparent (GTK_BIN (window)->child, box2);

  gtk_container_add (GTK_CONTAINER (window), box1);
}


void
xfsm_window_grab_input (GtkWindow *window)
{
  GdkWindow *xwindow = GTK_WIDGET (window)->window;

  gdk_pointer_grab (xwindow, TRUE, 0, NULL, NULL, GDK_CURRENT_TIME);
  gdk_keyboard_grab (xwindow, FALSE, GDK_CURRENT_TIME);
  XSetInputFocus (GDK_DISPLAY (), GDK_WINDOW_XWINDOW (xwindow),
                  RevertToParent, CurrentTime);
}


XfceRc*
xfsm_open_config (gboolean readonly)
{
  return xfce_rc_config_open (XFCE_RESOURCE_CONFIG,
                              "xfce4-session/xfce4-session.rc",
                              readonly);
}

