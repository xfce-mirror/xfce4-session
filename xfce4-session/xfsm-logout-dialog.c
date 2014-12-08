/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2011      Nick Schermer <nick@xfce.org>
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
#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <gtk/gtk.h>

#include <libxfsm/xfsm-util.h>

#include <xfce4-session/xfsm-logout-dialog.h>
#include <xfce4-session/xfsm-fadeout.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-legacy.h>
#include <xfce4-session/xfsm-error.h>

#ifdef GDK_WINDOWING_X11
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#endif



#define BORDER   6
#define SHOTSIZE 64



static void       xfsm_logout_dialog_finalize (GObject          *object);
static GtkWidget *xfsm_logout_dialog_button   (const gchar      *title,
                                               const gchar      *icon_name,
                                               const gchar      *icon_name_fallback,
                                               XfsmShutdownType  type,
                                               XfsmLogoutDialog *dialog);



enum
{
  MODE_LOGOUT_BUTTONS,
  MODE_SHOW_ERROR,
  N_MODES
};

struct _XfsmLogoutDialogClass
{
  GtkDialogClass __parent__;
};

struct _XfsmLogoutDialog
{
  GtkDialog __parent__;

  /* set when a button is clicked */
  XfsmShutdownType  type_clicked;

  /* save session checkbox */
  GtkWidget        *save_session;

  /* mode widgets */
  GtkWidget        *box[N_MODES];

  /* dialog buttons */
  GtkWidget        *button_cancel;
  GtkWidget        *button_ok;
  GtkWidget        *button_close;

  /* error label */
  GtkWidget        *error_label;

  /* pm instance */
  XfsmShutdown     *shutdown;
};



G_DEFINE_TYPE (XfsmLogoutDialog, xfsm_logout_dialog, GTK_TYPE_DIALOG)



static void
xfsm_logout_dialog_class_init (XfsmLogoutDialogClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfsm_logout_dialog_finalize;
}



