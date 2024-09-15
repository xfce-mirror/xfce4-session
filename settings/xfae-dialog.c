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
#include "config.h"
#endif

#include <libxfce4ui/libxfce4ui.h>

#include "xfae-dialog.h"

static void
xfae_dialog_update (XfaeDialog *dialog);
static void
xfae_dialog_browse (XfaeDialog *dialog);



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
  GtkWidget *run_hook_combo;
};



G_DEFINE_TYPE (XfaeDialog, xfae_dialog, GTK_TYPE_DIALOG)



static void
xfae_dialog_class_init (XfaeDialogClass *klass)
{
}



static void
xfae_dialog_init (XfaeDialog *dialog)
{
  GtkWidget *content_area;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *hbox;
  GtkWidget *button;
  GtkWidget *image;
  GEnumClass *klass;
  GEnumValue *enum_struct;
  guint i;

  gtk_dialog_add_buttons (
    GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_CANCEL, _("_OK"), GTK_RESPONSE_OK, NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);
  gtk_window_set_title (GTK_WINDOW (dialog), _("Add application"));

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_container_set_border_width (GTK_CONTAINER (content_area), 6);

  grid = g_object_new (GTK_TYPE_GRID,
                       "row-spacing", 6,
                       "column-spacing", 12,
                       NULL);
  gtk_container_add (GTK_CONTAINER (content_area), grid);
  gtk_container_set_border_width (GTK_CONTAINER (grid), 6);
  gtk_widget_show (grid);

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", _("Name:"),
                        "xalign", 0.0f,
                        NULL);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  gtk_widget_show (label);

  dialog->name_entry = g_object_new (GTK_TYPE_ENTRY,
                                     "activates-default", TRUE,
                                     "hexpand", TRUE,
                                     NULL);

  gtk_grid_attach (GTK_GRID (grid), dialog->name_entry, 1, 0, 1, 1);
  gtk_widget_show (dialog->name_entry);

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", _("Description:"),
                        "xalign", 0.0f,
                        NULL);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
  gtk_widget_show (label);

  dialog->descr_entry = g_object_new (GTK_TYPE_ENTRY,
                                      "activates-default", TRUE,
                                      "hexpand", TRUE,
                                      NULL);
  gtk_grid_attach (GTK_GRID (grid), dialog->descr_entry, 1, 1, 1, 1);
  gtk_widget_show (dialog->descr_entry);

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", _("Command:"),
                        "xalign", 0.0f,
                        NULL);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);
  gtk_widget_show (label);

  hbox = g_object_new (GTK_TYPE_BOX,
                       "spacing", 6,
                       NULL);
  gtk_grid_attach (GTK_GRID (grid), hbox, 1, 2, 1, 1);
  gtk_widget_show (hbox);

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", _("Trigger:"),
                        "xalign", 0.0f,
                        NULL);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 3, 1, 1);
  gtk_widget_show (label);

  dialog->run_hook_combo = gtk_combo_box_text_new ();
  gtk_widget_set_margin_bottom (dialog->run_hook_combo, 5);
  klass = g_type_class_ref (XFSM_TYPE_RUN_HOOK);
  for (i = 0; i < klass->n_values; ++i)
    {
      enum_struct = g_enum_get_value (klass, i);
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (dialog->run_hook_combo), _(enum_struct->value_nick));
    }
  g_type_class_unref (klass);
  gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->run_hook_combo), 0);

  gtk_grid_attach (GTK_GRID (grid), dialog->run_hook_combo, 1, 3, 1, 1);
  gtk_widget_show (dialog->run_hook_combo);

  dialog->command_entry = g_object_new (GTK_TYPE_ENTRY,
                                        "activates-default", TRUE,
                                        "hexpand", TRUE,
                                        NULL);

  gtk_box_pack_start (GTK_BOX (hbox), dialog->command_entry, TRUE, TRUE, 0);
  gtk_widget_show (dialog->command_entry);

  g_signal_connect_swapped (G_OBJECT (dialog->name_entry), "notify::text",
                            G_CALLBACK (xfae_dialog_update), dialog);
  g_signal_connect_swapped (G_OBJECT (dialog->command_entry), "notify::text",
                            G_CALLBACK (xfae_dialog_update), dialog);

  button = g_object_new (GTK_TYPE_BUTTON,
                         "can-default", FALSE,
                         NULL);
  g_signal_connect_swapped (G_OBJECT (button), "clicked",
                            G_CALLBACK (xfae_dialog_browse), dialog);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  image = gtk_image_new_from_icon_name ("document-open", GTK_ICON_SIZE_BUTTON);
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
  GtkWidget *chooser;
  gchar *filename;

  chooser = gtk_file_chooser_dialog_new (_("Select a command"), GTK_WINDOW (dialog),
                                         GTK_FILE_CHOOSER_ACTION_OPEN, _("Cancel"),
                                         GTK_RESPONSE_CANCEL, _("OK"), GTK_RESPONSE_ACCEPT,
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
GtkWidget *
xfae_dialog_new (const gchar *name,
                 const gchar *descr,
                 const gchar *command,
                 XfsmRunHook run_hook)
{
  XfaeDialog *dialog = g_object_new (XFAE_TYPE_DIALOG, NULL);

  if (name)
    gtk_entry_set_text (GTK_ENTRY (dialog->name_entry), name);
  if (descr)
    gtk_entry_set_text (GTK_ENTRY (dialog->descr_entry), descr);
  if (command)
    gtk_entry_set_text (GTK_ENTRY (dialog->command_entry), command);
  gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->run_hook_combo), run_hook);
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
                 gchar **name,
                 gchar **descr,
                 gchar **command,
                 XfsmRunHook *run_hook)
{
  g_return_if_fail (XFAE_IS_DIALOG (dialog));
  g_return_if_fail (name != NULL);
  g_return_if_fail (descr != NULL);
  g_return_if_fail (command != NULL);
  g_return_if_fail (run_hook != NULL);

  *name = gtk_editable_get_chars (GTK_EDITABLE (dialog->name_entry), 0, -1);
  *descr = gtk_editable_get_chars (GTK_EDITABLE (dialog->descr_entry), 0, -1);
  *command = gtk_editable_get_chars (GTK_EDITABLE (dialog->command_entry), 0, -1);
  *run_hook = gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->run_hook_combo));

  g_strstrip (*name);
  g_strstrip (*descr);
  g_strstrip (*command);
}
