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
 * Parts of this file where taken from gnome-session/logout.c, which
 * was written by Owen Taylor <otaylor@redhat.com>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <gtk/gtk.h>

#include <xfce4-session/shutdown.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-splash-theme.h>
#include <xfce4-session/xfsm-util.h>

#define BORDER		6

static GList *fadeout_screens = NULL;

static char stipple_data[] = {
  ' ', '.',
  '.', ' ',
};

/* XXX - Make this a module XfsmFadeout !! */

static void
fadeout_screen (GdkScreen      *screen,
                const GdkColor *focolor)
{
  GdkGCValues values;
  GdkWindow *root;
  GdkColor *color;
  GdkBitmap *bm;
  GdkGC *gc;
  int w, h;
  int m;

  root = gdk_screen_get_root_window (screen);
  gdk_drawable_get_size (GDK_DRAWABLE (root), &w, &h);

  fadeout_screens = g_list_prepend (fadeout_screens, screen);

  bm = gdk_bitmap_create_from_data (root, stipple_data, 2, 2);

  values.function = GDK_COPY;
  values.fill = GDK_STIPPLED;
  values.stipple = GDK_PIXMAP (bm);
  values.subwindow_mode = TRUE;

  gc = gdk_gc_new_with_values (GDK_DRAWABLE (root), &values,
                               GDK_GC_FUNCTION | GDK_GC_FILL |
                               GDK_GC_STIPPLE | GDK_GC_SUBWINDOW);

  color = gdk_color_copy (focolor);
  gdk_gc_set_rgb_fg_color (gc, color);
  gdk_color_free (color);

  for (m = 0; m < gdk_screen_get_n_monitors (screen); ++m)
    {
      GdkRectangle geometry;

      gdk_screen_get_monitor_geometry (screen, m, &geometry);

      gdk_draw_rectangle (GDK_DRAWABLE (root), gc, TRUE,
                          geometry.x, geometry.y,
                          geometry.width, geometry.height);
    }

  g_object_unref (G_OBJECT (gc));
  g_object_unref (G_OBJECT (bm));
}

static void
show_fadeout_windows (XfceRc *rc)
{
  const gchar *theme_name;
  XfsmSplashTheme *theme;
  GdkDisplay *display;
  GdkColor focolor;
  int n;

  /* load fadeout settings */
  xfce_rc_set_group (rc, "General");
  theme_name = xfce_rc_read_entry (rc, "SplashTheme", "Default");
  theme = xfsm_splash_theme_load (theme_name);
  xfsm_splash_theme_get_focolor (theme, &focolor);
  xfsm_splash_theme_destroy (theme);

  display = gdk_display_get_default ();
  for (n = 0; n < gdk_display_get_n_screens (display); ++n)
    fadeout_screen (gdk_display_get_screen (display, n), &focolor);
}

static void
hide_fadeout_windows (void)
{
  GdkWindowAttr attr;
  GdkWindow *window;
  GdkWindow *root;
  GList *lp;

  attr.x = 0;
  attr.y = 0;
  attr.event_mask = 0;
  attr.window_type = GDK_WINDOW_TOPLEVEL;
  attr.wclass = GDK_INPUT_OUTPUT;
  attr.override_redirect = TRUE;

  for (lp = fadeout_screens; lp != NULL; lp = lp->next)
    {
      root = gdk_screen_get_root_window (GDK_SCREEN (lp->data));

      gdk_drawable_get_size (GDK_DRAWABLE (root), &attr.width, &attr.height);

      window = gdk_window_new (root, &attr, GDK_WA_X | GDK_WA_Y |
                               GDK_WA_NOREDIR);

      gdk_window_show (window);
      gdk_flush ();
      gdk_window_hide (window);
      g_object_unref (G_OBJECT (window));
    }

  g_list_free (fadeout_screens);
  fadeout_screens = NULL;
}


/*
 */
