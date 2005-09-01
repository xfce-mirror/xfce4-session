/* $Id$ */
/*-
 * Copyright (c) 2003-2005 Benedikt Meurer <benny@xfce.org>
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
#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef GTK_DISABLE_DEPRECATED
#undef GTK_DISABLE_DEPRECATED
#endif

#include <gtk/gtk.h>

#include <libxfcegui4/libxfcegui4.h>

#define OPTION_TIPS 0
#define OPTION_FORTUNES 1

static const gchar *titles[] = {
  N_("Tips and tricks"),
  N_("Fortunes")
};

static guint option = OPTION_TIPS;
static GtkWidget *header = NULL;



static gboolean
autostart_enabled (void)
{
  gboolean enabled = FALSE;
  XfceRc  *rc;

  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "autostart/xfce4-tips-autostart.desktop", TRUE);
  if (G_LIKELY (rc != NULL))
    {
      xfce_rc_set_group (rc, "Desktop Entry");
      enabled = !xfce_rc_read_bool_entry (rc, "Hidden", TRUE);
      xfce_rc_close (rc);
    }

  return enabled;
}



static void
autostart_toggled (GtkToggleButton *button)
{
  gboolean active = gtk_toggle_button_get_active (button);
  XfceRc  *rc;

  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "autostart/xfce4-tips-autostart.desktop", FALSE);
  if (G_LIKELY (rc != NULL))
    {
      xfce_rc_set_group (rc, "Desktop Entry");
      xfce_rc_write_bool_entry (rc, "Hidden", !active);
      xfce_rc_close (rc);
    }
}



static void
title_cb(GtkWidget *w, const gchar *title)
{
  gchar *str;

  if (GTK_IS_LABEL(w)) {
    str = g_strconcat("<span size=\"larger\" weight=\"bold\">",
        _(title), "</span>", NULL);
    gtk_label_set_markup(GTK_LABEL(w), str);
    g_free(str);
  }
}

static void
item_cb(GtkWidget *btn, gpointer data)
{
  GtkWidget *wid;

  option = GPOINTER_TO_UINT(data);

  /* XXX - this should be improved in libxfcegui4 */
  wid = gtk_bin_get_child(GTK_BIN(header));
  gtk_container_foreach(GTK_CONTAINER(wid), (GtkCallback)title_cb,
      (gpointer)titles[option]);
}

static void
next_cb(GtkWidget *btn, GtkTextBuffer *textbuf)
{
  gchar buffer[1024];
  GtkTextIter start;
  GtkTextIter end;
  FILE *fp;

  /* clear the text buffer */
  gtk_text_buffer_get_bounds(textbuf, &start, &end);
  gtk_text_buffer_delete(textbuf, &start, &end);

  switch (option) {
  case OPTION_TIPS:
    strcpy(buffer, "fortune " TIPSDIR "/tips");
    break;

  case OPTION_FORTUNES:
    strcpy(buffer, "fortune");
    break;
  }

  if ((fp = popen(buffer, "r")) == NULL) {
    perror("Unable to execute fortune");
    return;
  }

  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    gtk_text_buffer_get_end_iter(textbuf, &end);
    gtk_text_buffer_insert(textbuf, &end, buffer, -1);
  }

  pclose (fp);
}

int
main (int argc, char **argv)
{
  GtkWidget *dlg;
  GtkWidget *sw;
  GtkWidget *view;
  GtkWidget *vbox1;
  GtkWidget *vbox2;
  GtkWidget *check;
  GtkWidget *bbox;
  GtkWidget *item;
  GtkWidget *menu;
  GtkWidget *opt;
  GtkWidget *next;
  GtkWidget *close;
  GdkPixbuf *logo;
  gchar *text;

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
  
  gtk_init (&argc, &argv);

  dlg = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (dlg), _("Xfce tips and tricks"));
  gtk_window_stick (GTK_WINDOW (dlg));
  gtk_window_set_default_size (GTK_WINDOW (dlg), 600, 400);
  gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox1);
  gtk_container_add (GTK_CONTAINER (dlg), vbox1);

  logo = xfce_themed_icon_load ("xfce4-logo", 48);
  gtk_window_set_icon (GTK_WINDOW (dlg), logo);
  text = g_strdup_printf (_("Tips and tricks"));
  header = xfce_create_header (logo, text);
  gtk_widget_show (header);
  gtk_box_pack_start (GTK_BOX (vbox1), header, FALSE, FALSE, 0);
  g_object_unref (logo);
  g_free (text);

  vbox2 = gtk_vbox_new (FALSE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (vbox2), 6);
  gtk_widget_show (vbox2);
  gtk_box_pack_start (GTK_BOX (vbox1), vbox2, TRUE, TRUE, 0);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
  gtk_widget_show (sw);
  gtk_box_pack_start (GTK_BOX (vbox2), sw, TRUE, TRUE, 0);

  view = gtk_text_view_new ();
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), FALSE);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
  gtk_widget_show (view);
  gtk_container_add (GTK_CONTAINER (sw), view);

  check = gtk_check_button_new_with_mnemonic (_("Display tips on _startup"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), autostart_enabled ());
  g_signal_connect (G_OBJECT (check), "toggled",
                    G_CALLBACK (autostart_toggled), NULL);
  gtk_box_pack_start (GTK_BOX (vbox2), check, FALSE, FALSE, 6);
  gtk_widget_show (check);

  /* button box */
  bbox = gtk_hbutton_box_new();
  gtk_widget_show(bbox);
  gtk_box_pack_start(GTK_BOX(vbox2), bbox, FALSE, FALSE, 0);

  menu = gtk_menu_new();
  gtk_widget_show(menu);
  
  item = gtk_menu_item_new_with_label(_("Tips and tricks"));
  g_signal_connect(item, "activate", G_CALLBACK(item_cb),
      GUINT_TO_POINTER(OPTION_TIPS));
  g_signal_connect(item, "activate", G_CALLBACK(next_cb),
      gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)));
  gtk_widget_show(item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

  item = gtk_menu_item_new_with_label(_("Fortunes"));
  g_signal_connect(item, "activate", G_CALLBACK(item_cb),
      GUINT_TO_POINTER(OPTION_FORTUNES));
  g_signal_connect(item, "activate", G_CALLBACK(next_cb),
      gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)));
  gtk_widget_show(item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

  opt = gtk_option_menu_new();
  gtk_option_menu_set_menu(GTK_OPTION_MENU(opt), menu);
  gtk_widget_show(opt);
  gtk_box_pack_start(GTK_BOX(bbox), opt, FALSE, FALSE, 0);

  next = gtk_button_new_with_label(_("Next"));
  gtk_widget_show(next);
  gtk_box_pack_start(GTK_BOX(bbox), next, FALSE, FALSE, 0);

  close = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
  gtk_widget_show(close);
  gtk_box_pack_start(GTK_BOX(bbox), close, FALSE, FALSE, 0);

  g_signal_connect(dlg, "delete-event", G_CALLBACK(gtk_main_quit), NULL);
  g_signal_connect(dlg, "destroy-event", G_CALLBACK(gtk_main_quit), NULL);
  g_signal_connect(close, "clicked", G_CALLBACK(gtk_main_quit), NULL);
  g_signal_connect(next, "clicked", G_CALLBACK(next_cb),
      gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)));

  next_cb(next, gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)));

  gtk_widget_show(dlg);

  gtk_main();

  return(0);
}
