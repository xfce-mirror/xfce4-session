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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfce4mcs/mcs-manager.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <xfce-mcs-manager/manager-plugin.h>


#define BORDER 6


/*
   Global variables
 */
static GtkWidget    *dialog = NULL;
static GtkWidget    *general_chooser;
static GtkWidget    *general_autosave;
static GtkWidget    *general_prompt;
static GtkWidget    *advanced_kde;
static GtkWidget    *advanced_gnome;
static GtkWidget    *advanced_remote;
static GtkTooltips  *tooltips;
static gboolean      kiosk_can_chooser;
static gboolean      kiosk_can_logout;
static gboolean      kiosk_can_compat;
static gboolean      kiosk_can_security;



/*
   Config
 */
static XfceRc*
config_open (gboolean readonly)
{
  return xfce_rc_config_open (XFCE_RESOURCE_CONFIG,
                              "xfce4-session/xfce4-session.rc",
                              readonly);
}


void
config_store (void)
{
  XfceRc *rc;
  
  g_return_if_fail (dialog != NULL);
  
  rc = config_open (FALSE);

  xfce_rc_set_group (rc, "General");
  if (G_LIKELY (kiosk_can_logout))
    {
      xfce_rc_write_bool_entry (rc, "AutoSave", gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (general_autosave)));
      xfce_rc_write_bool_entry (rc, "PromptOnLogout", gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (general_prompt)));
    }
  if (G_LIKELY (kiosk_can_security))
    {
      xfce_rc_write_bool_entry (rc, "DisableTcp", !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (advanced_remote)));
    }

  if (G_LIKELY (kiosk_can_compat))
    {
      xfce_rc_set_group (rc, "Compatibility");
      xfce_rc_write_bool_entry (rc, "LaunchGnome", gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (advanced_gnome)));
      xfce_rc_write_bool_entry (rc, "LaunchKDE", gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (advanced_kde)));
    }

  if (G_LIKELY (kiosk_can_chooser))
    {
      xfce_rc_set_group (rc, "Chooser");
      xfce_rc_write_bool_entry (rc, "AlwaysDisplay", gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (general_chooser)));
    }

  xfce_rc_close (rc);
}



/*
   General tab
 */
