/* $Id$ */
/*-
 * Copyright (c) 2004 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include <xfce4-session/chooser-icon.h>
#include <xfce4-session/xfsm-chooser.h>
#include <xfce4-session/xfsm-util.h>


#define BORDER 6

static void xfsm_chooser_class_init (XfsmChooserClass *klass);
static void xfsm_chooser_init (XfsmChooser *chooser);
static void xfsm_chooser_finalize (GObject *object);
static void xfsm_chooser_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec);
static void xfsm_chooser_update_drag_icon (XfsmChooser *chooser);
static void xfsm_chooser_rebuild_list_store (XfsmChooser *chooser);
static void xfsm_chooser_update_list_store (XfsmChooser *chooser);


#ifdef SESSION_SCREENSHOTS
static GdkPixbuf *load_thumbnail (const gchar *name);
#endif
/*static gint session_compare (XfsmChooserSession *a, XfsmChooserSession *b);*/

enum
{
  PROP_ZERO,
  PROP_SESSION_RC,
  PROP_SESSION_DEFAULT,
};

enum
{
  PREVIEW_COLUMN,
  NAME_COLUMN,
  TITLE_COLUMN,
  ATIME_COLUMN,
  N_COLUMNS,
};


static GObjectClass *parent_class;

static GtkTargetEntry dnd_targets[] =
{
  { "application/xfsm4-session", 0, 0 },
};

static guint dnd_ntargets = sizeof (dnd_targets) / sizeof (*dnd_targets);


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
  gobject_class->set_property = xfsm_chooser_set_property;

  parent_class = gtk_type_class (gtk_dialog_get_type ());

  g_object_class_install_property (gobject_class, PROP_SESSION_DEFAULT,
      g_param_spec_string ("session-default", _("Default session"),
        _("Default session"), NULL, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, PROP_SESSION_RC,
      g_param_spec_pointer ("session-rc", _("Session rc object"),
        _("Session rc object"), G_PARAM_WRITABLE));
}


static void
xfsm_chooser_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  XfsmChooser *chooser;

  chooser = XFSM_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_SESSION_DEFAULT:
      if (chooser->session_default != NULL)
        g_free (chooser->session_default);
      chooser->session_default = g_value_dup_string (value);
      xfsm_chooser_update_list_store (chooser);
      break;

    case PROP_SESSION_RC:
      chooser->session_rc = XFCE_RC (g_value_get_pointer (value));
      xfsm_chooser_rebuild_list_store (chooser);
      xfsm_chooser_update_list_store (chooser);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
xfsm_chooser_update_drag_icon (XfsmChooser *chooser)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  GdkPixbuf        *icon;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser->tree));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter, PREVIEW_COLUMN, &icon, -1);
      gtk_drag_source_set_icon_pixbuf (chooser->tree, icon);
      g_object_unref (G_OBJECT (icon));
    }
}


static void
xfsm_chooser_focus_in (GtkWidget *widget,
                       GtkDirectionType arg1,
                       XfsmChooser *chooser)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chooser->radio_load),
                                TRUE);
  gtk_label_set_text (GTK_LABEL (chooser->start), _("Restore"));
  xfsm_chooser_update_drag_icon (chooser);
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
  xfsm_chooser_update_drag_icon (chooser);
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
      xfsm_chooser_update_drag_icon (chooser);
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
xfsm_chooser_drag_data_delete (GtkWidget      *treeview,
                               GdkDragContext *context,
                               XfsmChooser    *chooser)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  GValue            value;
  gchar             group[256];

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser->tree));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      if (G_LIKELY (chooser->session_rc != NULL))
        {
          bzero (&value, sizeof (value));
          gtk_tree_model_get_value (model, &iter, NAME_COLUMN, &value);
          g_snprintf (group, 256, "Session: %s", g_value_get_string (&value));
          g_value_unset (&value);

          xfce_rc_delete_group (chooser->session_rc, group, TRUE);
          xfce_rc_flush (chooser->session_rc);
        }

      gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
    }
}


static void
xfsm_chooser_init (XfsmChooser *chooser)
{
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
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
  model = gtk_list_store_new (N_COLUMNS,
                              GDK_TYPE_PIXBUF,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_INT);
  chooser->tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL(model));
  g_object_unref (G_OBJECT (model));
  gtk_tooltips_set_tip (chooser->tooltips, chooser->tree,
                        _("Choose the session you want to restore. "
                          "You can simply double-click the session "
                          "name to restore it."),
                        NULL);
  gtk_drag_source_set (chooser->tree, GDK_BUTTON1_MASK,
                       dnd_targets, dnd_ntargets,
                       GDK_ACTION_MOVE);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (chooser->tree), FALSE);
  column = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "pixbuf", PREVIEW_COLUMN,
                                       NULL);
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "markup", TITLE_COLUMN,
                                       NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (chooser->tree), column);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser->tree));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
  g_signal_connect (G_OBJECT (selection), "changed",
                    G_CALLBACK (xfsm_chooser_selection_changed), chooser);
  g_signal_connect (G_OBJECT (chooser->tree), "row-activated",
                    G_CALLBACK (xfsm_chooser_row_activated), chooser);
  g_signal_connect (G_OBJECT (chooser->tree), "focus-in-event",
                    G_CALLBACK (xfsm_chooser_focus_in), chooser);
  g_signal_connect (G_OBJECT (chooser->tree), "drag-data-delete",
                    G_CALLBACK (xfsm_chooser_drag_data_delete), chooser);
  gtk_container_add (GTK_CONTAINER (swin), chooser->tree);
  gtk_widget_set_size_request (chooser->tree, -1, 90);
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
  g_object_unref (chooser->tooltips);

  if (chooser->session_default != NULL)
    g_free (chooser->session_default);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