static void
xfsm_logout_dialog_init (XfsmLogoutDialog *dialog)
{
  const gchar   *username;
  GtkWidget     *label;
  gchar         *label_str;
  PangoAttrList *attrs;
  GtkWidget     *vbox;
  GtkWidget     *button_vbox;
  GtkWidget     *main_vbox;
  GtkWidget     *hbox;
  GtkWidget     *button;
  gboolean       can_shutdown;
  gboolean       save_session = FALSE;
  gboolean       can_restart;
  gboolean       can_suspend = FALSE;
  gboolean       can_hibernate = FALSE;
  gboolean       auth_suspend = FALSE;
  gboolean       auth_hibernate = FALSE;
  GError        *error = NULL;
  XfconfChannel *channel;
  GtkWidget     *image;
  GtkWidget     *separator;
  gboolean       upower_not_found = FALSE;

  dialog->type_clicked = XFSM_SHUTDOWN_LOGOUT;
  dialog->shutdown = xfsm_shutdown_get ();

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  /* load xfconf settings */
  channel = xfsm_open_config ();
  if (xfsm_shutdown_can_save_session (dialog->shutdown))
    save_session = xfconf_channel_get_bool (channel, "/general/SaveOnExit", TRUE);

  main_vbox = gtk_vbox_new (FALSE, BORDER);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), main_vbox, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), BORDER);
  gtk_widget_show (main_vbox);

  /* label showing the users' name */
  username = g_get_real_name ();
  if (username == NULL
      || *username == '\0'
      || strcmp ("Unknown", username) == 0)
    username = g_get_user_name ();

  label_str = g_strdup_printf (_("Log out %s"), username);
  label = gtk_label_new (label_str);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
  gtk_box_pack_start (GTK_BOX (main_vbox), label, FALSE, TRUE, 0);
  gtk_widget_show (label);
  g_free (label_str);

  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_scale_new (PANGO_SCALE_LARGE));
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  pango_attr_list_unref (attrs);

  separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (main_vbox), separator, FALSE, TRUE, 0);
  gtk_widget_show (separator);

  /**
   * Start mode MODE_LOGOUT_BUTTONS
   **/
  dialog->box[MODE_LOGOUT_BUTTONS] = vbox = gtk_vbox_new (FALSE, BORDER);
  gtk_box_pack_start (GTK_BOX (main_vbox), vbox, TRUE, TRUE, 0);

  /**
   * Cancel
   **/
  dialog->button_cancel = gtk_dialog_add_button (GTK_DIALOG (dialog),
                                                 GTK_STOCK_CANCEL,
                                                 GTK_RESPONSE_CANCEL);

  button_vbox = gtk_vbox_new (TRUE, BORDER);
  gtk_box_pack_start (GTK_BOX (vbox), button_vbox, FALSE, TRUE, 0);
  gtk_widget_show (button_vbox);

  /* row for logout/shutdown and reboot */
  hbox = gtk_hbox_new (TRUE, BORDER);
  gtk_box_pack_start (GTK_BOX (button_vbox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);

  /**
   * Logout
   **/
  button = xfsm_logout_dialog_button (_("_Log Out"), "system-log-out",
                                      "xfsm-logout", XFSM_SHUTDOWN_LOGOUT,
                                      dialog);

  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  gtk_widget_show (button);
  gtk_widget_grab_focus (button);

  /**
   * Reboot
   **/
  if (!xfsm_shutdown_can_restart (dialog->shutdown, &can_restart, &error))
    {
      g_printerr ("%s: Querying CanRestart failed, %s\n\n",
                  PACKAGE_NAME, ERROR_MSG (error));
      g_clear_error (&error);

      can_restart = FALSE;
    }

  button = xfsm_logout_dialog_button (_("_Restart"), "system-reboot",
                                      "xfsm-reboot", XFSM_SHUTDOWN_RESTART,
                                      dialog);

  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  gtk_widget_set_sensitive (button, can_restart);
  gtk_widget_show (button);

  /**
   * Shutdown
   **/
  if (!xfsm_shutdown_can_shutdown (dialog->shutdown, &can_shutdown, &error))
    {
      g_printerr ("%s: Querying CanShutdown failed. %s\n\n",
                  PACKAGE_NAME, ERROR_MSG (error));
      g_clear_error (&error);

      can_shutdown = FALSE;
    }

  button = xfsm_logout_dialog_button (_("Shut _Down"), "system-shutdown",
                                      "xfsm-shutdown", XFSM_SHUTDOWN_SHUTDOWN,
                                      dialog);

  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  gtk_widget_set_sensitive (button, can_shutdown);
  gtk_widget_show (button);

  /* new row for suspend/hibernate */
  hbox = gtk_hbox_new (TRUE, BORDER);
  gtk_box_pack_start (GTK_BOX (button_vbox), hbox, FALSE, TRUE, 0);

  /**
   * Suspend
   *
   * Hide the button if UPower is not installed or system cannot suspend
   **/
  if (xfconf_channel_get_bool (channel, "/shutdown/ShowSuspend", TRUE))
    {
      if (xfsm_shutdown_can_suspend (dialog->shutdown, &can_suspend, &auth_suspend, &error))
        {
          if (can_suspend)
            {
              button = xfsm_logout_dialog_button (_("Sus_pend"), "system-suspend",
                                                  "xfsm-suspend", XFSM_SHUTDOWN_SUSPEND,
                                                  dialog);

              gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
              gtk_widget_set_sensitive (button, auth_suspend);
              gtk_widget_show (button);

              gtk_widget_show (hbox);
            }
        }
      else
        {
          g_printerr ("%s: Querying suspend failed: %s\n\n",
                      PACKAGE_NAME, ERROR_MSG (error));
          g_clear_error (&error);

          /* don't try hibernate again */
          upower_not_found = TRUE;
        }
    }

  /**
   * Hibernate
   *
   * Hide the button if UPower is not installed or system cannot suspend
   **/
  if (!upower_not_found
      && xfconf_channel_get_bool (channel, "/shutdown/ShowHibernate", TRUE))
    {
      if (xfsm_shutdown_can_hibernate (dialog->shutdown, &can_hibernate, &auth_hibernate, &error))
        {
          if (can_hibernate)
            {
              button = xfsm_logout_dialog_button (_("_Hibernate"), "system-hibernate",
                                                  "xfsm-hibernate", XFSM_SHUTDOWN_HIBERNATE,
                                                  dialog);

              gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
              gtk_widget_set_sensitive (button, auth_hibernate);
              gtk_widget_show (button);

              gtk_widget_show (hbox);
            }
        }
      else
        {
          g_printerr ("%s: Querying hibernate failed: %s\n\n",
                      PACKAGE_NAME, ERROR_MSG (error));
          g_clear_error (&error);
        }
    }

  /**
   * Save session
   **/
  if (xfsm_shutdown_can_save_session (dialog->shutdown)
      && !xfconf_channel_get_bool (channel, "/general/AutoSave", FALSE))
    {
      dialog->save_session = gtk_check_button_new_with_mnemonic (_("_Save session for future logins"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->save_session), save_session);
      gtk_box_pack_start (GTK_BOX (vbox), dialog->save_session, FALSE, TRUE, BORDER);
      gtk_widget_show (dialog->save_session);
    }

  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));


  /**
   * Start mode MODE_SHOW_ERROR
   **/
  dialog->box[MODE_SHOW_ERROR] = vbox = gtk_vbox_new (FALSE, BORDER);
  gtk_box_pack_start (GTK_BOX (main_vbox), vbox, TRUE, TRUE, 0);

  hbox = gtk_hbox_new (FALSE, BORDER * 2);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);

  image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_ERROR, GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  vbox = gtk_vbox_new (FALSE, BORDER);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  label = gtk_label_new (_("An error occurred"));
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0.00, 0.50);
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  gtk_widget_show (label);

  pango_attr_list_unref (attrs);
}