static GtkWidget*
general_create (XfceRc *rc)
{
  GtkWidget *frame;
  GtkWidget *page;
  GtkWidget *vbox;
  gboolean   autosave;
  gboolean   prompt;
  gboolean   chooser;

  xfce_rc_set_group (rc, "General");
  autosave = xfce_rc_read_bool_entry (rc, "AutoSave", FALSE);
  prompt = xfce_rc_read_bool_entry (rc, "PromptOnLogout", TRUE);
  xfce_rc_set_group (rc, "Chooser");
  chooser = xfce_rc_read_bool_entry (rc, "AlwaysDisplay", FALSE);

  page = gtk_vbox_new (FALSE, BORDER);
  gtk_container_set_border_width (GTK_CONTAINER (page), BORDER);

  frame = xfce_framebox_new (_("Session chooser"), TRUE);
  gtk_box_pack_start (GTK_BOX (page), frame, FALSE, TRUE, 0);
  vbox = gtk_vbox_new (FALSE, 0);
  xfce_framebox_add (XFCE_FRAMEBOX (frame), vbox);

  general_chooser = gtk_check_button_new_with_label (_("Display chooser "
                                                       "on login"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (general_chooser), chooser);
  g_signal_connect (G_OBJECT (general_chooser), "toggled",
                    G_CALLBACK (config_store), NULL);
  gtk_widget_set_sensitive (general_chooser, kiosk_can_chooser);
  gtk_box_pack_start (GTK_BOX (vbox), general_chooser, FALSE, TRUE, 0);
  gtk_tooltips_set_tip (tooltips, general_chooser,
                        _("If set, the session manager will ask you to choose "
                          "a session every time you log in to Xfce."),
                        NULL);

  frame = xfce_framebox_new (_("Logout settings"), TRUE);
  gtk_box_pack_start (GTK_BOX (page), frame, FALSE, TRUE, 0);
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_set_sensitive (vbox, kiosk_can_logout);
  xfce_framebox_add (XFCE_FRAMEBOX (frame), vbox);

  general_autosave = gtk_check_button_new_with_label (_("Automatically save session on logout"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (general_autosave), autosave);
  g_signal_connect (G_OBJECT (general_autosave), "toggled",
                    G_CALLBACK (config_store), NULL);
  gtk_box_pack_start (GTK_BOX (vbox), general_autosave, FALSE, TRUE, 0);
  gtk_tooltips_set_tip (tooltips, general_autosave,
                        _("This option instructs the session manager to "
                          "save the current session automatically when "
                          "you log out. If you don't select this option "
                          "you'll be prompted whether you want to save "
                          "the current session on each logout."),
                        NULL);

  general_prompt = gtk_check_button_new_with_label (_("Prompt on logout"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (general_prompt), prompt);
  g_signal_connect (G_OBJECT (general_prompt), "toggled",
                    G_CALLBACK (config_store), NULL);
  gtk_box_pack_start (GTK_BOX (vbox), general_prompt, FALSE, TRUE, 0);
  gtk_tooltips_set_tip (tooltips, general_prompt,
                        _("This option disables the logout confirmation "
                          "dialog. Whether the session will be saved or not "
                          "depends on whether you enabled the automatic "
                          "saving of sessions on logout or not."),
                        NULL);

  return page;
}



/*
   Advanced
 */
static GtkWidget*
advanced_create (XfceRc *rc)
{
  GtkWidget *frame;
  GtkWidget *image;
  GtkWidget *page;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GdkPixbuf *icon;
  gboolean gnome;
  gboolean kde;
  gboolean remote;

  xfce_rc_set_group (rc, "Compatibility");
  gnome = xfce_rc_read_bool_entry (rc, "LaunchGnome", FALSE);
  kde = xfce_rc_read_bool_entry (rc, "LaunchKDE", FALSE);
  xfce_rc_set_group (rc, "General");
  remote = !xfce_rc_read_bool_entry (rc, "DisableTcp", TRUE);

  page = gtk_vbox_new (FALSE, BORDER);
  gtk_container_set_border_width (GTK_CONTAINER (page), BORDER);

  frame = xfce_framebox_new (_("Compatibility"), TRUE);
  gtk_box_pack_start (GTK_BOX (page), frame, FALSE, TRUE, 0);
  hbox = gtk_hbox_new (FALSE, 0);
  xfce_framebox_add (XFCE_FRAMEBOX (frame), hbox);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_set_sensitive (vbox, kiosk_can_compat);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

  advanced_gnome = gtk_check_button_new_with_label (_("Launch Gnome services on startup"));
#ifdef HAVE_GNOME
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (advanced_gnome), gnome);
#else
  gtk_widget_set_sensitive (advanced_gnome, FALSE);
#endif
  g_signal_connect (G_OBJECT (advanced_gnome), "toggled",
                    G_CALLBACK (config_store), NULL);
  gtk_box_pack_start (GTK_BOX (vbox), advanced_gnome, FALSE, TRUE, 0);
  gtk_tooltips_set_tip (tooltips, advanced_gnome,
                        _("Enable this if you plan to use Gnome applications. "
                          "This will instruct the session manager to start "
                          "some vital Gnome services for you. You should "
                          "also enable this if you want to use the Assistive "
                          "Technologies that ship with Gnome."),
                        NULL);

  advanced_kde = gtk_check_button_new_with_label (_("Launch KDE services on startup"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (advanced_kde), kde);
  g_signal_connect (G_OBJECT (advanced_kde), "toggled",
                    G_CALLBACK (config_store), NULL);
  gtk_box_pack_start (GTK_BOX (vbox), advanced_kde, FALSE, TRUE, 0);
  gtk_tooltips_set_tip (tooltips, advanced_kde,
                        _("Enable this option if you plan to run KDE "
                          "applications as part of your Xfce Desktop session. "
                          "This will notably increase the startup time, but "
                          "on the other hand, KDE applications will startup "
                          "faster. Some KDE applications may not work at all "
                          "if you don't enable this option."),
                        NULL);

  icon = xfce_themed_icon_load ("xfsm-gnome-kde-logo", 64);
  image = gtk_image_new_from_pixbuf (icon);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, TRUE, 0);
  g_object_unref (icon);

  frame = xfce_framebox_new (_("Security"), TRUE);
  gtk_box_pack_start (GTK_BOX (page), frame, FALSE, TRUE, 0);
  vbox = gtk_vbox_new (FALSE, 0);
  xfce_framebox_add (XFCE_FRAMEBOX (frame), vbox);

  advanced_remote = gtk_check_button_new_with_label (_("Manage remote applications"));
#ifdef HAVE__ICETRANSNOLISTEN
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (advanced_remote), remote);
  gtk_widget_set_sensitive (advanced_remote, kiosk_can_security);
#else
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (advanced_remote), TRUE);
  gtk_widget_set_senstive (advanced_remote, FALSE);
#endif
  g_signal_connect (G_OBJECT (advanced_remote), "toggled",
                    G_CALLBACK (config_store), NULL);
  gtk_box_pack_start (GTK_BOX (vbox), advanced_remote, FALSE, TRUE, 0);
  gtk_tooltips_set_tip (tooltips, advanced_remote,
                        _("Allow the session manager to manage applications "
                          "running on remote hosts. Do not enable this "
                          "option unless you know what you are doing."),
                        NULL);

  return page;
}



