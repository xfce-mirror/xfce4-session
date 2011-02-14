/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "xfae-dialog.h"

#include <libxfce4ui/libxfce4ui.h>

static void xfae_dialog_update (XfaeDialog *dialog);
static void xfae_dialog_browse (XfaeDialog *dialog);



struct _XfaeDialogClass
{
  GtkDialogClass __parent__;
};

struct _XfaeDialog
{
  GtkDialog __parent__;

  GtkWidget *name_entry;
  GtkWidget *descr_entry;
  GtkWidget *command_entry;
};



G_DEFINE_TYPE (XfaeDialog, xfae_dialog, GTK_TYPE_DIALOG)



static void
xfae_dialog_class_init (XfaeDialogClass *klass)
{
}



static void
xfae_dialog_init (XfaeDialog *dialog)
{
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *hbox;
  GtkWidget *button;
  GtkWidget *image;

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                          GTK_STOCK_OK, GTK_RESPONSE_OK,
                          NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);
  gtk_window_set_title (GTK_WINDOW (dialog), _("Add application"));

  table = g_object_new (GTK_TYPE_TABLE,
                        "border-width", 12,
                        "row-spacing", 6,
                        "column-spacing", 12,
                        "n-rows", 3,
                        "n-columns", 2,
                        "homogeneous", FALSE,
                        NULL);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), table, TRUE, TRUE, 0);
  gtk_widget_show (table);

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", _("Name:"),
                        "xalign", 0.0f,
                        NULL);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  dialog->name_entry = g_object_new (GTK_TYPE_ENTRY,
                                     "activates-default", TRUE,
                                     NULL);
  g_signal_connect_swapped (G_OBJECT (dialog->name_entry), "notify::text",
                            G_CALLBACK (xfae_dialog_update), dialog);
  gtk_table_attach (GTK_TABLE (table), dialog->name_entry, 1, 2, 0, 1,
                    GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (dialog->name_entry);

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", _("Description:"),
                        "xalign", 0.0f,
                        NULL);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  dialog->descr_entry = g_object_new (GTK_TYPE_ENTRY,
                                      "activates-default", TRUE,
                                      NULL);
  gtk_table_attach (GTK_TABLE (table), dialog->descr_entry, 1, 2, 1, 2,
                    GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (dialog->descr_entry);

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", _("Command:"),
                        "xalign", 0.0f,
                        NULL);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  hbox = g_object_new (GTK_TYPE_HBOX,
                       "spacing", 6,
                       NULL);
  gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, 2, 3,
                    GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (hbox);

  dialog->command_entry = g_object_new (GTK_TYPE_ENTRY,
                                        "activates-default", TRUE,
                                        NULL);
  g_signal_connect_swapped (G_OBJECT (dialog->command_entry), "notify::text",
                            G_CALLBACK (xfae_dialog_update), dialog);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->command_entry, TRUE, TRUE, 0);
  gtk_widget_show (dialog->command_entry);

  button = g_object_new (GTK_TYPE_BUTTON,
                         "can-default", FALSE,
                         NULL);
  g_signal_connect_swapped (G_OBJECT (button), "clicked",
                            G_CALLBACK (xfae_dialog_browse), dialog);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  image = gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (button), image);
  gtk_widget_show (image);
}



static void
xfae_dialog_update (XfaeDialog *dialog)
{
  const gchar *command;
  const gchar *name;

  command = gtk_entry_get_text (GTK_ENTRY (dialog->command_entry));
  name = gtk_entry_get_text (GTK_ENTRY (dialog->name_entry));

  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
                                     GTK_RESPONSE_OK,
                                     *command && *name);
}



static void
xfae_dialog_browse (XfaeDialog *dialog)
{
  const gchar *command;
  GtkWidget   *chooser;
  gchar       *filename;

  chooser = gtk_file_chooser_dialog_new (_("Select a command"),
                                         GTK_WINDOW (dialog),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                         NULL);

  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);

  command = gtk_entry_get_text (GTK_ENTRY (dialog->command_entry));
  if (G_UNLIKELY (*command == '/'))
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (chooser), command);

  if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT)
    {
      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
      gtk_entry_set_text (GTK_ENTRY (dialog->command_entry), filename);
      g_free (filename);
    }

  gtk_widget_destroy (chooser);
}



/**
 * xfae_dialog_new:
 * @name    : initial name or %NULL.
 * @descr   : initial description or %NULL..
 * @command : initial command or %NULL..
 *
 * Allocates a new #XfaeDialog instance.
 *
 * Return value: the newly allocated #XfaeDialog.
 **/
GtkWidget*
xfae_dialog_new (const gchar *name,
                 const gchar *descr,
                 const gchar *command)
{
  XfaeDialog *dialog = g_object_new (XFAE_TYPE_DIALOG, NULL);

  if (name)
    gtk_entry_set_text (GTK_ENTRY (dialog->name_entry), name);
  if (descr)
    gtk_entry_set_text (GTK_ENTRY (dialog->descr_entry), descr );
  if (command)
    gtk_entry_set_text (GTK_ENTRY (dialog->command_entry), command);
  if (name != NULL || descr != NULL || command != NULL)
    gtk_window_set_title (GTK_WINDOW (dialog), _("Edit application"));

  return GTK_WIDGET (dialog);
}



/**
 * xfae_dialog_get:
 * @dialog  : a #XfaeDialog.
 * @name    : return location for the name.
 * @descr   : return location for the description.
 * @command : return location for the command.
 *
 * Extracts the parameters selected by the user
 * from the @dialog.
 **/
void
xfae_dialog_get (XfaeDialog *dialog,
                 gchar     **name,
                 gchar     **descr,
                 gchar     **command)
{
  g_return_if_fail (XFAE_IS_DIALOG (dialog));
  g_return_if_fail (name != NULL);
  g_return_if_fail (descr != NULL);
  g_return_if_fail (command != NULL);

  *name = gtk_editable_get_chars (GTK_EDITABLE (dialog->name_entry), 0, -1);
  *descr = gtk_editable_get_chars (GTK_EDITABLE (dialog->descr_entry), 0, -1);
  *command = gtk_editable_get_chars (GTK_EDITABLE (dialog->command_entry), 0, -1);

  g_strstrip (*name);
  g_strstrip (*descr);
  g_strstrip (*command);
}
