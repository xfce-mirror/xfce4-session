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

#undef GTK_DISABLE_DEPRECATED

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

#include <settings/gnome-kde-logo.h>
#include <settings/looknfeel.h>
#include <settings/session-icon.h>
#include <settings/settings.h>



/*
   Global variables
 */
static GtkWidget *dialog = NULL;
static GtkWidget *general_omenu;
static GtkWidget *general_autosave;
static GtkWidget *general_prompt;
static GtkWidget *advanced_kde;
static GtkWidget *advanced_gnome;
static GtkWidget *advanced_remote;



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
  gint history;
  XfceRc *rc;
  
  g_return_if_fail (dialog != NULL);
  
  rc = config_open (FALSE);

  xfce_rc_set_group (rc, "General");
  xfce_rc_write_bool_entry (rc, "AutoSave", gtk_toggle_button_get_active (
                            GTK_TOGGLE_BUTTON (general_autosave)));
  xfce_rc_write_bool_entry (rc, "DisableTcp", !gtk_toggle_button_get_active (
                            GTK_TOGGLE_BUTTON (advanced_remote)));
  xfce_rc_write_bool_entry (rc, "PromptOnLogout", gtk_toggle_button_get_active (
                            GTK_TOGGLE_BUTTON (general_prompt)));

  xfce_rc_set_group (rc, "Compatibility");
  xfce_rc_write_bool_entry (rc, "LaunchGnome", gtk_toggle_button_get_active (
                            GTK_TOGGLE_BUTTON (advanced_gnome)));
  xfce_rc_write_bool_entry (rc, "LaunchKDE", gtk_toggle_button_get_active (
                            GTK_TOGGLE_BUTTON (advanced_kde)));

  history = gtk_option_menu_get_history (GTK_OPTION_MENU (general_omenu));
  xfce_rc_set_group (rc, "Chooser");
  if (history == 0) /* Always Display */
    {
      xfce_rc_write_bool_entry (rc, "AlwaysDisplay", TRUE);
      xfce_rc_write_int_entry (rc, "Timeout", 0);
    }
  else if (history == 1) /* On Demand */
    {
      xfce_rc_write_bool_entry (rc, "AlwaysDisplay", FALSE);
      xfce_rc_write_int_entry (rc, "Timeout", 4);
    }
  else /* No chooser */
    {
      xfce_rc_write_bool_entry (rc, "AlwaysDisplay", FALSE);
      xfce_rc_write_int_entry (rc, "Timeout", 0);
    }

  looknfeel_store (rc);

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
  GtkWidget *menu;
  GtkWidget *item;
  gboolean autosave;
  gboolean prompt;
  gint history;

  xfce_rc_set_group (rc, "General");
  autosave = xfce_rc_read_bool_entry (rc, "AutoSave", FALSE);
  prompt = xfce_rc_read_bool_entry (rc, "PromptOnLogout", TRUE);
  xfce_rc_set_group (rc, "Chooser");
  if (xfce_rc_read_bool_entry (rc, "AlwaysDisplay", FALSE))
    history = 0;
  else if (xfce_rc_read_int_entry (rc, "Timeout", 4) != 0)
    history = 1;
  else
    history = 2;

  page = gtk_vbox_new (FALSE, BORDER);
  gtk_container_set_border_width (GTK_CONTAINER (page), BORDER);

  frame = xfce_framebox_new (_("Session chooser"), TRUE);
  gtk_box_pack_start (GTK_BOX (page), frame, FALSE, TRUE, 0);
  vbox = gtk_vbox_new (FALSE, 0);
  xfce_framebox_add (XFCE_FRAMEBOX (frame), vbox);

  menu = gtk_menu_new ();
  item = gtk_menu_item_new_with_mnemonic (_("Display chooser on each login"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  item = gtk_menu_item_new_with_mnemonic (_("Display chooser on demand"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  item = gtk_menu_item_new_with_mnemonic (_("Restore last session automatically"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  general_omenu = gtk_option_menu_new ();
  gtk_option_menu_set_menu (GTK_OPTION_MENU (general_omenu), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (general_omenu), history);
  g_signal_connect (G_OBJECT (general_omenu), "changed",
                    G_CALLBACK (config_store), NULL);
  gtk_box_pack_start (GTK_BOX (vbox), general_omenu, FALSE, TRUE, 0);

  frame = xfce_framebox_new (_("Logout settings"), TRUE);
  gtk_box_pack_start (GTK_BOX (page), frame, FALSE, TRUE, 0);
  vbox = gtk_vbox_new (FALSE, 0);
  xfce_framebox_add (XFCE_FRAMEBOX (frame), vbox);

  general_autosave = gtk_check_button_new_with_label (_("Automatically save session on logout"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (general_autosave), autosave);
  g_signal_connect (G_OBJECT (general_autosave), "toggled",
                    G_CALLBACK (config_store), NULL);
  gtk_box_pack_start (GTK_BOX (vbox), general_autosave, FALSE, TRUE, 0);

  general_prompt = gtk_check_button_new_with_label (_("Prompt on logout"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (general_prompt), prompt);
  g_signal_connect (G_OBJECT (general_prompt), "toggled",
                    G_CALLBACK (config_store), NULL);
  gtk_widget_set_sensitive (general_prompt, FALSE);
  gtk_box_pack_start (GTK_BOX (vbox), general_prompt, FALSE, TRUE, 0);

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

  advanced_kde = gtk_check_button_new_with_label (_("Launch KDE services on startup"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (advanced_kde), kde);
  g_signal_connect (G_OBJECT (advanced_kde), "toggled",
                    G_CALLBACK (config_store), NULL);
  gtk_box_pack_start (GTK_BOX (vbox), advanced_kde, FALSE, TRUE, 0);

  icon = xfce_inline_icon_at_size (gnome_kde_logo_data, 64, 64);
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
#else
  gtk_widget_set_senstive (advanced_remote, FALSE);
#endif
  g_signal_connect (G_OBJECT (advanced_remote), "toggled",
                    G_CALLBACK (config_store), NULL);
  gtk_box_pack_start (GTK_BOX (vbox), advanced_remote, FALSE, TRUE, 0);

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
  XfceRc *rc;

  if (dialog != NULL)
    {
      gtk_window_present (GTK_WINDOW (dialog));
      return;
    }

  rc = config_open (TRUE);

  dialog = gtk_dialog_new_with_buttons (_("Sessions and Startup"),
                                        NULL,
                                        GTK_DIALOG_NO_SEPARATOR,
                                        GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
                                        NULL);

  gtk_window_set_icon (GTK_WINDOW (dialog), plugin->icon);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (dialog_response), NULL);
  g_signal_connect (G_OBJECT (dialog), "delete-event",
                    G_CALLBACK (dialog_response), NULL);

  dbox = GTK_DIALOG (dialog)->vbox;

  header = xfce_create_header (plugin->icon, _("Sessions and Startup"));
  gtk_box_pack_start (GTK_BOX (dbox), header, FALSE, TRUE, 0);

  notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (dbox), notebook, TRUE, TRUE, 0);

  label = gtk_label_new (_("General"));
  page = general_create (rc);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);

  label = gtk_label_new (_("Look & Feel"));
  page = looknfeel_create (rc);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);

  label = gtk_label_new (_("Advanced"));
  page = advanced_create (rc);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);

  gtk_widget_show_all (dialog);

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
  plugin->icon = xfce_inline_icon_at_size (session_icon_data, 48, 48);

  return MCS_PLUGIN_INIT_OK;
}


MCS_PLUGIN_CHECK_INIT;