static void
xfsm_logout_dialog_finalize (GObject *object)
{
  XfsmLogoutDialog *dialog = XFSM_LOGOUT_DIALOG (object);

  g_object_unref (G_OBJECT (dialog->shutdown));

  (*G_OBJECT_CLASS (xfsm_logout_dialog_parent_class)->finalize) (object);
}



static void
xfsm_logout_dialog_set_mode (XfsmLogoutDialog *dialog,
                             gint              mode)
{
  gint i;

  for (i = 0; i < N_MODES; i++)
    gtk_widget_set_visible (dialog->box[i], i == mode);

  gtk_widget_set_visible (dialog->button_cancel, mode != MODE_SHOW_ERROR);
  gtk_widget_set_visible (dialog->button_close, mode == MODE_SHOW_ERROR);
}



static void
xfsm_logout_dialog_button_clicked (GtkWidget        *button,
                                   XfsmLogoutDialog *dialog)
{
  gint *val;

  val = g_object_get_data (G_OBJECT (button), "shutdown-type");
  g_assert (val != NULL);
  dialog->type_clicked = *val;

  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}



static GtkWidget *
xfsm_logout_dialog_button (const gchar      *title,
                           const gchar      *icon_name,
                           const gchar      *icon_name_fallback,
                           XfsmShutdownType  type,
                           XfsmLogoutDialog *dialog)
{
  GtkWidget    *button;
  GtkWidget    *vbox;
  GdkPixbuf    *pixbuf;
  GtkWidget    *image;
  GtkWidget    *label;
  static gint   icon_size = 0;
  gint          w, h;
  gint         *val;
  GtkIconTheme *icon_theme;

  g_return_val_if_fail (XFSM_IS_LOGOUT_DIALOG (dialog), NULL);

  val = g_new0 (gint, 1);
  *val = type;

  button = gtk_button_new ();
  g_object_set_data_full (G_OBJECT (button), "shutdown-type", val, g_free);
  g_signal_connect (G_OBJECT (button), "clicked",
      G_CALLBACK (xfsm_logout_dialog_button_clicked), dialog);

  vbox = gtk_vbox_new (FALSE, BORDER);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);
  gtk_container_add (GTK_CONTAINER (button), vbox);
  gtk_widget_show (vbox);

  if (G_UNLIKELY (icon_size == 0))
    {
      if (gtk_icon_size_lookup (GTK_ICON_SIZE_DND, &w, &h))
        icon_size = MAX (w, h);
      else
        icon_size = 32;
    }

  icon_theme = gtk_icon_theme_get_for_screen (gtk_window_get_screen (GTK_WINDOW (dialog)));
  pixbuf = gtk_icon_theme_load_icon (icon_theme, icon_name, icon_size, 0, NULL);
  if (G_UNLIKELY (pixbuf == NULL))
    {
      pixbuf = gtk_icon_theme_load_icon (icon_theme, icon_name_fallback,
                                         icon_size, GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                         NULL);
    }

  image = gtk_image_new_from_pixbuf (pixbuf);
  gtk_box_pack_start (GTK_BOX (vbox), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  label = gtk_label_new_with_mnemonic (title);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  return button;
}



