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
#ifdef HAVE_GTK_LAYER_SHELL
#include <gtk-layer-shell/gtk-layer-shell.h>
#endif

#include <libxfsm/xfsm-util.h>

#include <xfce4-session/xfsm-logout-dialog.h>
#include <xfce4-session/xfsm-fadeout.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-legacy.h>
#include <xfce4-session/xfsm-error.h>

#ifdef ENABLE_X11
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#endif



#define BORDER   6
#define SHOTSIZE 256



static void       xfsm_logout_dialog_finalize (GObject          *object);
static GtkWidget *xfsm_logout_dialog_button   (const gchar      *title,
                                               const gchar      *icon_name,
                                               const gchar      *icon_name_fallback,
                                               const gchar      *icon_name_fallback2,
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
  const gchar    *username;
  GtkWidget      *label;
  gchar          *label_str;
  PangoAttrList  *attrs;
  GtkWidget      *vbox;
  GtkWidget      *button_vbox;
  GtkWidget      *main_vbox;
  GtkWidget      *hbox;
  GtkWidget      *button;
  gboolean        can_shutdown = FALSE;
  gboolean        has_updates;
  gboolean        save_session = FALSE;
  gboolean        can_logout = FALSE;
  gboolean        can_restart = FALSE;
  gboolean        can_suspend = FALSE;
  gboolean        can_hibernate = FALSE;
  gboolean        can_hybrid_sleep = FALSE;
  gboolean        can_switch_user = FALSE;
  gboolean        auth_shutdown = FALSE;
  gboolean        auth_restart = FALSE;
  gboolean        auth_suspend = FALSE;
  gboolean        auth_hibernate = FALSE;
  gboolean        auth_hybrid_sleep = FALSE;
  GError         *error = NULL;
  XfconfChannel  *channel;
  GtkWidget      *image;
  GtkWidget      *separator;
  GtkCssProvider *provider;

#ifdef HAVE_GTK_LAYER_SHELL
  if (gtk_layer_is_supported ())
    {
      gtk_layer_init_for_window (GTK_WINDOW (dialog));
      gtk_layer_set_keyboard_mode (GTK_WINDOW (dialog), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
      gtk_layer_set_layer (GTK_WINDOW (dialog), GTK_LAYER_SHELL_LAYER_OVERLAY);
    }
#endif

  dialog->type_clicked = XFSM_SHUTDOWN_LOGOUT;
  dialog->shutdown = xfsm_shutdown_get ();

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  /* Use Adwaita's keycap style to get a meaningful look out of the box */
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (dialog)), "keycap");
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (dialog)), "xfsm-logout-dialog");
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, ".xfsm-logout-dialog { font-size: initial; }", -1, NULL);
  gtk_style_context_add_provider (gtk_widget_get_style_context (GTK_WIDGET (dialog)),
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  /* load xfconf settings */
  channel = xfconf_channel_get (SETTINGS_CHANNEL);
  if (xfsm_shutdown_can_save_session (dialog->shutdown))
    save_session = xfconf_channel_get_bool (channel, "/general/SaveOnExit", TRUE);

  main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, BORDER);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), main_vbox, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
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
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (label)), "xfsm-logout-label");
  gtk_box_pack_start (GTK_BOX (main_vbox), label, FALSE, TRUE, 0);
  gtk_widget_show (label);
  g_free (label_str);

  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_scale_new (PANGO_SCALE_LARGE));
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  pango_attr_list_unref (attrs);

  separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start (GTK_BOX (main_vbox), separator, FALSE, TRUE, 0);
  gtk_widget_show (separator);

  /**
   * Start mode MODE_LOGOUT_BUTTONS
   **/
  dialog->box[MODE_LOGOUT_BUTTONS] = vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, BORDER);
  gtk_box_pack_start (GTK_BOX (main_vbox), vbox, TRUE, TRUE, 0);

  /**
   * Cancel
   **/
  dialog->button_cancel = gtk_dialog_add_button (GTK_DIALOG (dialog),
                                                 _("_Cancel"),
                                                 GTK_RESPONSE_CANCEL);

  button_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, BORDER);
  gtk_box_pack_start (GTK_BOX (vbox), button_vbox, FALSE, TRUE, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (button_vbox)), "xfsm-logout-buttons");
  gtk_widget_show (button_vbox);

  /* row for logout/shutdown and reboot */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, BORDER);
  gtk_box_pack_start (GTK_BOX (button_vbox), hbox, FALSE, TRUE, 0);
  gtk_box_set_homogeneous (GTK_BOX (hbox), TRUE);
  gtk_widget_show (hbox);

  /**
   * Logout
   **/
  can_logout = xfsm_shutdown_can_logout (dialog->shutdown);
  button = xfsm_logout_dialog_button (_("_Log Out"), "xfsm-logout",
                                      "system-log-out", NULL,
                                      XFSM_SHUTDOWN_LOGOUT, dialog);

  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  gtk_widget_show (button);
  gtk_widget_grab_focus (button);
  gtk_widget_set_sensitive (button, can_logout);

  /**
   * Check if packagekit downloaded offline updates
   **/
  has_updates = xfsm_shutdown_has_update_prepared (dialog->shutdown);

  /**
   * Reboot
   **/
  xfsm_shutdown_can_restart (dialog->shutdown, &can_restart, &auth_restart);

  button = xfsm_logout_dialog_button (has_updates ? _("_Restart and update") : _("_Restart"),
                                      "xfsm-reboot",
                                      "system-reboot", NULL,
                                      XFSM_SHUTDOWN_RESTART, dialog);

  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  gtk_widget_set_sensitive (button, can_restart && auth_restart);
  gtk_widget_show (button);

  /**
   * Shutdown
   **/
  xfsm_shutdown_can_shutdown (dialog->shutdown, &can_shutdown, &auth_shutdown);

  button = xfsm_logout_dialog_button (has_updates ? _("Update and Shut _Down") : _("Shut _Down"),
                                      "xfsm-shutdown",
                                      "system-shutdown", NULL,
                                      XFSM_SHUTDOWN_SHUTDOWN, dialog);

  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  gtk_widget_set_sensitive (button, can_shutdown && auth_shutdown);
  gtk_widget_show (button);

  /* new row for suspend/hibernate/hybrid sleep */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, BORDER);
  gtk_box_set_homogeneous (GTK_BOX (hbox), TRUE);
  gtk_box_pack_start (GTK_BOX (button_vbox), hbox, FALSE, TRUE, 0);

  /**
   * Suspend
   **/
  if (xfconf_channel_get_bool (channel, "/shutdown/ShowSuspend", TRUE))
    {
      xfsm_shutdown_can_suspend (dialog->shutdown, &can_suspend, &auth_suspend);
      if (can_suspend)
        {
          button = xfsm_logout_dialog_button (_("Sus_pend"), "xfsm-suspend",
                                              "system-suspend", NULL,
                                              XFSM_SHUTDOWN_SUSPEND, dialog);

          gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
          gtk_widget_set_sensitive (button, auth_suspend);
          gtk_widget_show (button);

          gtk_widget_show (hbox);
        }
    }

  /**
   * Hibernate
   **/
  if (xfconf_channel_get_bool (channel, "/shutdown/ShowHibernate", TRUE))
    {
      xfsm_shutdown_can_hibernate (dialog->shutdown, &can_hibernate, &auth_hibernate);
      if (can_hibernate)
        {
          button = xfsm_logout_dialog_button (_("_Hibernate"), "xfsm-hibernate",
                                              "system-hibernate", NULL,
                                              XFSM_SHUTDOWN_HIBERNATE, dialog);

          gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
          gtk_widget_set_sensitive (button, auth_hibernate);
          gtk_widget_show (button);

          gtk_widget_show (hbox);
        }
    }

  /**
   * Hybrid Sleep
   **/
  if (xfconf_channel_get_bool (channel, "/shutdown/ShowHybridSleep", TRUE))
    {
      xfsm_shutdown_can_hybrid_sleep (dialog->shutdown, &can_hybrid_sleep, &auth_hybrid_sleep);
      if (can_hybrid_sleep)
        {
          button = xfsm_logout_dialog_button (_("H_ybrid Sleep"), "xfsm-hibernate",
                                              "system-hibernate", "system-suspend-hibernate",
                                              XFSM_SHUTDOWN_HYBRID_SLEEP, dialog);

          gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
          gtk_widget_set_sensitive (button, auth_hybrid_sleep);
          gtk_widget_show (button);

          gtk_widget_show (hbox);
        }
    }

  /**
   * Switch User
   *
   * Hide the button if system cannot switch user, requires the display
   * manager to provide the org.freedesktop.DisplayManager dbus API
   **/
  if (xfconf_channel_get_bool (channel, "/shutdown/ShowSwitchUser", TRUE))
    {
      if (xfsm_shutdown_can_switch_user (dialog->shutdown, &can_switch_user, &error))
        {
          if (can_switch_user)
            {
              button = xfsm_logout_dialog_button (_("Switch _User"), "xfsm-switch-user",
                                                  "system-users", "system-users-symbolic",
                                                  XFSM_SHUTDOWN_SWITCH_USER, dialog);

              gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
              gtk_widget_set_sensitive (button, can_switch_user);
              gtk_widget_show (button);

              gtk_widget_show (hbox);
            }
        }
      else
        {
          g_warning ("Querying switch user failed: %s", ERROR_MSG (error));
          g_clear_error (&error);
        }
    }

  /**
   * Save session
   **/
  if (xfsm_shutdown_can_save_session (dialog->shutdown)
      && !xfconf_channel_get_bool (channel, "/general/AutoSave", FALSE)
      && xfconf_channel_get_bool (channel, "/general/ShowSave", TRUE)
      && WINDOWING_IS_X11 ())
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
  dialog->box[MODE_SHOW_ERROR] = vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, BORDER);
  gtk_box_pack_start (GTK_BOX (main_vbox), vbox, TRUE, TRUE, 0);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, BORDER * 2);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);

  image = gtk_image_new_from_icon_name ("dialog-error", GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, BORDER);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  label = gtk_label_new (_("An error occurred"));
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  gtk_label_set_xalign (GTK_LABEL (label), 0.00);
  gtk_label_set_yalign (GTK_LABEL (label), 0.50);
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
                           const gchar      *icon_name_fallback2,
                           XfsmShutdownType  type,
                           XfsmLogoutDialog *dialog)
{
  GtkWidget    *button;
  GtkWidget    *vbox;
  GtkWidget    *image;
  GtkWidget    *label;
  gint         *val;
  GtkIconTheme *icon_theme;

  g_return_val_if_fail (XFSM_IS_LOGOUT_DIALOG (dialog), NULL);

  val = g_new0 (gint, 1);
  *val = type;

  button = gtk_button_new ();
  g_object_set_data_full (G_OBJECT (button), "shutdown-type", val, g_free);
  g_signal_connect (G_OBJECT (button), "clicked",
      G_CALLBACK (xfsm_logout_dialog_button_clicked), dialog);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, BORDER);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);
  gtk_container_add (GTK_CONTAINER (button), vbox);
  gtk_widget_show (vbox);

  icon_theme = gtk_icon_theme_get_default ();

  image = gtk_image_new ();
  if (gtk_icon_theme_has_icon (icon_theme, icon_name))
      gtk_image_set_from_icon_name (GTK_IMAGE (image), icon_name, GTK_ICON_SIZE_DIALOG);
  else if (gtk_icon_theme_has_icon (icon_theme, icon_name_fallback))
      gtk_image_set_from_icon_name (GTK_IMAGE (image), icon_name_fallback, GTK_ICON_SIZE_DIALOG);
  else if (gtk_icon_theme_has_icon (icon_theme, icon_name_fallback2))
      gtk_image_set_from_icon_name (GTK_IMAGE (image), icon_name_fallback2, GTK_ICON_SIZE_DIALOG);

  gtk_image_set_pixel_size (GTK_IMAGE (image), 48);
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

  rect.width = gdk_window_get_width (window);
  rect.height = gdk_window_get_height (window);

  gdk_window_get_origin (window, &x, &y);

  rect.x = x;
  rect.y = y;

  if (!gdk_rectangle_intersect (&rect, &screen_rect, &rect))
    return NULL;

  screenshot = gdk_pixbuf_get_from_window  (window, 0, 0, rect.width, rect.height);

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

      if (!filename)
        {
          g_warning ("Unable to save screenshot, "
                     "error calling xfce_resource_save_location with %s, "
                     "check your permissions", path);
          g_free (path);
          return;
        }

      if (!gdk_pixbuf_save (scaled, filename, "png", &error, NULL))
        {
          g_warning ("Failed to save session screenshot: %s", error->message);
          g_error_free (error);
        }

      g_free (path);
      g_free (filename);
      g_object_unref (G_OBJECT (scaled));
    }
}



