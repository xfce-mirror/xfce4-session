/* $Id$ */
/*-
 * Copyright (c) 2004 Benedikt Meurer <benny@xfce.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include <xfce4-session/xfsm-chooser.h>
#include <xfce4-session/xfsm-util.h>


#define BORDER 6

enum
{
  NAME_COLUMN,
  N_COLUMNS,
};

static void xfsm_chooser_class_init (XfsmChooserClass *klass);
static void xfsm_chooser_init (XfsmChooser *chooser);
static void xfsm_chooser_finalize (GObject *object);

static GObjectClass *parent_class;


GType
xfsm_chooser_get_type (void)
{
  static GType chooser_type = 0;

  if (chooser_type == 0)
    {
      static const GTypeInfo chooser_info = {
        sizeof (XfsmChooserClass),
        NULL,
        NULL,
        (GClassInitFunc) xfsm_chooser_class_init,
        NULL,
        NULL,
        sizeof (XfsmChooser),
        0,
        (GInstanceInitFunc) xfsm_chooser_init,
      };

      chooser_type = g_type_register_static (GTK_TYPE_DIALOG,
                                             "XfsmChooser",
                                             &chooser_info,
                                             0);
    }

  return chooser_type;
}

static void
xfsm_chooser_class_init (XfsmChooserClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfsm_chooser_finalize;
  parent_class = gtk_type_class (gtk_dialog_get_type ());
}

static void
xfsm_chooser_focus_in (GtkWidget *widget,
                       GtkDirectionType arg1,
                       XfsmChooser *chooser)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chooser->radio_load),
                                TRUE);
  gtk_label_set_text (GTK_LABEL (chooser->start), _("Restore"));
}

static void
xfsm_chooser_row_activated (GtkTreeView       *treeview,
                            GtkTreePath       *path,
                            GtkTreeViewColumn *column,
                            XfsmChooser       *chooser)
{
  gtk_dialog_response (GTK_DIALOG (chooser), GTK_RESPONSE_OK);
}

static void
xfsm_chooser_selection_changed (GtkTreeSelection *selection,
                                XfsmChooser *chooser)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chooser->radio_load),
                                TRUE);
  gtk_label_set_text (GTK_LABEL (chooser->start), _("Restore"));
}

static void
xfsm_chooser_name_changed (GtkEditable *name,
                           XfsmChooser *chooser)
{
  gboolean valid = TRUE;
  gchar *text;

  text = gtk_editable_get_chars (name, 0, -1);

  if (strlen (text) == 0)
    {
      valid = FALSE;
    }
  else
    {
      GtkTreeModel *model;
      GtkTreeIter iter;
      GValue value;

      bzero (&value, sizeof (value));

      model = gtk_tree_view_get_model (GTK_TREE_VIEW (chooser->tree));

      if (gtk_tree_model_get_iter_first (model, &iter))
        {
          while (valid)
            {
              gtk_tree_model_get_value (model, &iter, NAME_COLUMN, &value);

              if (strcmp (text, g_value_get_string (&value)) == 0)
                valid = FALSE;

              g_value_unset (&value);

              if (!gtk_tree_model_iter_next (model, &iter))
                break;
            }
        }
    }
    
  gtk_dialog_set_response_sensitive (GTK_DIALOG (chooser),
                                     GTK_RESPONSE_OK,
                                     valid);
  g_free (text);
}

static void
xfsm_chooser_load_toggled (GtkToggleButton *radio_load,
                           XfsmChooser *chooser)
{
  if (gtk_toggle_button_get_active (radio_load))
    {
      gtk_widget_set_sensitive (chooser->name, FALSE);
      gtk_dialog_set_response_sensitive (GTK_DIALOG (chooser),
                                         GTK_RESPONSE_OK,
                                         TRUE);
      gtk_label_set_text (GTK_LABEL (chooser->start), _("Restore"));
    }
  else
    {
      gtk_widget_set_sensitive (chooser->name, TRUE);
      xfsm_chooser_name_changed (GTK_EDITABLE (chooser->name),
                                 chooser);
      gtk_label_set_text (GTK_LABEL (chooser->start), _("Create"));
    }
}

static void
xfsm_chooser_init (XfsmChooser *chooser)
{
  GtkTreeSelection *selection;
  GtkListStore *model;
  GtkWidget *button;
  GtkWidget *swin;
  GtkWidget *dbox;
  GtkWidget *hbox;

  dbox = GTK_DIALOG (chooser)->vbox;

  /* allocate tooltips */
  chooser->tooltips = gtk_tooltips_new ();
  g_object_ref (G_OBJECT (chooser->tooltips));
  gtk_object_sink (GTK_OBJECT (chooser->tooltips));

  /* "Load session" radio button */
  chooser->radio_load = gtk_radio_button_new_with_label (NULL,
      _("Restore a previously saved session:"));
  gtk_tooltips_set_tip (chooser->tooltips, chooser->radio_load,
                        _("Choose this option if you want to restore "
                          "one of your previously saved sessions. "
                          "This will bring up the desktop as it was "
                          "when you saved the session."),
                        NULL);
  gtk_box_pack_start (GTK_BOX (dbox), chooser->radio_load,
                      FALSE, FALSE, BORDER);
  g_signal_connect (G_OBJECT (chooser->radio_load), "toggled",
                    G_CALLBACK (xfsm_chooser_load_toggled), chooser);
  gtk_widget_show (chooser->radio_load);

  /* scrolled window */
  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin), 
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin),
                                       GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (dbox), swin, TRUE, TRUE, 0);
  gtk_widget_show (swin);

  /* tree view */
  model = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING);
  chooser->tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL(model));
  g_object_unref (G_OBJECT (model));
  gtk_tooltips_set_tip (chooser->tooltips, chooser->tree,
                        _("Choose the session you want to restore. "
                          "You can simply double-click the session "
                          "name to restore it."),
                        NULL);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (chooser->tree), FALSE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (chooser->tree),
      gtk_tree_view_column_new_with_attributes ("Sessions",
        gtk_cell_renderer_text_new (), "text", NAME_COLUMN, NULL));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser->tree));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
  g_signal_connect (G_OBJECT (selection), "changed",
                    G_CALLBACK (xfsm_chooser_selection_changed), chooser);
  g_signal_connect (G_OBJECT (chooser->tree), "row-activated",
                    G_CALLBACK (xfsm_chooser_row_activated), chooser);
  g_signal_connect (G_OBJECT (chooser->tree), "focus-in-event",
                    G_CALLBACK (xfsm_chooser_focus_in), chooser);
  gtk_container_add (GTK_CONTAINER (swin), chooser->tree);
  gtk_widget_show (chooser->tree);

  /* horizontal box for the "Create new session" action */
  hbox = gtk_hbox_new (BORDER, FALSE);
  gtk_box_pack_start (GTK_BOX (dbox), hbox, FALSE, FALSE, BORDER);
  gtk_widget_show (hbox);

  /* "Create new session" radio button */
  chooser->radio_create = gtk_radio_button_new_with_label_from_widget (
      GTK_RADIO_BUTTON (chooser->radio_load), _("Create a new session:"));
  gtk_tooltips_set_tip (chooser->tooltips, chooser->radio_create,
                        _("Choose this option if you want to create a "
                          "new session. The new session will be started "
                          "as a default desktop. XXX - better text?"),
                        NULL);
  gtk_box_pack_start (GTK_BOX (hbox), chooser->radio_create, TRUE, FALSE, 0);
  gtk_widget_show (chooser->radio_create);

  /* Name entry for the "Create new session" action */
  chooser->name = gtk_entry_new ();
  gtk_tooltips_set_tip (chooser->tooltips, chooser->name,
                        _("Enter the name of the new session here. The "
                          "name has to be unique among your session names "
                          "and empty session names are not allowed."),
                        NULL);
  gtk_box_pack_start (GTK_BOX (hbox), chooser->name, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (chooser->name), "changed",
                    G_CALLBACK (xfsm_chooser_name_changed), chooser);
  gtk_widget_show (chooser->name);

  /* "Logout" button */
  button = xfsm_imgbtn_new (_("Logout"), GTK_STOCK_QUIT, NULL);
  gtk_tooltips_set_tip (chooser->tooltips, button,
                        _("Cancel the login attempt and return to "
                          "the login screen."),
                        NULL);
  gtk_dialog_add_action_widget (GTK_DIALOG (chooser), button,
                                GTK_RESPONSE_CANCEL);
  gtk_widget_show (button);

  /* "Continue" button */
  button = xfsm_imgbtn_new ("", GTK_STOCK_OK, &chooser->start);
  gtk_dialog_add_action_widget (GTK_DIALOG (chooser), button,
                                GTK_RESPONSE_OK);
  gtk_widget_show (button);
}


