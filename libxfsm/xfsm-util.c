/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2004 Jasper Huijsmans <jasper@xfce.org>
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
 * Parts of this file where taken from gnome-session/gsm-multiscreen.c,
 * which was written by Mark McLoughlin <mark@skynet.ie>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <cairo-gobject.h>

#include "xfsm-util.h"


gboolean
xfsm_start_application (gchar **command,
                        gchar **environment,
                        GdkScreen *screen,
                        const gchar *current_directory,
                        const gchar *client_machine,
                        const gchar *user_id)
{
  gboolean result;
  gchar *screen_name;
  gchar **argv;
  gint argc;
  gint size;

  g_return_val_if_fail (command != NULL && *command != NULL, FALSE);

  argv = g_new (gchar *, 21);
  size = 20;
  argc = 0;

  if (client_machine != NULL)
    {
      /* setting current directory on a remote machine is not supported */
      current_directory = NULL;

      argv[argc++] = g_strdup ("xon");
      argv[argc++] = g_strdup (client_machine);
    }

  if (screen != NULL)
    {
      if (client_machine != NULL)
        {
          gchar *display_name =
            xfsm_gdk_display_get_fullname (gdk_screen_get_display (screen));

          screen_name = g_strdup_printf ("%s.%d", display_name,
                                         gdk_screen_get_number (screen));
        }
      else
        screen_name = gdk_screen_make_display_name (screen);
      argv[argc++] = g_strdup ("env");
      argv[argc++] = g_strdup_printf ("DISPLAY=%s", screen_name);
      g_free (screen_name);
    }

  for (; *command != NULL; ++command)
    {
      if (argc == size)
        {
          size *= 2;
          argv = g_realloc (argv, (size + 1) * sizeof (*argv));
        }

      argv[argc++] = xfce_expand_variables (*command, environment);
    }

  argv[argc] = NULL;

  result = g_spawn_async (current_directory,
                          argv,
                          environment,
                          G_SPAWN_LEAVE_DESCRIPTORS_OPEN | G_SPAWN_SEARCH_PATH,
                          NULL,
                          NULL,
                          NULL,
                          NULL);

  g_strfreev (argv);

  return result;
}


void
xfsm_place_trash_window (GtkWindow *window,
                         GdkScreen *screen,
                         gint monitor)
{
  GtkRequisition requisition;
  GdkRectangle geometry;

  gdk_monitor_get_geometry (gdk_display_get_monitor (gdk_screen_get_display (screen), monitor), &geometry);
  gtk_widget_get_preferred_size (GTK_WIDGET (window), &requisition, NULL);

  gtk_window_move (window, 0, geometry.height - requisition.height);
}


gboolean
xfsm_strv_equal (gchar **a, gchar **b)
{
  if ((a == NULL && b != NULL) || (a != NULL && b == NULL))
    return FALSE;
  else if (a == NULL || b == NULL)
    return TRUE;

  while (*a != NULL && *b != NULL)
    {
      if (strcmp (*a, *b) != 0)
        return FALSE;

      ++a;
      ++b;
    }

  return (*a == NULL && *b == NULL);
}


gchar *
xfsm_gdk_display_get_fullname (GdkDisplay *display)
{
  const gchar *name;
  const gchar *np;
  gchar *hostname;
  gchar buffer[256];
  gchar *bp;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  name = gdk_display_get_name (display);
  if (WINDOWING_IS_WAYLAND ())
    return g_strdup (name);

  if (*name == ':')
    {
      hostname = xfce_gethostname ();
      g_strlcpy (buffer, hostname, 256);
      g_free (hostname);

      bp = buffer + strlen (buffer);

      for (np = name; *np != '\0' && *np != '.' && bp < buffer + 255;)
        *bp++ = *np++;
      *bp = '\0';
    }
  else
    {
      g_strlcpy (buffer, name, 256);

      for (bp = buffer + strlen (buffer) - 1; *bp != ':'; --bp)
        if (*bp == '.')
          {
            *bp = '\0';
            break;
          }
    }

#ifdef HAVE_OS_CYGWIN
  /* rename a colon (:) to a hash (#) under cygwin. windows doesn't like
   * filenames with a colon... */
  for (gchar *s = display_name; *s != '\0'; ++s)
    if (*s == ':')
      *s = '#';
#endif

  return g_strdup (buffer);
}

cairo_surface_t *
xfsm_load_session_preview (const gchar *name,
                           gint size,
                           gint scale_factor)
{
  cairo_surface_t *icon = NULL;
  GdkDisplay *display;
  gchar *display_name;
  gchar *filename;
  gchar *path;

  /* determine thumb file */
  display = gdk_display_get_default ();
  display_name = xfsm_gdk_display_get_fullname (display);
  path = g_strconcat ("sessions/thumbs-", display_name, "/", name, ".png", NULL);
  filename = xfce_resource_lookup (XFCE_RESOURCE_CACHE, path);
  g_free (display_name);
  g_free (path);

  if (filename != NULL)
    {
      GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale (filename, size * scale_factor, size * scale_factor, TRUE, NULL);
      if (G_LIKELY (pb != NULL))
        {
          icon = gdk_cairo_surface_create_from_pixbuf (pb, scale_factor, NULL);
          g_object_unref (pb);
        }
    }
  g_free (filename);

  return icon;
}

const gchar *
settings_list_sessions_get_filename (void)
{
  static gchar *filename = NULL;

  if (filename == NULL)
    {
      gchar *display_name = xfsm_gdk_display_get_fullname (gdk_display_get_default ());
      gchar *resource_name = g_strconcat ("sessions/xfce4-session-", display_name, NULL);
      filename = xfce_resource_save_location (XFCE_RESOURCE_CACHE, resource_name, TRUE);
      g_free (resource_name);
      g_free (display_name);
    }

  return filename;
}