static void
xfsm_logout_dialog_grab_callback (GdkSeat   *seat,
                                  GdkWindow *window,
                                  gpointer   user_data)
{
  /* ensure window is mapped to avoid unsuccessful grabs */
  if (!gdk_window_is_visible (window))
    gdk_window_show (window);
}



static gint
xfsm_logout_dialog_run (GtkDialog *dialog,
                        gboolean   grab_input)
{
  GdkWindow *window;
  gint       ret;
  GdkDevice *device;
  GdkSeat   *seat;

  if (grab_input)
    {
      gtk_widget_show_now (GTK_WIDGET (dialog));

      window = gtk_widget_get_window (GTK_WIDGET (dialog));

      device = gtk_get_current_event_device ();
      seat = device != NULL
             ? gdk_device_get_seat (device)
             : gdk_display_get_default_seat (gtk_widget_get_display (GTK_WIDGET (dialog)));

      if (gdk_seat_grab (seat, window,
                         GDK_SEAT_CAPABILITY_KEYBOARD,
                         FALSE, NULL, NULL,
                         xfsm_logout_dialog_grab_callback,
                         NULL) != GDK_GRAB_SUCCESS)
        {
          g_critical ("Failed to grab the keyboard for logout window");
        }

#ifdef ENABLE_X11
      if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
        {
          /* force input to the dialog */
          gdk_x11_display_error_trap_push (gdk_display_get_default ());
          XSetInputFocus (gdk_x11_get_default_xdisplay (),
                          GDK_WINDOW_XID (window),
                          RevertToParent, CurrentTime);
          gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());
        }
#endif
    }

  ret = gtk_dialog_run (dialog);

  if (grab_input)
    gdk_seat_ungrab (seat);

  return ret;
}



