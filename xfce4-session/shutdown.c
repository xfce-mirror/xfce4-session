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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <gtk/gtk.h>

#include <xfce4-session/shutdown.h>
#include <xfce4-session/xfsm-compat-gnome.h>
#include <xfce4-session/xfsm-compat-kde.h>
#include <xfce4-session/xfsm-fadeout.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-legacy.h>
#include <xfce4-session/xfsm-shutdown-helper.h>
#include <xfce4-session/xfsm-util.h>

#define BORDER		6


static XfsmShutdownHelper *shutdown_helper = NULL;


#ifdef SESSION_SCREENSHOTS
static void
screenshot_save (GdkPixmap *pm, GdkRectangle *area)
{
  gchar *display_name;
  gchar *resource;
  gchar *filename;
  GdkDisplay *dpy;
  GdkPixbuf *spb;
  GdkPixbuf *pb;

  pb = gdk_pixbuf_get_from_drawable (NULL, GDK_DRAWABLE (pm), NULL,
                                     0, 0, 0, 0, area->width, area->height);

  if (pb != NULL)
    {
      /* scale down the pixbuf */
      spb = gdk_pixbuf_scale_simple (pb, 52, 43, GDK_INTERP_HYPER);

      if (spb != NULL)
        {
          /* determine thumb file */
          dpy = gdk_drawable_get_display (GDK_DRAWABLE (pm));
          display_name = xfsm_display_fullname (dpy);
          resource = g_strconcat ("sessions/thumbs-", display_name,
                                  "/", session_name, ".png", NULL);
          filename = xfce_resource_save_location (XFCE_RESOURCE_CACHE,
                                                  resource, TRUE);
          g_free (display_name);
          g_free (resource);

          gdk_pixbuf_save (spb, filename, "png", NULL, NULL);

          g_object_unref (G_OBJECT (spb));
          g_free (filename);
        }

      g_object_unref (G_OBJECT (pb));
    }
}
#endif


static void
entry_activate_cb (GtkWidget *entry, GtkDialog *dialog)
{
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}


/*
 */