gboolean
shutdownDialog(gint *shutdownType, gboolean *saveSession)
{
	gboolean accessibility;
  GdkScreen *screen;
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *dbox;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *image;
  GtkWidget *radio_vbox;
  GtkWidget *radio_logout;
  GtkWidget *radio_reboot;
  GtkWidget *radio_halt;
	GtkWidget *checkbox;
	GtkWidget *hidden;
  gboolean saveonexit;
  gboolean autosave;
  gint monitor;
	gint result;
  XfceRc *rc;

	g_return_val_if_fail(saveSession != NULL, FALSE);
	g_return_val_if_fail(shutdownType != NULL, FALSE);

  rc = xfsm_open_config (FALSE);

  xfce_rc_set_group (rc, "General");
  saveonexit = xfce_rc_read_bool_entry (rc, "SaveOnExit", TRUE);
  autosave = xfce_rc_read_bool_entry (rc, "AutoSave", FALSE);

  /* It's really bad here if someone else has the pointer
   * grabbed, so we first grab the pointer and keyboard
   * to an offscreen window, and then once we have the
   * server grabbed, move that to our dialog.
   */
  gtk_rc_reparse_all ();

  /* get screen with pointer */
  screen = xfsm_locate_screen_with_pointer (&monitor);
  if (screen == NULL)
    {
      screen = gdk_screen_get_default ();
      monitor = 0;
    }

	/* Try to grab Input on a hidden window first */
	hidden = gtk_invisible_new_for_screen (screen);
	gtk_widget_show_now (hidden);

	accessibility = GTK_IS_ACCESSIBLE (gtk_widget_get_accessible (hidden));

  if (!accessibility) 
    {
      for (;;)
        {
          if (gdk_pointer_grab (hidden->window, FALSE, 0, NULL, NULL,
                                GDK_CURRENT_TIME) == GDK_GRAB_SUCCESS)
            {
              if (gdk_keyboard_grab (hidden->window, FALSE, GDK_CURRENT_TIME)
                  == GDK_GRAB_SUCCESS)
                {
                  break;
                }
          
              gdk_pointer_ungrab (GDK_CURRENT_TIME);
            }

          g_usleep (50 * 1000);
        }

      dialog = g_object_new (GTK_TYPE_DIALOG,
                             "type", GTK_WINDOW_POPUP,
                             NULL);
    }
  else
    {
      dialog = gtk_dialog_new ();
      atk_object_set_role (gtk_widget_get_accessible (dialog), ATK_ROLE_ALERT);
      gtk_window_set_decorated (GTK_WINDOW (dialog), FALSE);
    }

  gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL,
                         GTK_RESPONSE_CANCEL);
  gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_OK,
                         GTK_RESPONSE_OK);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  gtk_window_set_screen (GTK_WINDOW (dialog), screen);

	dbox = GTK_DIALOG(dialog)->vbox;

	/* */
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(dbox), hbox, TRUE, TRUE, BORDER);
	gtk_widget_show(hbox);

	/* */
	image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_QUESTION,
                                    GTK_ICON_SIZE_DIALOG);
	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, TRUE, BORDER);
	gtk_widget_show(image);

	/* */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, BORDER);
	gtk_widget_show(vbox);

	/* */
	label = gtk_label_new(_("What do you want to do next?"));
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, BORDER);
	gtk_widget_show(label);

  radio_vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), radio_vbox, TRUE, FALSE, BORDER);
  gtk_widget_show (radio_vbox);

  radio_logout = gtk_radio_button_new_with_label (
      NULL, _("Quit current session"));
  gtk_box_pack_start (GTK_BOX (radio_vbox), radio_logout, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_logout), TRUE);
  gtk_widget_show (radio_logout);

  radio_reboot = gtk_radio_button_new_with_label_from_widget (
      GTK_RADIO_BUTTON (radio_logout), _("Reboot the computer"));
  gtk_box_pack_start (GTK_BOX (radio_vbox), radio_reboot, FALSE, FALSE, 0);
  gtk_widget_show (radio_reboot);

  radio_halt = gtk_radio_button_new_with_label_from_widget (
      GTK_RADIO_BUTTON (radio_logout), _("Turn off the computer"));
  gtk_box_pack_start (GTK_BOX (radio_vbox), radio_halt, FALSE, FALSE, 0);
  gtk_widget_show (radio_halt);

	/* */
  if (!autosave)
    {
      checkbox = gtk_check_button_new_with_mnemonic(
          _("_Save session for future logins"));
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), saveonexit);
      gtk_box_pack_start(GTK_BOX(vbox), checkbox, FALSE, TRUE, BORDER);
      gtk_widget_show(checkbox);
    }
  else
    {
      checkbox = NULL;
    }

  /* create small border */
  if (!accessibility)
    xfsm_window_add_border (GTK_WINDOW (dialog));
	
  /* center dialog on target monitor */
  xfsm_center_window_on_screen (GTK_WINDOW (dialog), screen, monitor);

  if (!accessibility)
    {
      gdk_x11_grab_server ();

      show_fadeout_windows (rc);
    }

	/* need to realize the dialog first! */
	gtk_widget_show_now (dialog);

	/* Grab Keyboard and Mouse pointer */
  if (!accessibility) 
    {
	    gdk_pointer_grab (dialog->window, TRUE, 0, NULL, NULL, GDK_CURRENT_TIME);
    	gdk_keyboard_grab (dialog->window, FALSE, GDK_CURRENT_TIME);
    	XSetInputFocus (GDK_DISPLAY (), GDK_WINDOW_XWINDOW (dialog->window),
		                  RevertToParent, CurrentTime);
    }

	/* run the logout dialog */
	result = gtk_dialog_run (GTK_DIALOG(dialog));

	if (result == GTK_RESPONSE_OK) {
		*saveSession = autosave || gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON(checkbox));
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio_logout)))
      *shutdownType = SHUTDOWN_LOGOUT;
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio_reboot)))
      *shutdownType = SHUTDOWN_REBOOT;
    else
      *shutdownType = SHUTDOWN_HALT;
	}

	gtk_widget_destroy(dialog);
  gtk_widget_destroy(hidden);

	/* Release Keyboard/Mouse pointer grab */
  if (!accessibility)
    {
      hide_fadeout_windows ();

      gdk_x11_ungrab_server ();
      gdk_pointer_ungrab (GDK_CURRENT_TIME);
      gdk_keyboard_ungrab (GDK_CURRENT_TIME);
      gdk_flush ();
    }

	/*
	 * run an iteration through the main loop to give the
	 * confirm dialog time to disappear
	 */
	g_main_context_iteration (NULL, TRUE);

  /*
   * remember the current settings.
   */
  if (result == GTK_RESPONSE_OK)
    {
      xfce_rc_set_group (rc, "General");
      xfce_rc_write_entry (rc, "SessionName", session_name);
      xfce_rc_write_bool_entry (rc, "SaveOnExit", *saveSession);
    }

  xfce_rc_close (rc);

	return (result == GTK_RESPONSE_OK);
}