xfsm_chooser_rebuild_list_store (XfsmChooser *chooser)
{
  GdkPixbuf    *preview_default = NULL;
  GdkPixbuf    *preview;
  GtkTreeModel *model;
  GtkTreeIter   iter;
  gchar       **groups;
  gchar        *title;
  gchar        *name;
  guint         n;
  time_t        time;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (chooser->tree));

  /* clear list store */
  gtk_list_store_clear (GTK_LIST_STORE (model));

  if (G_LIKELY (chooser->session_rc != NULL))
    {
      groups = xfce_rc_get_groups (chooser->session_rc);
      for (n = 0; groups[n] != NULL; ++n)
        {
          if (G_UNLIKELY (strncmp (groups[n], "Session: ", 9) != 0)
              || G_UNLIKELY (strlen (groups[n]) == 9))
            {
              continue;
            }

          xfce_rc_set_group (chooser->session_rc, groups[n]);
          time = xfce_rc_read_int_entry (chooser->session_rc, "LastAccess", 0);
          name = groups[n] + 9;

#ifdef SESSION_SCREENSHOTS
          preview = load_thumbnail (name);
#else
          preview = NULL;
#endif

          if (preview == NULL)
            {
              if (G_UNLIKELY (preview_default == NULL))
                {
                  preview_default = xfce_inline_icon_at_size (chooser_icon_data,
                                                              52, 42);
                }

              preview = GDK_PIXBUF (g_object_ref (preview_default));
            }

          title = g_strdup_printf ("<b><big>%s</big></b>\n"
                                   "<small><i>Last access: %s</i></small>",
                                   name, ctime (&time));

          gtk_list_store_append (GTK_LIST_STORE (model), &iter);
          gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                              PREVIEW_COLUMN, preview,
                              NAME_COLUMN, name,
                              TITLE_COLUMN, title,
                              ATIME_COLUMN, time,
                              -1);

          g_object_unref (preview);
          g_free (title);
        }

      if (preview_default != NULL)
        g_object_unref (preview_default);
      g_strfreev (groups);

      /* XXX sort the model */
    }
}


static void
xfsm_chooser_update_list_store (XfsmChooser *chooser)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  gboolean          match;
  GValue            value;

  if (G_UNLIKELY (chooser->session_default == NULL))
    return;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser->tree));
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (chooser->tree));

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      bzero (&value, sizeof (value));

      do
        {
          gtk_tree_model_get_value (model, &iter, NAME_COLUMN, &value);

          match = (strcmp (chooser->session_default,
                           g_value_get_string (&value)) == 0);

          g_value_unset (&value);

          if (match)
            {
              gtk_tree_selection_select_iter (selection, &iter);
              break;
            }
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }
}


#if 0
void
xfsm_chooser_set_sessions (XfsmChooser *chooser,
                           GList *sessions,
                           const gchar *default_session)
{
  XfsmChooserSession *session;
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter diter;
  GtkTreeIter iter;
  GdkPixbuf *pb_default;
  GdkPixbuf *pb;
  gchar *title;
  GList *lp;

  g_return_if_fail (XFSM_IS_CHOOSER (chooser));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser->tree));
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (chooser->tree));

  if (sessions != NULL)
    {
      /* sort session by access time */
      sessions = g_list_sort (sessions, (GCompareFunc) session_compare);

      pb_default = xfce_inline_icon_at_size (chooser_icon_data, 52, 42);

      for (lp = sessions; lp != NULL; lp = lp->next)
        {
          session = (XfsmChooserSession *) lp->data;

#ifdef SESSION_SCREENSHOTS
          pb = load_thumbnail (session->name);
          if (pb == NULL)
            {
#endif
              g_object_ref (G_OBJECT (pb_default));
              pb = pb_default;
#ifdef SESSION_SCREENSHOTS
            }
#endif

          title = g_strdup_printf ("<b><big>%s</big></b>\n"
                                   "<small><i>Last access: %s</i></small>",
                                   session->name, ctime (&session->atime));

          gtk_list_store_append (GTK_LIST_STORE (model), &iter);
          gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                              ICON_COLUMN, pb,
                              NAME_COLUMN, session->name,
                              TITLE_COLUMN, title,
                              -1);

          if (lp == sessions || (default_session != NULL
                && strcmp (default_session, session->name) == 0))
            {
              diter = iter;
            }

          g_object_unref (G_OBJECT (pb));
          g_free (title);
        }

      g_object_unref (pb_default);

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
#endif


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


#ifdef SESSION_SCREENSHOTS
static GdkPixbuf*
load_thumbnail (const gchar *name)
{
  GdkDisplay *display;
  GdkPixbuf  *pb;
  gchar *display_name;
  gchar *resource;
  gchar *filename;

  /* determine thumb file */
  display = gdk_display_get_default ();
  display_name = xfsm_display_fullname (display);
  resource = g_strconcat ("sessions/thumbs-", display_name,
                          "/", name, ".png", NULL);
  filename = xfce_resource_save_location (XFCE_RESOURCE_CACHE, resource, TRUE);
  g_free (display_name);
  g_free (resource);

  pb = gdk_pixbuf_new_from_file (filename, NULL);

  g_free (filename);

  return pb;
}
#endif


/*static gint*/
/*session_compare (XfsmChooserSession *a, XfsmChooserSession *b)*/
/*{*/
/*  if (a->atime > b->atime)*/
/*    return -1;*/
/*  return 1;*/
/*}*/