gboolean
xfsm_logout_dialog (const gchar      *session_name,
                    XfsmShutdownType *return_type,
                    gboolean         *return_save_session,
                    gboolean          accessibility)
{
  gint              result;
  GtkWidget        *hidden;
  GtkWidget        *dialog;
  GdkScreen        *screen;
  gint              monitor;
  GdkPixbuf        *screenshot = NULL;
#ifdef ENABLE_X11
  XfsmFadeout      *fadeout = NULL;
#endif
  XfsmLogoutDialog *xfsm_dialog;
  XfconfChannel    *channel = xfconf_channel_get (SETTINGS_CHANNEL);
  gboolean          autosave;
  XfsmShutdown     *shutdown;
  GdkDevice        *device;
  GdkSeat          *seat;
  gint              grab_count = 0;

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

  hidden = gtk_invisible_new_for_screen (screen);
  gtk_widget_show (hidden);

  /* check if accessibility is enabled */
  if (G_LIKELY (!accessibility))
    {
      /* wait until we can grab the keyboard, we need this for
       * the dialog when running it */
      for (;;)
        {
          device = gtk_get_current_event_device ();
          seat = device != NULL
                 ? gdk_device_get_seat (device)
                 : gdk_display_get_default_seat (gtk_widget_get_display (hidden));

          if (gdk_seat_grab (seat, gtk_widget_get_window (hidden),
                             GDK_SEAT_CAPABILITY_KEYBOARD,
                             FALSE, NULL, NULL,
                             xfsm_logout_dialog_grab_callback,
                             NULL) == GDK_GRAB_SUCCESS)
            {
              gdk_seat_ungrab (seat);
              break;
            }

          if (grab_count++ >= 40)
            {
              g_warning ("Failed to grab the keyboard for logout window");
              break;
            }

          g_usleep (G_USEC_PER_SEC / 20);
        }

      /* make a screenshot */
      if (xfconf_channel_get_bool (channel, "/general/ShowScreenshots", TRUE))
        screenshot = xfsm_logout_dialog_screenshot_new (screen);

#ifdef ENABLE_X11
      if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
        {
          /* display fadeout */
          fadeout = xfsm_fadeout_new (gdk_screen_get_display (screen));
        }
#endif

      dialog = g_object_new (XFSM_TYPE_LOGOUT_DIALOG,
                             "type", GTK_WINDOW_POPUP,
                             "screen", screen, NULL);

      gtk_widget_realize (dialog);
      gdk_window_set_override_redirect (gtk_widget_get_window (dialog), TRUE);
      gdk_window_raise (gtk_widget_get_window (dialog));
    }
  else
    {
      dialog = g_object_new (XFSM_TYPE_LOGOUT_DIALOG,
                             "decorated", !accessibility,
                             "screen", screen, NULL);

      gtk_window_set_keep_above (GTK_WINDOW (dialog), TRUE);
      atk_object_set_role (gtk_widget_get_accessible (dialog), ATK_ROLE_ALERT);
    }

  gtk_widget_destroy (hidden);

  xfsm_dialog = XFSM_LOGOUT_DIALOG (dialog);

  /* set mode */
  xfsm_logout_dialog_set_mode (xfsm_dialog, MODE_LOGOUT_BUTTONS);

  result = xfsm_logout_dialog_run (GTK_DIALOG (dialog), !accessibility);

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

#ifdef ENABLE_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    {
      if (fadeout != NULL)
        xfsm_fadeout_destroy (fadeout);
    }
#endif

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