/*
 */
gint
shutdown(gint type)
{
	gchar *command;
	gchar *fallback;
	gchar *buf;
	GError *error;
	gint status;

	error = NULL;
  fallback = NULL;

	switch (type) {
	case SHUTDOWN_REBOOT:
		command = g_strdup_printf("%s reboot", SHUTDOWN_COMMAND);
		break;

	case SHUTDOWN_HALT:
		command = g_strdup_printf("%s poweroff", SHUTDOWN_COMMAND);
    fallback = g_strdup_printf("%s halt", SHUTDOWN_COMMAND);
		break;

	default:
		return(EXIT_SUCCESS);
	}

again:
	if (!g_spawn_command_line_sync(command, NULL, &buf, &status, &error)) {
    if (fallback != NULL)
      goto try_fallback;

		xfce_err(_("The following error occured while trying to "
			   "shutdown the computer:\n\n%s"), error->message);
		g_error_free(error);
		g_free(command);
		return(EXIT_FAILURE);
	}

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    if (fallback != NULL)
      goto try_fallback;

		xfce_err(_(
			"The following error occured while trying to "
			"shutdown the computer:\n\n%s"), buf);
		g_free(command);
		return(EXIT_FAILURE);
	}

	g_free(command);
	return(EXIT_SUCCESS);

try_fallback:
  /* try using fallback command */
  g_free(command);
  command = fallback;
  fallback = NULL;
  goto again;
}