static void
xfsm_chooser_finalize (GObject *object)
{
  XfsmChooser *chooser;

  g_return_if_fail (XFSM_IS_CHOOSER (object));

  chooser = XFSM_CHOOSER (object);
  g_object_unref (G_OBJECT (chooser->tooltips));

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


void
xfsm_chooser_set_sessions (XfsmChooser *chooser,
                           GList *sessions,
                           const gchar *default_session)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter diter;
  GtkTreeIter iter;
  GList *lp;

  g_return_if_fail (XFSM_IS_CHOOSER (chooser));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser->tree));
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (chooser->tree));

  if (sessions != NULL)
    {
      for (lp = sessions; lp != NULL; lp = lp->next)
        {
          gtk_list_store_append (GTK_LIST_STORE (model), &iter);
          gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                              NAME_COLUMN, (const gchar *) lp->data,
                              -1);

          if (lp == sessions || (default_session != NULL
                && strcmp (default_session, (const gchar *) lp->data) == 0))
            {
              diter = iter;
            }
        }

      gtk_tree_selection_select_iter (selection, &diter);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chooser->radio_load),
                                    TRUE);

      gtk_label_set_text (GTK_LABEL (chooser->start), _("Restore"));
    }
  else
    {
      gtk_widget_set_sensitive (chooser->radio_load, FALSE);
      gtk_widget_set_sensitive (chooser->tree, FALSE);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chooser->radio_create),
                                    TRUE);

      gtk_label_set_text (GTK_LABEL (chooser->start), _("Create"));
    }

  xfsm_chooser_load_toggled (GTK_TOGGLE_BUTTON (chooser->radio_load), chooser);
}

XfsmChooserReturn
xfsm_chooser_run (XfsmChooser *chooser, gchar **name)
{
  gint response;

  g_return_val_if_fail (XFSM_IS_CHOOSER (chooser), XFSM_CHOOSER_LOGOUT);
  
  response = gtk_dialog_run (GTK_DIALOG (chooser));

  if (response == GTK_RESPONSE_CANCEL)
    return XFSM_CHOOSER_LOGOUT;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (chooser->radio_load)))
    {
      GtkTreeSelection *selection;
      GtkTreeModel *model;
      GtkTreeIter iter;
      GValue value;

      bzero (&value, sizeof (value));
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser->tree));
      gtk_tree_selection_get_selected (selection, &model, &iter);
      gtk_tree_model_get_value (model, &iter, NAME_COLUMN, &value);
      if (name != NULL)
        *name = g_value_dup_string (&value);
      g_value_unset (&value);

      return XFSM_CHOOSER_LOAD;
    }

  if (name != NULL)
    *name = gtk_editable_get_chars (GTK_EDITABLE (chooser->name), 0, -1);
  return XFSM_CHOOSER_CREATE;
}