gboolean
shutdownDialog(gint *shutdownType, gboolean *saveSession)
{
	gboolean accessibility;
  XfsmFadeout *fadeout = NULL;
  const gchar *theme_name;
  XfsmSplashTheme *theme;
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
  GtkWidget *entry_vbox;
  GtkWidget *entry;
	GtkWidget *hidden;
  GtkWidget *ok_button;
  GtkWidget *cancel_button;
  gboolean saveonexit;
  gboolean autosave;
  gint monitor;
	gint result;
  XfceRc *rc;
#ifdef SESSION_SCREENSHOTS
  GdkRectangle screenshot_area;
  GdkWindow *root;
  GdkPixmap *screenshot_pm = NULL;
  GdkGC *screenshot_gc;
#endif

  /* XXX - move to other module?! */
  GdkPixmap *saved = NULL;
  GdkGC *saved_gc = NULL;
  int saved_x, saved_y;
  int saved_w, saved_h;

	g_return_val_if_fail(saveSession != NULL, FALSE);
	g_return_val_if_fail(shutdownType != NULL, FALSE);

  /* destroy any previously running shutdown helper first */
  if (shutdown_helper != NULL)
    {
      xfsm_shutdown_helper_destroy (shutdown_helper);
      shutdown_helper = NULL;
    }

  /* load configuration */
  rc = xfsm_open_config (FALSE);
  xfce_rc_set_group (rc, "General");
  saveonexit = xfce_rc_read_bool_entry (rc, "SaveOnExit", TRUE);
  autosave = xfce_rc_read_bool_entry (rc, "AutoSave", FALSE);

  /** XXX - evalute General/PromptOnLogout! */

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

      gdk_x11_grab_server ();

#ifdef SESSION_SCREENSHOTS
      /* grab a screenshot */
      root = gdk_screen_get_root_window (screen);
      gdk_screen_get_monitor_geometry (screen, monitor, &screenshot_area);
      screenshot_pm = gdk_pixmap_new (GDK_DRAWABLE (root),
                                      screenshot_area.width,
                                      screenshot_area.height,
                                      -1);
      screenshot_gc = gdk_gc_new (GDK_DRAWABLE (screenshot_pm));
      gdk_gc_set_function (screenshot_gc, GDK_COPY);
      gdk_gc_set_subwindow (screenshot_gc, TRUE);
      gdk_draw_drawable (GDK_DRAWABLE (screenshot_pm),
                         screenshot_gc,
                         GDK_DRAWABLE (root),
                         screenshot_area.x,
                         screenshot_area.y,
                         0,
                         0,
                         screenshot_area.width,
                         screenshot_area.height);
      g_object_unref (G_OBJECT (screenshot_gc));
#endif

      /* display fadeout */
      xfce_rc_set_group (rc, "General");
      theme_name = xfce_rc_read_entry (rc, "SplashTheme", "Default");
      theme = xfsm_splash_theme_load (theme_name);
      fadeout = xfsm_fadeout_new (gtk_widget_get_display (hidden), theme);
      xfsm_splash_theme_destroy (theme);

      /* create confirm dialog */
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

  cancel_button = gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL,
                                         GTK_RESPONSE_CANCEL);
  ok_button = gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_OK,
                                     GTK_RESPONSE_OK);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  gtk_window_set_screen (GTK_WINDOW (dialog), screen);

	dbox = GTK_DIALOG(dialog)->vbox;

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(dbox), hbox, TRUE, TRUE, BORDER);
	gtk_widget_show(hbox);

	image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_QUESTION,
                                    GTK_ICON_SIZE_DIALOG);
	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, TRUE, BORDER);
	gtk_widget_show(image);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, BORDER);
	gtk_widget_show(vbox);

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

  /* connect to the shutdown helper */
  shutdown_helper = xfsm_shutdown_helper_spawn ();
  if (shutdown_helper == NULL)
    {
      gtk_widget_set_sensitive (radio_reboot, FALSE);
      gtk_widget_set_sensitive (radio_halt, FALSE);
    }

  /* save portion of the root window covered by the dialog */
  if (!accessibility && shutdown_helper != NULL)
    {
      GdkWindow *root;

      gtk_window_get_position (GTK_WINDOW (dialog), &saved_x, &saved_y);
      gtk_window_get_size (GTK_WINDOW (dialog), &saved_w, &saved_h);

      root = gdk_screen_get_root_window (screen);
      saved = gdk_pixmap_new (GDK_DRAWABLE (root), saved_w, saved_h, -1);
      saved_gc = gdk_gc_new (GDK_DRAWABLE (saved));
      gdk_gc_set_function (saved_gc, GDK_COPY);
      gdk_gc_set_subwindow (saved_gc, TRUE);

      gdk_draw_drawable (GDK_DRAWABLE (saved), saved_gc, GDK_DRAWABLE (root),
                         saved_x, saved_y, 0, 0, saved_w, saved_h);
    }

	/* need to realize the dialog first! */
	gtk_widget_show_now (dialog);
  gtk_widget_grab_focus (ok_button);

	/* Grab Keyboard and Mouse pointer */
  if (!accessibility) 
    xfsm_window_grab_input (GTK_WINDOW (dialog));

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

  gtk_widget_hide (dialog);

  /* ask password */
  if (result == GTK_RESPONSE_OK && *shutdownType != SHUTDOWN_LOGOUT
      && xfsm_shutdown_helper_need_password (shutdown_helper))
    {
      GdkWindow *root;

      root = gdk_screen_get_root_window (screen);

      /* clear the area occupied by the dialog first */
      if (!accessibility)
        {
          gdk_draw_drawable (GDK_DRAWABLE (root), saved_gc, GDK_DRAWABLE(saved),
                             0, 0, saved_x, saved_y, saved_w, saved_h);
        }

      gtk_container_remove (GTK_CONTAINER (vbox), radio_vbox);

      entry_vbox = gtk_vbox_new (FALSE, BORDER);
      gtk_box_pack_start (GTK_BOX (vbox), entry_vbox, TRUE, TRUE, 0);
      gtk_widget_show (entry_vbox);

#if GTK_CHECK_VERSION(2,4,0)
      gtk_image_set_from_stock (GTK_IMAGE (image),
                                GTK_STOCK_DIALOG_AUTHENTICATION,
                                GTK_ICON_SIZE_DIALOG);
#endif

      gtk_label_set_text (GTK_LABEL (label), _("Please enter your password:"));

      entry = gtk_entry_new ();
      gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
      gtk_box_pack_start (GTK_BOX (entry_vbox), entry, TRUE, TRUE, 0);
      g_signal_connect (G_OBJECT (entry), "activate",
                        G_CALLBACK (entry_activate_cb), dialog);
      gtk_widget_show (entry);

      /* center dialog on target monitor */
      xfsm_center_window_on_screen (GTK_WINDOW (dialog), screen, monitor);

      /* again, save the area below the dialog */
      if (!accessibility)
        {
          g_object_unref (G_OBJECT (saved));

          gtk_window_get_position (GTK_WINDOW (dialog), &saved_x, &saved_y);
          gtk_window_get_size (GTK_WINDOW (dialog), &saved_w, &saved_h);

          saved = gdk_pixmap_new (GDK_DRAWABLE (root), saved_w, saved_h, -1);
          gdk_draw_drawable (GDK_DRAWABLE (saved), saved_gc, GDK_DRAWABLE(root),
                             saved_x, saved_y, 0, 0, saved_w, saved_h);
        }

      gtk_widget_show_now (dialog);
      gtk_widget_grab_focus (entry);

      /* Grab Keyboard and Mouse pointer */
      if (!accessibility) 
        xfsm_window_grab_input (GTK_WINDOW (dialog));

      result = gtk_dialog_run (GTK_DIALOG (dialog));

      if (result == GTK_RESPONSE_OK)
        {
          const gchar *pw = gtk_entry_get_text (GTK_ENTRY (entry));

          if (!xfsm_shutdown_helper_send_password (shutdown_helper, pw))
            {
              gtk_widget_hide (dialog);

              /* clear the area occupied by the dialog first */
              if (!accessibility)
                {
                  gdk_draw_drawable (GDK_DRAWABLE (root), saved_gc,
                                     GDK_DRAWABLE(saved), 0, 0,
                                     saved_x, saved_y, saved_w, saved_h);
                }

              gtk_image_set_from_stock (GTK_IMAGE (image),
                                        GTK_STOCK_DIALOG_ERROR,
                                        GTK_ICON_SIZE_DIALOG);

              gtk_label_set_text (GTK_LABEL (label),
                                  _("<b>An error occured</b>"));
              gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

              gtk_container_remove (GTK_CONTAINER (
                    GTK_DIALOG (dialog)->action_area), cancel_button);
              gtk_container_remove (GTK_CONTAINER (
                    GTK_DIALOG (dialog)->action_area), ok_button);

              gtk_container_remove (GTK_CONTAINER (entry_vbox), entry);

              gtk_dialog_add_button (GTK_DIALOG (dialog),
                                     GTK_STOCK_CLOSE,
                                     GTK_RESPONSE_CANCEL);

              label = gtk_label_new (_("Either the password you entered is "
                                       "invalid, or the system administrator "
                                       "disallows shutting down this computer "
                                       "with your user account."));
              gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
              gtk_box_pack_start (GTK_BOX (entry_vbox), label, TRUE, TRUE, 0);
              gtk_widget_show (label);

              /* center dialog on target monitor */
              xfsm_center_window_on_screen (GTK_WINDOW (dialog), screen,
                                            monitor);

              gtk_widget_show_now (dialog);

              /* Grab Keyboard and Mouse pointer */
              if (!accessibility) 
                xfsm_window_grab_input (GTK_WINDOW (dialog));

              gtk_dialog_run (GTK_DIALOG (dialog));

              result = GTK_RESPONSE_CANCEL;
            }
        }
    }

	gtk_widget_destroy(dialog);
  gtk_widget_destroy(hidden);

	/* Release Keyboard/Mouse pointer grab */
  if (!accessibility)
    {
      xfsm_fadeout_destroy (fadeout);

      if (saved != NULL)
        {
          g_object_unref (G_OBJECT (saved_gc));
          g_object_unref (G_OBJECT (saved));
        }

      gdk_x11_ungrab_server ();
      gdk_pointer_ungrab (GDK_CURRENT_TIME);
      gdk_keyboard_ungrab (GDK_CURRENT_TIME);
      gdk_flush ();
    }

  /* process all pending events first */
  while (gtk_events_pending ())
    g_main_context_iteration (NULL, FALSE);

  /*
   * remember the current settings.
   */
  if (result == GTK_RESPONSE_OK)
    {
      xfce_rc_set_group (rc, "General");
      xfce_rc_write_entry (rc, "SessionName", session_name);
      xfce_rc_write_bool_entry (rc, "SaveOnExit", *saveSession);
    }