/*
   Dialog
 */
static gboolean
dialog_response (void)
{
  if (dialog != NULL)
    {
      gtk_widget_destroy (dialog);
      dialog = NULL;
    }

  if (tooltips != NULL)
    {
      gtk_object_destroy (GTK_OBJECT (tooltips));
      tooltips = NULL;
    }

  return TRUE;
}


static void
dialog_run (McsPlugin *plugin)
{
  GtkWidget *notebook;
  GtkWidget *header;
  GtkWidget *label;
  GtkWidget *page;
  GtkWidget *dbox;
  XfceKiosk *kiosk;
  XfceRc    *rc;

  if (dialog != NULL)
    {
      gtk_window_present (GTK_WINDOW (dialog));
      return;
    }

  kiosk = xfce_kiosk_new ("xfce4-session");
  kiosk_can_chooser = xfce_kiosk_query (kiosk, "Chooser");
  kiosk_can_logout = xfce_kiosk_query (kiosk, "Logout");
  kiosk_can_compat = xfce_kiosk_query (kiosk, "Compatibility");
  kiosk_can_security = xfce_kiosk_query (kiosk, "Security");
  xfce_kiosk_free (kiosk);
  tooltips = gtk_tooltips_new ();

  rc = config_open (TRUE);

  dialog = gtk_dialog_new_with_buttons (_("Sessions and Startup"),
                                        NULL,
                                        GTK_DIALOG_NO_SEPARATOR,
                                        GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
                                        NULL);

  gtk_window_set_icon (GTK_WINDOW (dialog), plugin->icon);

  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (dialog_response), NULL);
  g_signal_connect (G_OBJECT (dialog), "delete-event",
                    G_CALLBACK (dialog_response), NULL);

  dbox = GTK_DIALOG (dialog)->vbox;

  header = xfce_create_header (plugin->icon, _("Sessions and Startup"));
  gtk_box_pack_start (GTK_BOX (dbox), header, FALSE, TRUE, 0);
  gtk_widget_show (header);

  notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (dbox), notebook, TRUE, TRUE, 0);
  gtk_widget_show (notebook);

  label = gtk_label_new (_("General"));
  page = general_create (rc);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);
  gtk_widget_show_all (page);
  gtk_widget_show (label);

  label = gtk_label_new (_("Advanced"));
  page = advanced_create (rc);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);
  gtk_widget_show_all (page);
  gtk_widget_show (label);

  xfce_gtk_window_center_on_monitor_with_pointer (GTK_WINDOW (dialog));
  gtk_widget_show (dialog);

  xfce_rc_close (rc);
}



/*
   Mcs interface
 */
McsPluginInitResult
mcs_plugin_init (McsPlugin *plugin)
{
  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  plugin->plugin_name = g_strdup ("session");
  plugin->caption = g_strdup (_("Sessions and Startup"));
  plugin->run_dialog = dialog_run;
  plugin->icon = xfce_themed_icon_load ("xfce4-session", 48);

  return MCS_PLUGIN_INIT_OK;
}


MCS_PLUGIN_CHECK_INIT;
