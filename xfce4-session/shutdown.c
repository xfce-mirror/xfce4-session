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

#ifdef HAVE_GETPWUID
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#endif

#ifdef HAVE_ASM_UNISTD_H
#include <asm/unistd.h>  /* for __NR_ioprio_set */
#endif

#include <libxfce4util/libxfce4util.h>
#include <gtk/gtk.h>

#include <libxfsm/xfsm-util.h>

#include <xfce4-session/shutdown.h>
#include <xfce4-session/xfsm-compat-gnome.h>
#include <xfce4-session/xfsm-compat-kde.h>
#include <xfce4-session/xfsm-fadeout.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-legacy.h>
#include <xfce4-session/xfsm-shutdown-helper.h>

#define BORDER    6


static XfsmShutdownHelper *shutdown_helper = NULL;

static GtkWidget *shutdown_dialog = NULL;

#ifdef SESSION_SCREENSHOTS
static void
screenshot_save (const gchar *session_name, GdkPixmap *pm, GdkRectangle *area)
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
          display_name = xfsm_gdk_display_get_fullname (dpy);
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

static void
logout_button_clicked (GtkWidget *b, gint *shutdownType)
{
    *shutdownType = XFSM_SHUTDOWN_LOGOUT;

    gtk_dialog_response (GTK_DIALOG (shutdown_dialog), GTK_RESPONSE_OK);
}

static void
reboot_button_clicked (GtkWidget *b, gint *shutdownType)
{
    *shutdownType = XFSM_SHUTDOWN_REBOOT;

    gtk_dialog_response (GTK_DIALOG (shutdown_dialog), GTK_RESPONSE_OK);
}

static void
halt_button_clicked (GtkWidget *b, gint *shutdownType)
{
    *shutdownType = XFSM_SHUTDOWN_HALT;

    gtk_dialog_response (GTK_DIALOG (shutdown_dialog), GTK_RESPONSE_OK);
}

static void
suspend_button_clicked (GtkWidget *b, gint *shutdownType)
{
    *shutdownType = XFSM_SHUTDOWN_SUSPEND;

    gtk_dialog_response (GTK_DIALOG (shutdown_dialog), GTK_RESPONSE_OK);
}

static void
hibernate_button_clicked (GtkWidget *b, gint *shutdownType)
{
    *shutdownType = XFSM_SHUTDOWN_HIBERNATE;

    gtk_dialog_response (GTK_DIALOG (shutdown_dialog), GTK_RESPONSE_OK);
}

/*
 */
