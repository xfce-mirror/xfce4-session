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
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <X11/Xlib.h>

#include <gdk/gdkx.h>

#include <libxfce4ui/libxfce4ui.h>

#include <libxfsm/xfsm-util.h>


gboolean
xfsm_start_application (gchar      **command,
                        gchar      **environment,
                        GdkScreen   *screen,
                        const gchar *current_directory,
                        const gchar *client_machine,
                        const gchar *user_id)
{
  gboolean result;
  gchar   *screen_name;
  gchar  **argv;
  gint     argc;
  gint     size;

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
                         gint       monitor)
{
  GtkRequisition requisition;
  GdkRectangle   geometry;

  gdk_screen_get_monitor_geometry (screen, monitor, &geometry);
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


XfconfChannel*
xfsm_open_config (void)
{
  static XfconfChannel *channel = NULL;

  if (G_UNLIKELY (channel == NULL))
    channel = xfconf_channel_get ("xfce4-session");
  return channel;
}

gchar*
xfsm_gdk_display_get_fullname (GdkDisplay *display)
{
  const gchar *name;
  const gchar *np;
  gchar       *hostname;
  gchar        buffer[256];
  gchar       *bp;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  name = gdk_display_get_name (display);
  if (*name == ':')
    {
      hostname = xfce_gethostname ();
      g_strlcpy (buffer, hostname, 256);
      g_free (hostname);

      bp = buffer + strlen (buffer);

      for (np = name; *np != '\0' && *np != '.' && bp < buffer + 255; )
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

  return g_strdup (buffer);
}

GdkPixbuf *
xfsm_load_session_preview (const gchar *name)
{
  GdkDisplay *display;
  GdkPixbuf  *pb = NULL;
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
    pb = gdk_pixbuf_new_from_file (filename, NULL);
  g_free (filename);

  return pb;
}

XfceRc *
settings_list_sessions_open_rc (void)
{
  XfceRc *rc;
  gchar *display_name;
  gchar *resource_name;
  gchar *session_file;

  display_name  = xfsm_gdk_display_get_fullname (gdk_display_get_default ());
  resource_name = g_strconcat ("sessions/xfce4-session-", display_name, NULL);
  session_file  = xfce_resource_save_location (XFCE_RESOURCE_CACHE, resource_name, TRUE);
  g_free (resource_name);
  g_free (display_name);

  if (!g_file_test (session_file, G_FILE_TEST_IS_REGULAR))
    {
      g_debug ("xfsm_manager_load_session: Something wrong with %s, Does it exist? Permissions issue?", session_file);
      return FALSE;
    }

  rc = xfce_rc_simple_open (session_file, FALSE);
  if (G_UNLIKELY (rc == NULL))
  {
    g_warning ("xfsm_manager_load_session: unable to open %s", session_file);
    return FALSE;
  }
  return rc;
}

GList *
settings_list_sessions (XfceRc *rc)
{
  XfsmSessionInfo *session;
  GdkPixbuf       *preview_default = NULL;
  GList           *sessions = NULL;
  gchar          **groups;
  gint             n;

  groups = xfce_rc_get_groups (rc);
  for (n = 0; groups[n] != NULL; ++n)
    {
      if (strncmp (groups[n], "Session: ", 9) == 0)
        {
          xfce_rc_set_group (rc, groups[n]);
          session = g_new0 (XfsmSessionInfo, 1);
          session->name = g_strdup (groups[n] + 9);
          session->atime = xfce_rc_read_int_entry (rc, "LastAccess", 0);
          session->preview = xfsm_load_session_preview (session->name);

          if (session->preview == NULL)
            {
              if (G_UNLIKELY (preview_default == NULL))
                {
                  preview_default = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                                              "xfce4-logo", 64,
                                                              GTK_ICON_LOOKUP_GENERIC_FALLBACK, NULL);
                }

              session->preview = GDK_PIXBUF (g_object_ref (preview_default));
            }

          sessions = g_list_append (sessions, session);
        }
    }

  if (preview_default != NULL)
    g_object_unref (preview_default);

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
                              GDK_TYPE_PIXBUF,
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
                                       "pixbuf", PREVIEW_COLUMN,
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
                                 GList        *sessions)
{
  XfsmSessionInfo *session;
  GtkTreeIter      iter;
  gchar           *accessed;
  gchar           *title;
  GList           *lp;

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
settings_list_sessions_delete_session (GtkButton   *button,
                                       GtkTreeView *treeview)
{
  XfceRc            *rc;
  gchar             *session_file;
  gchar             *display_name;
  gchar             *resource_name;
  GtkTreeModel      *model;
  GtkTreeIter        iter;
  GtkTreeSelection  *selection;
  GValue             value;
  gchar             *session;

  display_name = xfsm_gdk_display_get_fullname (gdk_display_get_default ());
  resource_name = g_strconcat ("sessions/xfce4-session-", display_name, NULL);
  session_file = xfce_resource_save_location (XFCE_RESOURCE_CACHE, resource_name, TRUE);

  if (!g_file_test (session_file, G_FILE_TEST_IS_REGULAR))
    {
      g_debug ("xfsm_manager_load_session: Something wrong with %s, Does it exist? Permissions issue?", session_file);
      return;
    }

  /* Remove the session from session file */
  bzero (&value, sizeof (value));
  selection = gtk_tree_view_get_selection (treeview);
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      g_warning ("xfsm_chooser_get_session: !gtk_tree_selection_get_selected");
      return;
    }
  gtk_tree_model_get_value (model, &iter, NAME_COLUMN, &value);
  session = g_strdup_printf ("Session: %s", g_value_get_string (&value));
  g_value_unset (&value);
  rc = xfce_rc_simple_open (session_file, FALSE);
  xfce_rc_delete_group (rc, session, FALSE);
  xfce_rc_close (rc);
  g_free (session);

  /* Remove the session from the treeview */
  model = gtk_tree_view_get_model (treeview);
  selection = gtk_tree_view_get_selection (treeview);
  gtk_tree_selection_get_selected (selection, &model, &iter);
  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}