GKeyFile *
settings_list_sessions_open_key_file (gboolean readonly)
{
  GKeyFile *file;
  GError *error = NULL;
  const gchar *filename, *delimiter;

  filename = settings_list_sessions_get_filename ();
  if (filename == NULL)
    {
      g_warning ("Failed to access path %s", filename);
      return NULL;
    }

  file = g_key_file_new ();
  if (!g_key_file_load_from_file (file, filename, G_KEY_FILE_NONE, &error))
    {
      /* session file does not exist: allowed */
      if (g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        {
          if (readonly)
            return NULL;
        }
      else
        {
          g_warning ("Failed to open session file %s: %s", filename, error->message);
          g_error_free (error);
          g_key_file_free (file);
          return NULL;
        }
    }

  delimiter = SESSION_FILE_DELIMITER;
  g_key_file_set_list_separator (file, *delimiter);

  return file;
}

GList *
settings_list_sessions (GKeyFile *file,
                        gint scale_factor)
{
  XfsmSessionInfo *session;
  cairo_surface_t *preview_default = NULL;
  GList *sessions = NULL;
  gchar **groups;
  gint n;

  groups = g_key_file_get_groups (file, NULL);
  for (n = 0; groups[n] != NULL; ++n)
    {
      if (strncmp (groups[n], "Session: ", 9) == 0)
        {
          session = g_new0 (XfsmSessionInfo, 1);
          session->name = g_strdup (groups[n] + 9);
          session->atime = g_key_file_get_integer (file, groups[n], "LastAccess", NULL);
          session->preview = xfsm_load_session_preview (session->name, 64, scale_factor);

          if (session->preview == NULL)
            {
              if (G_UNLIKELY (preview_default == NULL))
                {
                  GdkPixbuf *pb = gtk_icon_theme_load_icon_for_scale (gtk_icon_theme_get_default (),
                                                                      "xfce4-logo",
                                                                      64,
                                                                      scale_factor,
                                                                      GTK_ICON_LOOKUP_GENERIC_FALLBACK | GTK_ICON_LOOKUP_FORCE_SIZE,
                                                                      NULL);
                  if (G_LIKELY (pb != NULL))
                    {
                      preview_default = gdk_cairo_surface_create_from_pixbuf (pb, scale_factor, NULL);
                      g_object_unref (pb);
                    }
                }

              session->preview = cairo_surface_reference (preview_default);
            }

          sessions = g_list_append (sessions, session);
        }
    }

  if (preview_default != NULL)
    cairo_surface_destroy (preview_default);

  g_strfreev (groups);

  return sessions;
}

void
settings_list_sessions_treeview_init (GtkTreeView *treeview)
{
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GtkListStore *model;

  model = gtk_list_store_new (N_COLUMNS,
                              CAIRO_GOBJECT_TYPE_SURFACE,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_INT);

  gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (model));
  g_object_unref (G_OBJECT (model));

  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), TRUE);
  column = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "surface", PREVIEW_COLUMN,
                                       NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  column = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_set_title (column, _("Session"));
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "markup", TITLE_COLUMN,
                                       NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  column = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_set_alignment (column, 1.0);
  gtk_tree_view_column_set_title (column, _("Last accessed"));
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", ACCESSED_COLUMN,
                                       NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
}

static gint
compare_session_atime (XfsmSessionInfo *session1,
                       XfsmSessionInfo *session2)
{
  if (session1->atime < session2->atime)
    return 1;
  if (session1->atime > session2->atime)
    return -1;
  return 0;
}

static GList *
sort_sessions_on_atime (GList *sessions)
{
  return g_list_sort (sessions, (GCompareFunc) compare_session_atime);
}

void
settings_list_sessions_populate (GtkTreeModel *model,
                                 GList *sessions)
{
  XfsmSessionInfo *session;
  GtkTreeIter iter;
  gchar *accessed;
  gchar *title;
  GList *lp;

  sessions = sort_sessions_on_atime (sessions);

  gtk_list_store_clear (GTK_LIST_STORE (model));

  for (lp = sessions; lp != NULL; lp = lp->next)
    {
      session = (XfsmSessionInfo *) lp->data;

      title = g_strdup_printf ("<b>%s</b>", session->name);
      accessed = g_strstrip (g_strdup (ctime (&session->atime)));

      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          PREVIEW_COLUMN, session->preview,
                          NAME_COLUMN, session->name,
                          TITLE_COLUMN, title,
                          ACCESSED_COLUMN, accessed,
                          ATIME_COLUMN, session->atime,
                          -1);

      g_free (accessed);
      g_free (title);
    }
}

void
settings_list_sessions_delete_session (GtkButton *button,
                                       GtkTreeView *treeview)
{
  GKeyFile *file;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreeSelection *selection;
  GValue value = G_VALUE_INIT;
  gchar *session;

  selection = gtk_tree_view_get_selection (treeview);
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      g_warning ("xfsm_chooser_get_session: !gtk_tree_selection_get_selected");
      return;
    }

  file = settings_list_sessions_open_key_file (TRUE);
  if (file == NULL)
    return;

  /* Remove the session from session file */
  gtk_tree_model_get_value (model, &iter, NAME_COLUMN, &value);
  session = g_strdup_printf ("Session: %s", g_value_get_string (&value));
  g_value_unset (&value);
  g_key_file_remove_group (file, session, NULL);
  g_key_file_free (file);
  g_free (session);

  /* Remove the session from the treeview */
  model = gtk_tree_view_get_model (treeview);
  selection = gtk_tree_view_get_selection (treeview);
  gtk_tree_selection_get_selected (selection, &model, &iter);
  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}