gboolean
shutdownDialog(const gchar *sessionName, XfsmShutdownType *shutdownType, gboolean *saveSession)
{
  gboolean accessibility;
  GtkIconTheme *icon_theme;
  XfsmFadeout *fadeout = NULL;
  GdkScreen *screen;
  GtkWidget *dialog;
  GtkWidget *label;
  GtkWidget *dbox;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *image;
  GtkWidget *checkbox;
  GtkWidget *entry_vbox;
  GtkWidget *entry;
  GtkWidget *hidden;
  GtkWidget *logout_button;
  GtkWidget *reboot_button;
  GtkWidget *halt_button;
  GtkWidget *suspend_button = NULL;
  GtkWidget *hibernate_button = NULL;
  GtkWidget *cancel_button;
  GtkWidget *ok_button;
  GdkPixbuf *icon;
  gboolean saveonexit;
  gboolean autosave;
  gboolean prompt;
  gboolean show_suspend;
  gboolean show_hibernate;

  gboolean show_restart;
  gboolean show_shutdown;

  gboolean require_password;

  gint monitor;
  gint result;
  XfceKiosk *kiosk;
  gboolean kiosk_can_shutdown;
  gboolean kiosk_can_save_session;
  XfconfChannel *channel;
#ifdef SESSION_SCREENSHOTS
  GdkRectangle screenshot_area;
  GdkWindow *root;
  GdkPixmap *screenshot_pm = NULL;
  GdkGC *screenshot_gc;
#endif
#ifdef HAVE_GETPWUID
  struct passwd *pw;
#endif

  g_return_val_if_fail(saveSession != NULL, FALSE);
  g_return_val_if_fail(shutdownType != NULL, FALSE);

  icon_theme = gtk_icon_theme_get_default ();

  /* destroy any previously running shutdown helper first */
  if (shutdown_helper != NULL)
    {
      g_object_unref (shutdown_helper);
      shutdown_helper = NULL;
    }

  /* load kiosk settings */
  kiosk = xfce_kiosk_new ("xfce4-session");
  kiosk_can_shutdown = xfce_kiosk_query (kiosk, "Shutdown");
  kiosk_can_save_session = xfce_kiosk_query (kiosk, "SaveSession");
  xfce_kiosk_free (kiosk);

  /* load configuration */
  channel = xfsm_open_config ();
  channel = xfconf_channel_get ("xfce4-session");
  saveonexit = xfconf_channel_get_bool (channel, "/general/SaveOnExit", TRUE);
  autosave = xfconf_channel_get_bool (channel, "/general/AutoSave", FALSE);
  prompt = xfconf_channel_get_bool (channel, "/general/PromptOnLogout", TRUE);
  show_suspend = xfconf_channel_get_bool (channel, "/shutdown/ShowSuspend", TRUE);
  show_hibernate = xfconf_channel_get_bool (channel, "/shutdown/ShowHibernate", TRUE);

  /* make the session-save settings obey the kiosk settings */
  if (!kiosk_can_save_session)
    {
      saveonexit = FALSE;
      autosave = FALSE;
    }

  /* if PromptOnLogout is off, saving depends on AutoSave */
  if (!prompt)
    {
      *shutdownType = XFSM_SHUTDOWN_LOGOUT;
      *saveSession = autosave;

      return TRUE;
    }

  /* spawn the helper early so we know what it supports when
   * constructing the dialog */
  shutdown_helper = xfsm_shutdown_helper_new ();

  /* It's really bad here if someone else has the pointer
   * grabbed, so we first grab the pointer and keyboard
   * to an offscreen window, and then once we have the
   * server grabbed, move that to our dialog.
   */
  gtk_rc_reparse_all ();

  /* get screen with pointer */
  screen = xfce_gdk_screen_get_active (&monitor);
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
          if (gdk_pointer_grab (hidden->window, TRUE, 0, NULL, NULL,
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
      fadeout = xfsm_fadeout_new (gtk_widget_get_display (hidden));
      gdk_flush ();

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

  shutdown_dialog = dialog;

  cancel_button = gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL,
                                         GTK_RESPONSE_CANCEL);

  ok_button = gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_OK,
                                     GTK_RESPONSE_OK);

  gtk_widget_hide (ok_button);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  gtk_window_set_screen (GTK_WINDOW (dialog), screen);

  dbox = GTK_DIALOG(dialog)->vbox;

  vbox = gtk_vbox_new(FALSE, BORDER);
  gtk_box_pack_start(GTK_BOX(dbox), vbox, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);
  gtk_widget_show(vbox);

#ifdef HAVE_GETPWUID
  pw = getpwuid (getuid ());
  if (G_LIKELY(pw && pw->pw_name && *pw->pw_name))
    {
      gchar *text = g_strdup_printf (_("<span size='large'><b>Log out %s</b></span>"), pw->pw_name);
      GtkWidget *logout_label = g_object_new (GTK_TYPE_LABEL,
                                             "label", text,
                                             "use-markup", TRUE,
                                             "justify", GTK_JUSTIFY_CENTER,
                                             "xalign", 0.5,
                                             "yalign", 0.5,
                                             NULL);

      gtk_widget_show (logout_label);
      gtk_box_pack_start (GTK_BOX (vbox), logout_label, FALSE, FALSE, 0);

      g_free (text);
    }
#endif

  hbox = gtk_hbox_new (TRUE, BORDER);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  /* logout */
  logout_button = gtk_button_new ();
  gtk_widget_show (logout_button);
  gtk_box_pack_start (GTK_BOX (hbox), logout_button, TRUE, TRUE, 0);

  g_signal_connect (logout_button, "clicked",
                    G_CALLBACK (logout_button_clicked), shutdownType);

  vbox2 = gtk_vbox_new (FALSE, BORDER);
  gtk_container_set_border_width (GTK_CONTAINER (vbox2), BORDER);
  gtk_widget_show (vbox2);
  gtk_container_add (GTK_CONTAINER (logout_button), vbox2);

  icon = gtk_icon_theme_load_icon (icon_theme,
                                   "system-log-out",
                                   32,
                                   GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                   NULL);
  if (!icon)
    icon = gtk_icon_theme_load_icon (icon_theme,
                                     "xfsm-logout",
                                     32,
                                     GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                     NULL);

  image = gtk_image_new_from_pixbuf (icon);
  gtk_widget_show (image);
  gtk_box_pack_start (GTK_BOX (vbox2), image, FALSE, FALSE, 0);
  g_object_unref (icon);

  label = gtk_label_new (_("Log Out"));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, FALSE, 0);

  /* reboot */
  reboot_button = gtk_button_new ();
  gtk_widget_show (reboot_button);
  gtk_box_pack_start (GTK_BOX (hbox), reboot_button, TRUE, TRUE, 0);

  g_signal_connect (reboot_button, "clicked",
                    G_CALLBACK (reboot_button_clicked), shutdownType);

  vbox2 = gtk_vbox_new (FALSE, BORDER);
  gtk_container_set_border_width (GTK_CONTAINER (vbox2), BORDER);
  gtk_widget_show (vbox2);
  gtk_container_add (GTK_CONTAINER (reboot_button), vbox2);

  icon = gtk_icon_theme_load_icon (icon_theme,
                                   "system-reboot",
                                   32,
                                   GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                   NULL);

  if (!icon)
    icon = gtk_icon_theme_load_icon (icon_theme,
                                     "xfsm-reboot",
                                     32,
                                     GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                     NULL);

  image = gtk_image_new_from_pixbuf (icon);
  gtk_widget_show (image);
  gtk_box_pack_start (GTK_BOX (vbox2), image, FALSE, FALSE, 0);
  g_object_unref (icon);

  label = gtk_label_new (_("Restart"));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, FALSE, 0);

  g_object_get (shutdown_helper,
                "user-can-restart", &show_restart,
                NULL);

  if (!kiosk_can_shutdown || !show_restart )
    gtk_widget_set_sensitive (reboot_button, FALSE);

  /* halt */
  halt_button = gtk_button_new ();
  gtk_widget_show (halt_button);
  gtk_box_pack_start (GTK_BOX (hbox), halt_button, TRUE, TRUE, 0);

  g_signal_connect (halt_button, "clicked",
                    G_CALLBACK (halt_button_clicked), shutdownType);

  vbox2 = gtk_vbox_new (FALSE, BORDER);
  gtk_container_set_border_width (GTK_CONTAINER (vbox2), BORDER);
  gtk_widget_show (vbox2);
  gtk_container_add (GTK_CONTAINER (halt_button), vbox2);

  icon = gtk_icon_theme_load_icon (icon_theme,
                                   "system-shutdown",
                                   32,
                                   GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                   NULL);

  if (!icon)
    icon = gtk_icon_theme_load_icon (icon_theme,
                                     "xfsm-shutdown",
                                     32,
                                     GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                     NULL);

  image = gtk_image_new_from_pixbuf (icon);
  gtk_widget_show (image);
  gtk_box_pack_start (GTK_BOX (vbox2), image, FALSE, FALSE, 0);
  g_object_unref (icon);

  label = gtk_label_new (_("Shut Down"));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, FALSE, 0);

  g_object_get (shutdown_helper,
                "user-can-shutdown", &show_shutdown,
                NULL);

  if (!kiosk_can_shutdown || !show_shutdown)
    gtk_widget_set_sensitive (halt_button, FALSE);

  if (show_suspend)
    g_object_get (shutdown_helper,
                  "user-can-suspend", &show_suspend,
                  NULL);

  if (show_hibernate)
    g_object_get (shutdown_helper,
                  "user-can-hibernate", &show_hibernate,
                  NULL);


  if (kiosk_can_shutdown && (show_suspend || show_hibernate))
    {
      hbox = gtk_hbox_new (FALSE, BORDER);
      gtk_widget_show (hbox);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    }

  /* suspend */
  if (kiosk_can_shutdown && show_suspend)
    {
      suspend_button = gtk_button_new ();
      gtk_widget_show (suspend_button);
      gtk_box_pack_start (GTK_BOX (hbox), suspend_button, TRUE, TRUE, 0);

      g_signal_connect (suspend_button, "clicked",
                        G_CALLBACK (suspend_button_clicked), shutdownType);

      vbox2 = gtk_vbox_new (FALSE, BORDER);
      gtk_container_set_border_width (GTK_CONTAINER (vbox2), BORDER);
      gtk_widget_show (vbox2);
      gtk_container_add (GTK_CONTAINER (suspend_button), vbox2);

      icon = gtk_icon_theme_load_icon (icon_theme,
                                       "system-suspend",
                                       32,
                                       GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                       NULL);

      if (!icon)
        icon = gtk_icon_theme_load_icon (icon_theme,
                                         "xfsm-suspend",
                                         32,
                                         GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                         NULL);

      image = gtk_image_new_from_pixbuf (icon);
      gtk_widget_show (image);
      gtk_box_pack_start (GTK_BOX (vbox2), image, FALSE, FALSE, 0);
      g_object_unref (icon);

      label = gtk_label_new (_("Suspend"));
      gtk_widget_show (label);
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, FALSE, 0);
    }

  /* hibernate */
  if (kiosk_can_shutdown && show_hibernate)
    {
      hibernate_button = gtk_button_new ();
      gtk_widget_show (hibernate_button);
      gtk_box_pack_start (GTK_BOX (hbox), hibernate_button, TRUE, TRUE, 0);

      g_signal_connect (hibernate_button, "clicked",
                        G_CALLBACK (hibernate_button_clicked), shutdownType);

      vbox2 = gtk_vbox_new (FALSE, BORDER);
      gtk_container_set_border_width (GTK_CONTAINER (vbox2), BORDER);
      gtk_widget_show (vbox2);
      gtk_container_add (GTK_CONTAINER (hibernate_button), vbox2);

      icon = gtk_icon_theme_load_icon (icon_theme,
                                       "system-hibernate",
                                       32,
                                       GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                       NULL);

      if (!icon)
        icon = gtk_icon_theme_load_icon (icon_theme,
                                         "xfsm-hibernate",
                                         32,
                                         GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                         NULL);

      image = gtk_image_new_from_pixbuf (icon);
      gtk_widget_show (image);
      gtk_box_pack_start (GTK_BOX (vbox2), image, FALSE, FALSE, 0);
      g_object_unref (icon);

      label = gtk_label_new (_("Hibernate"));
      gtk_widget_show (label);
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, FALSE, 0);
  }

  /* save session */
  if (!autosave && kiosk_can_save_session)
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
  gtk_window_set_screen (GTK_WINDOW (dialog), screen);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

  /* save portion of the root window covered by the dialog */
  if (!accessibility && shutdown_helper != NULL)
    {
      gtk_widget_realize (dialog);
      gdk_window_set_override_redirect (dialog->window, TRUE);
      gdk_window_raise (dialog->window);
    }

  /* need to realize the dialog first! */
  gtk_widget_show_now (dialog);
  gtk_widget_grab_focus (logout_button);

  /* Grab Keyboard and Mouse pointer */
  if (!accessibility)
    xfsm_window_grab_input (GTK_WINDOW (dialog));

  /* run the logout dialog */
  result = gtk_dialog_run (GTK_DIALOG(dialog));

  if (result == GTK_RESPONSE_OK) {
    *saveSession = autosave ? autosave :
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox));
  }

  gtk_widget_hide (dialog);

  g_object_get (shutdown_helper,
                "require-password", &require_password,
                NULL);

  /* ask password */
  if (result == GTK_RESPONSE_OK && *shutdownType != XFSM_SHUTDOWN_LOGOUT
      && require_password )
    {
      gtk_widget_show (ok_button);

      gtk_widget_destroy (vbox);

      entry_vbox = gtk_vbox_new (FALSE, BORDER);
      gtk_box_pack_start (GTK_BOX (dbox), entry_vbox, TRUE, TRUE, BORDER);
      gtk_widget_show (entry_vbox);

      label = gtk_label_new (_("Please enter your password:"));
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_widget_show (label);
      gtk_box_pack_start (GTK_BOX (entry_vbox), label, FALSE, FALSE, 0);

      entry = gtk_entry_new ();
      gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
      gtk_box_pack_start (GTK_BOX (entry_vbox), entry, TRUE, TRUE, 0);
      g_signal_connect (G_OBJECT (entry), "activate",
                        G_CALLBACK (entry_activate_cb), dialog);
      gtk_widget_show (entry);

      /* center dialog on target monitor */
      gtk_window_set_screen (GTK_WINDOW (dialog), screen);
      gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

      gtk_widget_show_now (dialog);
      gtk_widget_grab_focus (entry);

      /* Grab Keyboard and Mouse pointer */
      if (!accessibility)
        xfsm_window_grab_input (GTK_WINDOW (dialog));

      result = gtk_dialog_run (GTK_DIALOG (dialog));

      if (result == GTK_RESPONSE_OK)
        {
          const gchar *password = gtk_entry_get_text (GTK_ENTRY (entry));

          if (!xfsm_shutdown_helper_send_password (shutdown_helper, password))
            {
              gtk_label_set_text (GTK_LABEL (label),
                                  _("<b>An error occurred</b>"));
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
              gtk_window_set_screen (GTK_WINDOW (dialog), screen);
              gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

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

  shutdown_dialog = NULL;

  /* Release Keyboard/Mouse pointer grab */
  if (!accessibility)
    {
      xfsm_fadeout_destroy (fadeout);

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
      xfconf_channel_set_string (channel, "/general/SessionName", sessionName);
      xfconf_channel_set_bool (channel, "/general/SaveOnExit", *saveSession);
    }
  else
    {
      g_object_unref (shutdown_helper);
      shutdown_helper = NULL;
    }

#ifdef SESSION_SCREENSHOTS
  if (screenshot_pm != NULL)
    {
      if (result == GTK_RESPONSE_OK)
        screenshot_save (sessionName, screenshot_pm, &screenshot_area);

      g_object_unref (G_OBJECT (screenshot_pm));
    }
#endif

  return (result == GTK_RESPONSE_OK);
}


/*
 */
gint
xfsm_shutdown(XfsmShutdownType type)
{
  gboolean result;
  GError *error = NULL;

  /* kludge */
  if (type == XFSM_SHUTDOWN_ASK)
    {
      g_warning ("xfsm_shutdown () passed XFSM_SHUTDOWN_ASK.  This is a bug.");
      type = XFSM_SHUTDOWN_LOGOUT;
    }

  /* these two remember if they were started or not */
  xfsm_compat_gnome_shutdown ();
  xfsm_compat_kde_shutdown ();

  /* kill legacy clients */
  xfsm_legacy_shutdown ();

#if !defined(__NR_ioprio_set) && defined(HAVE_SYNC)
  /* sync disk block in-core status with that on disk.  if
   * we have ioprio_set (), then we've already synced. */
  if (fork () == 0)
    {
# ifdef HAVE_SETSID
      setsid ();
# endif
      sync ();
      _exit (EXIT_SUCCESS);
    }
#endif  /* HAVE_SYNC */

  if (type == XFSM_SHUTDOWN_LOGOUT)
    return EXIT_SUCCESS;

  if (shutdown_helper == NULL)
    shutdown_helper = xfsm_shutdown_helper_new ();

  result = xfsm_shutdown_helper_send_command (shutdown_helper, type, &error);
  g_object_unref (shutdown_helper);
  shutdown_helper = NULL;

  if (!result)
    {
      xfce_message_dialog (NULL, _("Shutdown Failed"),
                           GTK_STOCK_DIALOG_ERROR,
                           _("Unable to perform shutdown"),
                           error->message,
                           GTK_STOCK_QUIT, GTK_RESPONSE_ACCEPT,
                           NULL);
      g_error_free (error);
      return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}