static GdkPixbuf *
xfsm_logout_dialog_screenshot_new (GdkScreen *screen)
{
  GdkRectangle  rect, screen_rect;
  GdkWindow    *window;
  GdkPixbuf    *screenshot;
  gint          x, y;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  screen_rect.x = 0;
  screen_rect.y = 0;
  screen_rect.width = gdk_screen_get_width (screen);
  screen_rect.height = gdk_screen_get_height (screen);

  window = gdk_screen_get_root_window (screen);
  gdk_drawable_get_size (GDK_DRAWABLE (window), &rect.width, &rect.height);
  gdk_window_get_origin (window, &x, &y);

  rect.x = x;
  rect.y = y;

  if (!gdk_rectangle_intersect (&rect, &screen_rect, &rect))
    return NULL;

  screenshot = gdk_pixbuf_get_from_drawable  (NULL,
                                             GDK_DRAWABLE (window),
                                             NULL,
                                             0, 0,
                                             0, 0,
                                             rect.width,
                                             rect.height);

  gdk_display_beep (gdk_screen_get_display (screen));

  return screenshot;
}



static GdkPixbuf *
exo_gdk_pixbuf_scale_ratio (GdkPixbuf *source,
                            gint       dest_size)
{
  gdouble wratio;
  gdouble hratio;
  gint    source_width;
  gint    source_height;
  gint    dest_width;
  gint    dest_height;

  g_return_val_if_fail (GDK_IS_PIXBUF (source), NULL);
  g_return_val_if_fail (dest_size > 0, NULL);

  source_width  = gdk_pixbuf_get_width  (source);
  source_height = gdk_pixbuf_get_height (source);

  wratio = (gdouble) source_width  / (gdouble) dest_size;
  hratio = (gdouble) source_height / (gdouble) dest_size;

  if (hratio > wratio)
    {
      dest_width  = rint (source_width / hratio);
      dest_height = dest_size;
    }
  else
    {
      dest_width  = dest_size;
      dest_height = rint (source_height / wratio);
    }

  return gdk_pixbuf_scale_simple (source, MAX (dest_width, 1),
                                  MAX (dest_height, 1), GDK_INTERP_BILINEAR);
}



static void
xfsm_logout_dialog_screenshot_save (GdkPixbuf   *screenshot,
                                    GdkScreen   *screen,
                                    const gchar *session_name)
{
  GdkPixbuf  *scaled;
  gchar      *path;
  gchar      *display_name;
  GdkDisplay *dpy;
  GError     *error = NULL;
  gchar      *filename;

  g_return_if_fail (GDK_IS_PIXBUF (screenshot));
  g_return_if_fail (GDK_IS_SCREEN (screen));

  scaled = exo_gdk_pixbuf_scale_ratio (screenshot, SHOTSIZE);
  if (G_LIKELY (scaled != NULL))
    {
      dpy = gdk_screen_get_display (screen);
      display_name = xfsm_gdk_display_get_fullname (dpy);
      path = g_strconcat ("sessions/thumbs-", display_name, "/", session_name, ".png", NULL);
      filename = xfce_resource_save_location (XFCE_RESOURCE_CACHE, path, TRUE);
      g_free (display_name);
      g_free (path);

      if (!filename)
        {
          g_warning ("Unable to save screenshot, "
                     "error calling xfce_resource_save_location with %s, "
                     "check your permissions", path);
          return;
        }

      if (!gdk_pixbuf_save (scaled, filename, "png", &error, NULL))
        {
          g_warning ("Failed to save session screenshot: %s", error->message);
          g_error_free (error);
        }

      g_free (filename);
      g_object_unref (G_OBJECT (scaled));
    }
}



static gint
xfsm_logout_dialog_run (GtkDialog *dialog,
                        gboolean   grab_input)
{
  GdkWindow *window;
  gint       ret;

  if (grab_input)
    {
      gtk_widget_show_now (GTK_WIDGET (dialog));

      window = gtk_widget_get_window (GTK_WIDGET (dialog));
      if (gdk_keyboard_grab (window, FALSE, GDK_CURRENT_TIME) != GDK_GRAB_SUCCESS)
        g_critical ("Failed to grab the keyboard for logout window");

#ifdef GDK_WINDOWING_X11
      /* force input to the dialog */
      gdk_error_trap_push ();
      XSetInputFocus (GDK_DISPLAY (),
                      GDK_WINDOW_XWINDOW (window),
                      RevertToParent, CurrentTime);
      gdk_error_trap_pop ();
#endif
    }

  ret = gtk_dialog_run (dialog);

  if (grab_input)
    gdk_keyboard_ungrab (GDK_CURRENT_TIME);

  return ret;
}