#ifdef SESSION_SCREENSHOTS
  if (screenshot_pm != NULL)
    {
      if (result == GTK_RESPONSE_OK)
        screenshot_save (screenshot_pm, &screenshot_area);

      g_object_unref (G_OBJECT (screenshot_pm));
    }
#endif

  xfce_rc_close (rc);

	return (result == GTK_RESPONSE_OK);
}


/*
 */
gint
xfsm_shutdown(gint type)
{
  gboolean result;

#ifdef HAVE_GNOME
  if (compat_gnome)
    xfsm_compat_gnome_shutdown ();
#endif

  if (compat_kde)
    xfsm_compat_kde_shutdown ();

  /* kill legacy clients */
  xfsm_legacy_shutdown ();

#ifdef HAVE_SYNC
  /* sync disk block in-core status with that on disk */
  sync ();
#endif

  if (type == SHUTDOWN_LOGOUT)
    return EXIT_SUCCESS;

  if (shutdown_helper == NULL)
    {
      g_warning ("No shutdown helper attached!");
      return EXIT_FAILURE;
    }

  if (type == SHUTDOWN_HALT)
    {
      result = xfsm_shutdown_helper_send_command (shutdown_helper,
                                                  XFSM_SHUTDOWN_POWEROFF);
    }
  else
    {
      result = xfsm_shutdown_helper_send_command (shutdown_helper,
                                                  XFSM_SHUTDOWN_REBOOT);
    }

  xfsm_shutdown_helper_destroy (shutdown_helper);
  shutdown_helper = NULL;

  if (!result)
    {
      /* XXX - graphical feedback ?! */
      g_warning ("Failed to perform shutdown action!");
      return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}