gboolean
xfsm_logout_dialog (const gchar      *session_name,
                    XfsmShutdownType *return_type,
                    gboolean         *return_save_session)
{
  gint              result;
  GtkWidget        *hidden;
  gboolean          a11y;
  GtkWidget        *dialog;
  GdkScreen        *screen;
  gint              monitor;
  GdkPixbuf        *screenshot = NULL;
  XfsmFadeout      *fadeout = NULL;
  XfsmLogoutDialog *xfsm_dialog;
  XfconfChannel    *channel = xfsm_open_config ();
  gboolean          autosave;
  XfsmShutdown     *shutdown;

  g_return_val_if_fail (return_type != NULL, FALSE);
  g_return_val_if_fail (return_save_session != NULL, FALSE);

  shutdown = xfsm_shutdown_get ();
  if (xfsm_shutdown_can_save_session (shutdown))
    autosave = xfconf_channel_get_bool (channel, "/general/AutoSave", FALSE);
  else
    autosave = FALSE;
  g_object_unref (shutdown);

  /* check if we need to bother the user */
  if (!xfconf_channel_get_bool (channel, "/general/PromptOnLogout", TRUE))
    {
      *return_type = XFSM_SHUTDOWN_LOGOUT;
      *return_save_session = autosave;

      return TRUE;
    }

  /* decide on which screen we should show the dialog */
  screen = xfce_gdk_screen_get_active (&monitor);
  if (G_UNLIKELY (screen == NULL))
    {
      screen = gdk_screen_get_default ();
      monitor = 0;
    }

  /* check if accessibility is enabled */
  hidden = gtk_invisible_new_for_screen (screen);
  gtk_widget_show (hidden);
  a11y = GTK_IS_ACCESSIBLE (gtk_widget_get_accessible (hidden));

  if (G_LIKELY (!a11y))
    {
      /* wait until we can grab the keyboard, we need this for
       * the dialog when running it */
      for (;;)
        {
          if (gdk_keyboard_grab (gtk_widget_get_window (hidden), FALSE,
                                 GDK_CURRENT_TIME) == GDK_GRAB_SUCCESS)
            {
              gdk_keyboard_ungrab (GDK_CURRENT_TIME);
              break;
            }

          g_usleep (G_USEC_PER_SEC / 20);
        }

      /* make a screenshot */
      if (xfconf_channel_get_bool (channel, "/general/ShowScreenshots", TRUE))
        screenshot = xfsm_logout_dialog_screenshot_new (screen);

      /* display fadeout */
      fadeout = xfsm_fadeout_new (gdk_screen_get_display (screen));
      gdk_flush ();

      dialog = g_object_new (XFSM_TYPE_LOGOUT_DIALOG,
                             "type", GTK_WINDOW_POPUP,
                             "screen", screen, NULL);

      xfsm_window_add_border (GTK_WINDOW (dialog));

      gtk_widget_realize (dialog);
      gdk_window_set_override_redirect (dialog->window, TRUE);
      gdk_window_raise (dialog->window);
    }
  else
    {
      dialog = g_object_new (XFSM_TYPE_LOGOUT_DIALOG,
                             "decorated", !a11y,
                             "screen", screen, NULL);

      gtk_window_set_keep_above (GTK_WINDOW (dialog), TRUE);
      atk_object_set_role (gtk_widget_get_accessible (dialog), ATK_ROLE_ALERT);
    }

  gtk_widget_destroy (hidden);

  xfsm_dialog = XFSM_LOGOUT_DIALOG (dialog);

  /* set mode */
  xfsm_logout_dialog_set_mode (xfsm_dialog, MODE_LOGOUT_BUTTONS);

  result = xfsm_logout_dialog_run (GTK_DIALOG (dialog), !a11y);

  gtk_widget_hide (dialog);

  if (result == GTK_RESPONSE_OK)
    {
      /* store autosave state */
      if (autosave)
        *return_save_session = TRUE;
      else if (xfsm_dialog->save_session != NULL)
        *return_save_session = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (xfsm_dialog->save_session));
      else
        *return_save_session = FALSE;

      /* return the clicked action */
      *return_type = xfsm_dialog->type_clicked;
    }

  if (fadeout != NULL)
    xfsm_fadeout_destroy (fadeout);

  gtk_widget_destroy (dialog);

  /* store channel settings if everything worked fine */
  if (result == GTK_RESPONSE_OK)
    {
      xfconf_channel_set_string (channel, "/general/SessionName", session_name);
      xfconf_channel_set_bool (channel, "/general/SaveOnExit", *return_save_session);
    }

  /* save the screenshot */
  if (screenshot != NULL)
    {
      if (result == GTK_RESPONSE_OK)
        xfsm_logout_dialog_screenshot_save (screenshot, screen, session_name);
      g_object_unref (G_OBJECT (screenshot));
    }

  return (result == GTK_RESPONSE_OK);
}
