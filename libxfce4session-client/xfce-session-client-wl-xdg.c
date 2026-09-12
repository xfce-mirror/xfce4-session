/*
 * Copyright (c) 2026 Brian Tarricone <brian@terricone.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include <gdk/gdkwayland.h>
#include <libxfce4util/libxfce4util.h>
#include <stdbool.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include "libwayland-shim.h"
#include "stolen-from-libwayland.h"
#include "xdg-session-management-v1-client.h"
#include "xdg-shell-client.h"
#include "xfce-session-client-dbus.h"
#include "xfce-session-client-private.h"
#include "xfce-session-client-wl-xdg.h"
#include "xfsm-manager-dbus-client.h"

#define TOPLEVEL_NAME_QUARK (toplevel_name_quark ())
#define SESSION_REGISTRATION_TIMEOUT (1000000) /* 1 second */

typedef enum
{
  SESSION_STATUS_INVALID = -1,
  SESSION_STATUS_CREATED = 0,
  SESSION_STATUS_RESTORED,
  SESSION_STATUS_REPLACED,
} SessionStatus;

struct _XfceSessionClientWlXdg
{
  XfceSessionClientDBus parent;

  struct wl_registry *registry;
  struct xdg_session_manager_v1 *xdg_manager;
  struct xdg_session_v1 *xdg_session;

  gchar *start_reason;
  SessionStatus session_status;

  GHashTable *toplevels; // gchar* -> Toplevel* (owns Toplevel)
  GQueue *queued_toplevel_removes; // gchar *
};

typedef struct
{
  GHashTable *surfaces_by_wl_surface; // wl_surface* -> WindowSurfaces* (owns WindowSurfaces)
  GHashTable *surfaces_by_xdg_surface; // xdg_surface* -> WindowSurfaces*
  GHashTable *surfaces_by_xdg_toplevel; // xdg_toplevel* -> WindowSurfaces*
} Surfaces;

typedef struct
{
  struct wl_surface *wl_surface;
  struct xdg_surface *xdg_surface;
  struct xdg_toplevel *xdg_toplevel;
} WindowSurfaces;

typedef struct
{
  gchar *restore_from_name;

  GtkWindow *window;
  gpointer signal_user_data;

  struct xdg_toplevel_session_v1 *toplevel_session;
} Toplevel;

static void
xfce_session_client_wl_xdg_finalize (GObject *obj);

static gboolean
xfce_session_client_wl_xdg_connect (XfceSessionClient *session_client,
                                    GError **error);
static void
xfce_session_client_wl_xdg_disconnect (XfceSessionClient *session_client);
static gboolean
xfce_session_client_wl_xdg_is_connected (XfceSessionClient *session_client);

static void
xfce_session_client_wl_xdg_add_window (XfceSessionClient *session_client,
                                       GtkWindow *window,
                                       const gchar *name);
static void
xfce_session_client_wl_xdg_restore_window (XfceSessionClient *session_client,
                                           GtkWindow *window,
                                           const gchar *name);
static void
xfce_session_client_wl_xdg_rename_window (XfceSessionClient *session_client,
                                          GtkWindow *window,
                                          const gchar *new_name);
static void
xfce_session_client_wl_xdg_remove_window (XfceSessionClient *session_client,
                                          const gchar *name);

static gboolean
intercept_xdg_wm_base_get_xdg_surface (gpointer data,
                                       struct wl_proxy *proxy,
                                       uint32_t opcode,
                                       struct wl_interface const *created_interface,
                                       uint32_t created_version,
                                       uint32_t flags,
                                       union wl_argument *args,
                                       struct wl_proxy **ret_proxy);

static gboolean
intercept_xdg_surface_get_toplevel (gpointer data,
                                    struct wl_proxy *proxy,
                                    uint32_t opcode,
                                    struct wl_interface const *created_interface,
                                    uint32_t created_version,
                                    uint32_t flags,
                                    union wl_argument *args,
                                    struct wl_proxy **ret_proxy);

static gboolean
intercept_xdg_surface_destroy (gpointer data,
                               struct wl_proxy *proxy,
                               uint32_t opcode,
                               struct wl_interface const *created_interface,
                               uint32_t created_version,
                               uint32_t flags,
                               union wl_argument *args,
                               struct wl_proxy **ret_proxy);

static gboolean
intercept_xdg_toplevel_destroy (gpointer data,
                                struct wl_proxy *proxy,
                                uint32_t opcode,
                                struct wl_interface const *created_interface,
                                uint32_t created_version,
                                uint32_t flags,
                                union wl_argument *args,
                                struct wl_proxy **ret_proxy);

static void
registry_global (void *data,
                 struct wl_registry *wl_registry,
                 uint32_t name,
                 const char *interface,
                 uint32_t version);
static void
registry_global_remove (void *data,
                        struct wl_registry *wl_registry,
                        uint32_t name);

static void
session_created (void *data,
                 struct xdg_session_v1 *xdg_session_v1,
                 const char *session_id);
static void
session_restored (void *data,
                  struct xdg_session_v1 *xdg_session_v1);
static void
session_replaced (void *data,
                  struct xdg_session_v1 *xdg_session_v1);

static const gchar *
reason_to_string (enum xdg_session_manager_v1_reason reason);
static const gchar *
session_status_to_string (SessionStatus status);

static void
toplevel_free (Toplevel *toplevel);

static GQuark
toplevel_name_quark (void);

static void
surfaces_init (void);

static WindowSurfaces *
surfaces_insert (Surfaces *surfaces,
                 struct wl_surface *wl_surface,
                 struct xdg_surface *xdg_surface);
static void
surfaces_set_xdg_toplevel (Surfaces *surfaces,
                           WindowSurfaces *wsurfaces,
                           struct xdg_toplevel *xdg_toplevel);

static WindowSurfaces *
surfaces_get_for_window (Surfaces *surfaces,
                         GtkWindow *window);
static WindowSurfaces *
surfaces_get_for_wl_surface (Surfaces *surfaces,
                             struct wl_surface *wl_surface);
static WindowSurfaces *
surfaces_get_for_xdg_surface (Surfaces *surfaces,
                              struct xdg_surface *xdg_surface);

static void
surfaces_xdg_surface_destroyed (Surfaces *surfaces,
                                struct xdg_surface *xdg_surface);
static void
surfaces_xdg_toplevel_destroyed (Surfaces *surfaces,
                                 struct xdg_toplevel *xdg_toplevel);


G_DEFINE_FINAL_TYPE (XfceSessionClientWlXdg, xfce_session_client_wl_xdg, XFCE_TYPE_SESSION_CLIENT_DBUS)


static Surfaces *all_surfaces = NULL;

static const struct wl_registry_listener registry_listener = {
  .global = registry_global,
  .global_remove = registry_global_remove,
};

static const struct xdg_session_v1_listener session_listener = {
  .created = session_created,
  .restored = session_restored,
  .replaced = session_replaced,
};

static void
xfce_session_client_wl_xdg_class_init (XfceSessionClientWlXdgClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = xfce_session_client_wl_xdg_finalize;

  XfceSessionClientClass *session_client_class = XFCE_SESSION_CLIENT_CLASS (klass);
  session_client_class->connect = xfce_session_client_wl_xdg_connect;
  session_client_class->disconnect = xfce_session_client_wl_xdg_disconnect;
  session_client_class->is_connected = xfce_session_client_wl_xdg_is_connected;
  session_client_class->add_window = xfce_session_client_wl_xdg_add_window;
  session_client_class->restore_window = xfce_session_client_wl_xdg_restore_window;
  session_client_class->rename_window = xfce_session_client_wl_xdg_rename_window;
  session_client_class->remove_window = xfce_session_client_wl_xdg_remove_window;

#ifndef HAVE_FUNC_ATTRIBUTE_CONSTRUCTOR
  surfaces_init ();
#endif
}

static void
xfce_session_client_wl_xdg_init (XfceSessionClientWlXdg *wxsession_client)
{
  wxsession_client->session_status = SESSION_STATUS_INVALID;
  wxsession_client->toplevels = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) toplevel_free);
  wxsession_client->queued_toplevel_removes = g_queue_new ();
}

static void
xfce_session_client_wl_xdg_finalize (GObject *obj)
{
  XfceSessionClientWlXdg *wxsession_client = XFCE_SESSION_CLIENT_WL_XDG (obj);
  GHashTable *toplevels = wxsession_client->toplevels;
  GQueue *queued_toplevel_removes = wxsession_client->queued_toplevel_removes;

  // Parent calls _disconnect() which frees (almost) everything.
  G_OBJECT_CLASS (xfce_session_client_wl_xdg_parent_class)->finalize (obj);

  g_hash_table_destroy (toplevels);
  g_queue_free_full (queued_toplevel_removes, g_free);
}

static gboolean
xfce_session_client_wl_xdg_register (XfceSessionClientDBus *dsession_client,
                                     GError **error)
{
  XfceSessionClientWlXdg *wxsession_client = XFCE_SESSION_CLIENT_WL_XDG (dsession_client);

  XfsmDbusManager *manager_proxy = _xfce_session_client_dbus_get_xfsm_manager_proxy (dsession_client);
  g_assert (manager_proxy != NULL);
  const gchar *client_id = xfce_session_client_get_client_id (XFCE_SESSION_CLIENT (dsession_client));
  gchar *client_object_path = NULL;

  g_clear_pointer (&wxsession_client->start_reason, g_free);

  if (!xfsm_dbus_manager_call_attach_client_sync (manager_proxy,
                                                  client_id != NULL ? client_id : "",
                                                  &client_object_path,
                                                  &wxsession_client->start_reason,
                                                  NULL,
                                                  error))
    {
      return FALSE;
    }

  _xfce_session_client_dbus_set_client_object_path (dsession_client, client_object_path);
  g_free (client_object_path);

  return TRUE;
}

static gboolean
xfce_session_client_wl_xdg_connect (XfceSessionClient *session_client,
                                    GError **error)
{
  XfceSessionClientWlXdg *wxsession_client = XFCE_SESSION_CLIENT_WL_XDG (session_client);
  struct wl_event_queue *queue = NULL;

  if (wxsession_client->xdg_session == NULL)
    {
      _xfce_session_client_set_state (session_client, XFCE_SESSION_CLIENT_STATE_REGISTERING);
      wxsession_client->session_status = SESSION_STATUS_INVALID;

      struct wl_display *display = gdk_wayland_display_get_wl_display (gdk_display_get_default ());

      // Use a separate event queue so that our roundtrip calls don't cause GDK to
      // get events and possibly send other signals or do other things.
      queue = wl_display_create_queue_with_name (display, "xfce-session-client");

      wxsession_client->registry = wl_display_get_registry (display);
      wl_proxy_set_queue ((struct wl_proxy *) wxsession_client->registry, queue);
      wl_registry_add_listener (wxsession_client->registry, &registry_listener, wxsession_client);
      wl_display_roundtrip_queue (display, queue);

      if (wxsession_client->xdg_manager == NULL)
        {
          g_set_error_literal (error,
                               XFCE_SESSION_CLIENT_ERROR,
                               XFCE_SESSION_CLIENT_ERROR_FAILED,
                               _("Your compositor does not support the xdg-session-management-v1 protocol"));
          goto out_err;
        }

      const gchar *client_id = xfce_session_client_get_client_id (session_client);
      enum xdg_session_manager_v1_reason reason = client_id != NULL
                                                    ? XDG_SESSION_MANAGER_V1_REASON_SESSION_RESTORE
                                                    : XDG_SESSION_MANAGER_V1_REASON_LAUNCH;

      g_debug ("Registering session with compositor with ID %s, reason %s",
               client_id != NULL ? client_id : "(none)",
               reason_to_string (reason));
      wxsession_client->xdg_session = xdg_session_manager_v1_get_session (wxsession_client->xdg_manager, reason, client_id);
      xdg_session_v1_add_listener (wxsession_client->xdg_session, &session_listener, wxsession_client);

      gint64 start = g_get_monotonic_time ();
      while (wxsession_client->session_status == SESSION_STATUS_INVALID
             && g_get_monotonic_time () - start < SESSION_REGISTRATION_TIMEOUT)
        {
          if (wl_display_roundtrip_queue (display, queue) < 0)
            {
              break;
            }
        }

      if (wxsession_client->session_status == SESSION_STATUS_INVALID)
        {
          g_set_error_literal (error,
                               XFCE_SESSION_CLIENT_ERROR,
                               XFCE_SESSION_CLIENT_ERROR_FAILED,
                               _("Compositor failed to respond to registration attempt in time"));
          goto out_err;
        }
      else if (wxsession_client->session_status == SESSION_STATUS_REPLACED)
        {
          g_set_error_literal (error,
                               XFCE_SESSION_CLIENT_ERROR,
                               XFCE_SESSION_CLIENT_ERROR_FAILED,
                               _("Another application took over our session during registration"));
          goto out_err;
        }
      else if (xfce_session_client_get_client_id (session_client) == NULL)
        {
          g_set_error_literal (error,
                               XFCE_SESSION_CLIENT_ERROR,
                               XFCE_SESSION_CLIENT_ERROR_FAILED,
                               _("Compositor did not tell us our client ID"));
          goto out_err;
        }

      wl_proxy_set_queue ((struct wl_proxy *) wxsession_client->registry, NULL);
      wl_proxy_set_queue ((struct wl_proxy *) wxsession_client->xdg_manager, NULL);
      wl_proxy_set_queue ((struct wl_proxy *) wxsession_client->xdg_session, NULL);
      wl_display_dispatch_queue_pending (display, queue);
      g_clear_pointer (&queue, wl_event_queue_destroy);

      g_debug ("Registered session with compositor with ID %s, status %s",
               xfce_session_client_get_client_id (session_client),
               session_status_to_string (wxsession_client->session_status));

      for (GList *lp = g_queue_peek_head_link (wxsession_client->queued_toplevel_removes);
           lp != NULL;
           lp = lp->next)
        {
          gchar *name = lp->data;
          xdg_session_v1_remove_toplevel (wxsession_client->xdg_session, name);
        }
      g_queue_clear_full (wxsession_client->queued_toplevel_removes, g_free);

      GHashTableIter iter;
      g_hash_table_iter_init (&iter, wxsession_client->toplevels);
      const gchar *name = NULL;
      Toplevel *toplevel = NULL;
      while (g_hash_table_iter_next (&iter, (gpointer) &name, (gpointer) &toplevel))
        {
          if (gtk_widget_get_mapped (GTK_WIDGET (toplevel->window)))
            {
              if (toplevel->restore_from_name != NULL)
                {
                  g_message ("Window \"%s\" was mapped when connected to session manager; removing and re-adding to session. State will be lost",
                             gtk_window_get_title (toplevel->window));
                  xdg_session_v1_remove_toplevel (wxsession_client->xdg_session, toplevel->restore_from_name);
                  g_clear_pointer (&toplevel->restore_from_name, g_free);
                }

              WindowSurfaces *wsurfaces = surfaces_get_for_window (all_surfaces, toplevel->window);
              if (wsurfaces != NULL && wsurfaces->xdg_toplevel != NULL)
                {
                  toplevel->toplevel_session = xdg_session_v1_add_toplevel (wxsession_client->xdg_session, wsurfaces->xdg_toplevel, name);
                  toplevel->restore_from_name = g_strdup (name);
                }
            }
        }
    }

  if (!XFCE_SESSION_CLIENT_CLASS (xfce_session_client_wl_xdg_parent_class)->is_connected (session_client))
    {
      // _xfce_session_client_dbus_do_connect() will set is_resumed incorrectly, so we
      // have to fix it up afterward, but we don't want a property-notify to go out
      // for the incorrect setting.
      g_object_freeze_notify (G_OBJECT (session_client));

      GError *error1 = NULL;
      if (!_xfce_session_client_dbus_do_connect (XFCE_SESSION_CLIENT_DBUS (session_client),
                                                 xfce_session_client_wl_xdg_register,
                                                 &error1))
        {
          _xfce_session_client_set_state (session_client, XFCE_SESSION_CLIENT_STATE_IDLE);
          g_message ("Unable to connect to an Xfce-compatible session manager; some application state may not be saved: %s", error1->message);
          g_error_free (error1);
        }

      _xfce_session_client_set_is_resumed (session_client, wxsession_client->session_status == SESSION_STATUS_RESTORED);
      g_object_thaw_notify (G_OBJECT (session_client));
    }

  return TRUE;

out_err:

  xfce_session_client_wl_xdg_disconnect (session_client);
  _xfce_session_client_set_state (session_client, XFCE_SESSION_CLIENT_STATE_DISCONNECTED);
  if (queue != NULL)
    {
      wl_event_queue_destroy (queue);
    }

  return FALSE;
}

static void
xfce_session_client_wl_xdg_disconnect (XfceSessionClient *session_client)
{
  XFCE_SESSION_CLIENT_CLASS (xfce_session_client_wl_xdg_parent_class)->disconnect (session_client);

  XfceSessionClientWlXdg *wxsession_client = XFCE_SESSION_CLIENT_WL_XDG (session_client);

  GHashTableIter iter;
  g_hash_table_iter_init (&iter, wxsession_client->toplevels);
  Toplevel *toplevel = NULL;
  while (g_hash_table_iter_next (&iter, NULL, (gpointer) &toplevel))
    {
      g_clear_pointer (&toplevel->toplevel_session, xdg_toplevel_session_v1_destroy);
    }

  g_clear_pointer (&wxsession_client->xdg_session, xdg_session_v1_destroy);
  g_clear_pointer (&wxsession_client->xdg_manager, xdg_session_manager_v1_destroy);
  g_clear_pointer (&wxsession_client->registry, wl_registry_destroy);

  g_clear_pointer (&wxsession_client->start_reason, g_free);
  wxsession_client->session_status = SESSION_STATUS_INVALID;
}

static gboolean
xfce_session_client_wl_xdg_is_connected (XfceSessionClient *session_client)
{
  XfceSessionClientWlXdg *wxsession_client = XFCE_SESSION_CLIENT_WL_XDG (session_client);
  return wxsession_client->xdg_session != NULL;
}

static void
window_unmapped (GtkWidget *window,
                 XfceSessionClientWlXdg *wxsession_client)
{
  const gchar *name = g_object_get_qdata (G_OBJECT (window), TOPLEVEL_NAME_QUARK);
  Toplevel *toplevel = g_hash_table_lookup (wxsession_client->toplevels, name);
  if (toplevel != NULL)
    {
      g_clear_pointer (&toplevel->toplevel_session, xdg_toplevel_session_v1_destroy);
    }
}

static void
window_destroyed (GtkWidget *window,
                  XfceSessionClientWlXdg *wxsession_client)
{
  const gchar *name = g_object_get_qdata (G_OBJECT (window), TOPLEVEL_NAME_QUARK);
  if (name != NULL)
    {
      g_hash_table_remove (wxsession_client->toplevels, name);
    }
}

static gboolean
name_is_claimed (XfceSessionClientWlXdg *wxsession_client,
                 const gchar *name)
{
  if (g_hash_table_lookup (wxsession_client->toplevels, name) != NULL)
    {
      return TRUE;
    }

  GHashTableIter iter;
  g_hash_table_iter_init (&iter, wxsession_client->toplevels);
  Toplevel *value = NULL;
  while (g_hash_table_iter_next (&iter, NULL, (gpointer) &value))
    {
      if (g_strcmp0 (name, value->restore_from_name) == 0)
        {
          return TRUE;
        }
    }

  return FALSE;
}

static void
xfce_session_client_wl_xdg_register_window (XfceSessionClientWlXdg *wxsession_client,
                                            GtkWindow *window,
                                            const gchar *name,
                                            const gchar *restore_from_name)
{
  g_return_if_fail (g_object_get_qdata (G_OBJECT (window), TOPLEVEL_NAME_QUARK) == NULL);
  g_return_if_fail (!name_is_claimed (wxsession_client, name));

  Toplevel *toplevel = g_new0 (Toplevel, 1);
  toplevel->restore_from_name = g_strdup (restore_from_name);
  toplevel->window = window;
  toplevel->signal_user_data = wxsession_client;

  g_signal_connect (window, "unmap", G_CALLBACK (window_unmapped), wxsession_client);
  g_signal_connect (window, "destroy", G_CALLBACK (window_destroyed), wxsession_client);

  g_object_set_qdata_full (G_OBJECT (window), TOPLEVEL_NAME_QUARK, g_strdup (name), g_free);
  g_hash_table_insert (wxsession_client->toplevels, g_strdup (name), toplevel);

  if (restore_from_name == NULL && wxsession_client->xdg_session != NULL && gtk_widget_get_mapped (GTK_WIDGET (window)))
    {
      WindowSurfaces *wsurfaces = surfaces_get_for_window (all_surfaces, window);
      if (wsurfaces != NULL && wsurfaces->xdg_toplevel != NULL)
        {
          toplevel->toplevel_session = xdg_session_v1_add_toplevel (wxsession_client->xdg_session, wsurfaces->xdg_toplevel, name);
          toplevel->restore_from_name = g_strdup (name);
        }
    }
}

static void
xfce_session_client_wl_xdg_add_window (XfceSessionClient *session_client,
                                       GtkWindow *window,
                                       const gchar *name)
{
  xfce_session_client_wl_xdg_register_window (XFCE_SESSION_CLIENT_WL_XDG (session_client),
                                              window,
                                              name,
                                              NULL);
}

static void
xfce_session_client_wl_xdg_restore_window (XfceSessionClient *session_client,
                                           GtkWindow *window,
                                           const gchar *name)
{
  xfce_session_client_wl_xdg_register_window (XFCE_SESSION_CLIENT_WL_XDG (session_client),
                                              window,
                                              name,
                                              name);
}

static void
xfce_session_client_wl_xdg_rename_window (XfceSessionClient *session_client,
                                          GtkWindow *window,
                                          const gchar *new_name)
{
  XfceSessionClientWlXdg *wxsession_client = XFCE_SESSION_CLIENT_WL_XDG (session_client);

  const gchar *name = g_object_get_qdata (G_OBJECT (window), TOPLEVEL_NAME_QUARK);
  g_return_if_fail (name != NULL);
  g_return_if_fail (!name_is_claimed (wxsession_client, new_name));

  gchar *old_key = NULL;
  Toplevel *toplevel = NULL;
  if (g_hash_table_steal_extended (wxsession_client->toplevels, name, (gpointer) &old_key, (gpointer) &toplevel))
    {
      g_free (old_key);

      g_object_set_qdata_full (G_OBJECT (window), TOPLEVEL_NAME_QUARK, g_strdup (new_name), (GDestroyNotify) g_free);
      g_hash_table_insert (wxsession_client->toplevels, g_strdup (new_name), toplevel);

      if (toplevel->toplevel_session != NULL)
        {
          xdg_toplevel_session_v1_rename (toplevel->toplevel_session, new_name);
          g_free (toplevel->restore_from_name);
          toplevel->restore_from_name = g_strdup (new_name);
        }
    }
}

static void
xfce_session_client_wl_xdg_remove_window (XfceSessionClient *session_client,
                                          const gchar *name)
{
  XfceSessionClientWlXdg *wxsession_client = XFCE_SESSION_CLIENT_WL_XDG (session_client);

  const gchar *name_to_remove;
  Toplevel *toplevel = g_hash_table_lookup (wxsession_client->toplevels, name);
  if (toplevel != NULL)
    {
      name_to_remove = toplevel->restore_from_name != NULL ? toplevel->restore_from_name : name;
    }
  else
    {
      name_to_remove = name;
    }

  if (wxsession_client->xdg_session != NULL)
    {
      xdg_session_v1_remove_toplevel (wxsession_client->xdg_session, name_to_remove);
    }
  else
    {
      g_queue_push_tail (wxsession_client->queued_toplevel_removes, g_strdup (name_to_remove));
    }

  g_hash_table_remove (wxsession_client->toplevels, name);
}

static gboolean
intercept_xdg_wm_base_get_xdg_surface (gpointer data,
                                       struct wl_proxy *proxy,
                                       uint32_t opcode,
                                       struct wl_interface const *created_interface,
                                       uint32_t created_version,
                                       uint32_t flags,
                                       union wl_argument *args,
                                       struct wl_proxy **ret_proxy)
{
  *ret_proxy = libwayland_shim_marshal_passthrough (proxy,
                                                    opcode,
                                                    created_interface,
                                                    created_version,
                                                    flags,
                                                    args);

  if (args[1].o != NULL
      && g_strcmp0 (wl_proxy_get_interface ((struct wl_proxy *) args[1].o)->name, wl_surface_interface.name) == 0)
    {
      Surfaces *surfaces = data;
      surfaces_insert (surfaces, (struct wl_surface *) args[1].o, (struct xdg_surface *) *ret_proxy);
    }
  else
    {
      g_critical ("Expected 1st argument to xdg_wm_base.get_xdg_surface to be a wl_surface, but it is: '%s'",
                  args[1].o != NULL ? wl_proxy_get_interface ((struct wl_proxy *) args[1].o)->name : "(null)");
    }

  return TRUE;
}

static gboolean
intercept_xdg_surface_get_toplevel (gpointer data,
                                    struct wl_proxy *proxy,
                                    uint32_t opcode,
                                    struct wl_interface const *created_interface,
                                    uint32_t created_version,
                                    uint32_t flags,
                                    union wl_argument *args,
                                    struct wl_proxy **ret_proxy)
{
  *ret_proxy = libwayland_shim_marshal_passthrough (proxy,
                                                    opcode,
                                                    created_interface,
                                                    created_version,
                                                    flags,
                                                    args);

  if (g_strcmp0 (wl_proxy_get_interface (proxy)->name, xdg_surface_interface.name) == 0)
    {
      struct xdg_surface *xdg_surface = (struct xdg_surface *) proxy;

      Surfaces *surfaces = data;
      WindowSurfaces *wsurfaces = surfaces_get_for_xdg_surface (surfaces, xdg_surface);
      if (wsurfaces != NULL)
        {
          surfaces_set_xdg_toplevel (surfaces, wsurfaces, (struct xdg_toplevel *) *ret_proxy);

          if (XFCE_IS_SESSION_CLIENT_WL_XDG (xfce_session_client_get ()))
            {
              XfceSessionClientWlXdg *wxsession_client = XFCE_SESSION_CLIENT_WL_XDG (xfce_session_client_get ());
              if (wxsession_client->xdg_session != NULL)
                {
                  GHashTableIter iter;
                  g_hash_table_iter_init (&iter, wxsession_client->toplevels);
                  gchar *name = NULL;
                  Toplevel *toplevel = NULL;
                  while (g_hash_table_iter_next (&iter, (gpointer) &name, (gpointer) &toplevel))
                    {
                      GdkWindow *window = toplevel->window != NULL ? gtk_widget_get_window (GTK_WIDGET (toplevel->window)) : NULL;
                      struct wl_surface *wl_surface = window != NULL ? gdk_wayland_window_get_wl_surface (window) : NULL;

                      if (wl_surface == wsurfaces->wl_surface)
                        {
                          if (toplevel->restore_from_name != NULL)
                            {
                              toplevel->toplevel_session = xdg_session_v1_restore_toplevel (wxsession_client->xdg_session,
                                                                                            wsurfaces->xdg_toplevel,
                                                                                            toplevel->restore_from_name);
                              if (g_strcmp0 (toplevel->restore_from_name, name) != 0)
                                {
                                  xdg_toplevel_session_v1_rename (toplevel->toplevel_session, name);
                                  g_free (toplevel->restore_from_name);
                                  toplevel->restore_from_name = g_strdup (name);
                                }
                            }
                          else
                            {
                              toplevel->toplevel_session = xdg_session_v1_add_toplevel (wxsession_client->xdg_session,
                                                                                        wsurfaces->xdg_toplevel,
                                                                                        name);
                              toplevel->restore_from_name = g_strdup (name);
                            }

                          break;
                        }
                    }
                }
            }
        }
      else
        {
          g_message ("Failed to find WindowSurfaces for xdg_surface; XfceSessionClient may misbehave");
        }
    }
  else
    {
      g_critical ("Expected proxy to xdg_surface.get_toplevel to be a xdg_surface, but it is: '%s'",
                  wl_proxy_get_interface (proxy)->name);
    }

  return TRUE;
}

static gboolean
intercept_xdg_surface_destroy (gpointer data,
                               struct wl_proxy *proxy,
                               uint32_t opcode,
                               struct wl_interface const *created_interface,
                               uint32_t created_version,
                               uint32_t flags,
                               union wl_argument *args,
                               struct wl_proxy **ret_proxy)
{
  if (g_strcmp0 (wl_proxy_get_interface (proxy)->name, xdg_surface_interface.name) == 0)
    {
      Surfaces *surfaces = data;
      surfaces_xdg_surface_destroyed (surfaces, (struct xdg_surface *) proxy);
    }
  else
    {
      g_critical ("Expected proxy to xdg_surface.destroy to be a xdg_surface, but it is: '%s'",
                  wl_proxy_get_interface (proxy)->name);
    }

  return FALSE;
}

static gboolean
intercept_xdg_toplevel_destroy (gpointer data,
                                struct wl_proxy *proxy,
                                uint32_t opcode,
                                struct wl_interface const *created_interface,
                                uint32_t created_version,
                                uint32_t flags,
                                union wl_argument *args,
                                struct wl_proxy **ret_proxy)
{
  if (g_strcmp0 (wl_proxy_get_interface (proxy)->name, xdg_toplevel_interface.name) == 0)
    {
      Surfaces *surfaces = data;
      surfaces_xdg_toplevel_destroyed (surfaces, (struct xdg_toplevel *) proxy);
    }
  else
    {
      g_critical ("Expected proxy to xdg_toplevel.destroy to be a xdg_toplevel, but it is: '%s'",
                  wl_proxy_get_interface (proxy)->name);
    }

  return FALSE;
}

static void
registry_global (void *data,
                 struct wl_registry *wl_registry,
                 uint32_t name,
                 const char *interface,
                 uint32_t version)
{
  XfceSessionClientWlXdg *wxsession_client = XFCE_SESSION_CLIENT_WL_XDG (data);

  if (g_strcmp0 (interface, xdg_session_manager_v1_interface.name) == 0 && wxsession_client->xdg_manager == NULL)
    {
      wxsession_client->xdg_manager = wl_registry_bind (wl_registry, name, &xdg_session_manager_v1_interface, 1);
    }
}

static void
registry_global_remove (void *data,
                        struct wl_registry *wl_registry,
                        uint32_t name)
{
}

static void
session_created (void *data,
                 struct xdg_session_v1 *xdg_session_v1,
                 const char *session_id)
{
  _xfce_session_client_set_client_id (XFCE_SESSION_CLIENT (data), session_id);
  XfceSessionClientWlXdg *wxsession_client = XFCE_SESSION_CLIENT_WL_XDG (data);
  wxsession_client->session_status = SESSION_STATUS_CREATED;
}

static void
session_restored (void *data,
                  struct xdg_session_v1 *xdg_session_v1)
{
  XfceSessionClientWlXdg *wxsession_client = XFCE_SESSION_CLIENT_WL_XDG (data);
  wxsession_client->session_status = SESSION_STATUS_RESTORED;
}
static void
session_replaced (void *data,
                  struct xdg_session_v1 *xdg_session_v1)
{
  xfce_session_client_disconnect (XFCE_SESSION_CLIENT (data));
  XfceSessionClientWlXdg *wxsession_client = XFCE_SESSION_CLIENT_WL_XDG (data);
  wxsession_client->session_status = SESSION_STATUS_REPLACED;
}

static const gchar *
reason_to_string (enum xdg_session_manager_v1_reason reason)
{
  switch (reason)
    {
    case XDG_SESSION_MANAGER_V1_REASON_LAUNCH:
      return "LAUNCH";
    case XDG_SESSION_MANAGER_V1_REASON_RECOVER:
      return "RECOVER";
    case XDG_SESSION_MANAGER_V1_REASON_SESSION_RESTORE:
      return "SESSION_RESTORE";
    default:
      return "UNKNOWN";
    }
}

static const gchar *
session_status_to_string (SessionStatus status)
{
  switch (status)
    {
    case SESSION_STATUS_CREATED:
      return "CREATED";
    case SESSION_STATUS_RESTORED:
      return "RESTORED";
    case SESSION_STATUS_REPLACED:
      return "REPLACED";
    default:
      return "INVALID";
    }
}

static void
toplevel_free (Toplevel *toplevel)
{
  if (toplevel->window != NULL)
    {
      g_object_set_qdata (G_OBJECT (toplevel->window), TOPLEVEL_NAME_QUARK, NULL);
      g_signal_handlers_disconnect_by_data (toplevel->window, toplevel->signal_user_data);
    }
  if (toplevel->toplevel_session != NULL)
    {
      xdg_toplevel_session_v1_destroy (toplevel->toplevel_session);
    }
  g_free (toplevel->restore_from_name);
  g_free (toplevel);
}

static GQuark
toplevel_name_quark (void)
{
  static GQuark q = 0;

  if (q == 0)
    {
      q = g_quark_from_static_string ("--xfce-session-client-wl-xdg-toplevel-name");
    }
  return q;
}

#ifdef HAVE_FUNC_ATTRIBUTE_CONSTRUCTOR
__attribute__ ((constructor))
#endif
static void
surfaces_init (void)
{
  Surfaces *surfaces = g_new0 (Surfaces, 1);

  surfaces->surfaces_by_wl_surface = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
  surfaces->surfaces_by_xdg_surface = g_hash_table_new (g_direct_hash, g_direct_equal);
  surfaces->surfaces_by_xdg_toplevel = g_hash_table_new (g_direct_hash, g_direct_equal);

  libwayland_shim_install_request_hook (&xdg_wm_base_interface, XDG_WM_BASE_GET_XDG_SURFACE, intercept_xdg_wm_base_get_xdg_surface, surfaces);
  libwayland_shim_install_request_hook (&xdg_surface_interface, XDG_SURFACE_GET_TOPLEVEL, intercept_xdg_surface_get_toplevel, surfaces);
  libwayland_shim_install_request_hook (&xdg_surface_interface, XDG_SURFACE_DESTROY, intercept_xdg_surface_destroy, surfaces);
  libwayland_shim_install_request_hook (&xdg_toplevel_interface, XDG_TOPLEVEL_DESTROY, intercept_xdg_toplevel_destroy, surfaces);

  all_surfaces = surfaces;
}

static WindowSurfaces *
surfaces_insert (Surfaces *surfaces,
                 struct wl_surface *wl_surface,
                 struct xdg_surface *xdg_surface)
{
  WindowSurfaces *wsurfaces = g_hash_table_lookup (surfaces->surfaces_by_wl_surface, wl_surface);
  if (wsurfaces != NULL)
    {
      g_hash_table_remove (surfaces->surfaces_by_xdg_surface, wsurfaces->xdg_surface);
      g_hash_table_remove (surfaces->surfaces_by_xdg_toplevel, wsurfaces->xdg_toplevel);
    }
  else
    {
      wsurfaces = g_new0 (WindowSurfaces, 1);
      wsurfaces->wl_surface = wl_surface;
      g_hash_table_insert (surfaces->surfaces_by_wl_surface, wsurfaces->wl_surface, wsurfaces);
    }

  wsurfaces->xdg_surface = xdg_surface;
  wsurfaces->xdg_toplevel = NULL;

  g_hash_table_insert (surfaces->surfaces_by_xdg_surface, wsurfaces->xdg_surface, wsurfaces);

  return wsurfaces;
}

static void
surfaces_set_xdg_toplevel (Surfaces *surfaces,
                           WindowSurfaces *wsurfaces,
                           struct xdg_toplevel *xdg_toplevel)
{
  wsurfaces->xdg_toplevel = xdg_toplevel;
  g_hash_table_insert (surfaces->surfaces_by_xdg_toplevel, xdg_toplevel, wsurfaces);
}

static WindowSurfaces *
surfaces_get_for_window (Surfaces *surfaces,
                         GtkWindow *window)
{
  GdkWindow *gdkwindow = gtk_widget_get_window (GTK_WIDGET (window));
  struct wl_surface *wl_surface = gdkwindow != NULL ? gdk_wayland_window_get_wl_surface (gdkwindow) : NULL;
  return wl_surface != NULL ? surfaces_get_for_wl_surface (surfaces, wl_surface) : NULL;
}

static WindowSurfaces *
surfaces_get_for_wl_surface (Surfaces *surfaces,
                             struct wl_surface *wl_surface)
{
  return g_hash_table_lookup (surfaces->surfaces_by_wl_surface, wl_surface);
}

static WindowSurfaces *
surfaces_get_for_xdg_surface (Surfaces *surfaces,
                              struct xdg_surface *xdg_surface)
{
  return g_hash_table_lookup (surfaces->surfaces_by_xdg_surface, xdg_surface);
}

static void
surfaces_xdg_surface_destroyed (Surfaces *surfaces,
                                struct xdg_surface *xdg_surface)
{
  WindowSurfaces *wsurfaces = g_hash_table_lookup (surfaces->surfaces_by_xdg_surface, xdg_surface);
  if (wsurfaces != NULL)
    {
      wsurfaces->xdg_surface = NULL;
      g_hash_table_remove (surfaces->surfaces_by_xdg_surface, xdg_surface);

      if (wsurfaces->xdg_toplevel == NULL)
        {
          g_hash_table_remove (surfaces->surfaces_by_wl_surface, wsurfaces->wl_surface);
        }
    }
}

static void
surfaces_xdg_toplevel_destroyed (Surfaces *surfaces,
                                 struct xdg_toplevel *xdg_toplevel)
{
  WindowSurfaces *wsurfaces = g_hash_table_lookup (surfaces->surfaces_by_xdg_toplevel, xdg_toplevel);
  if (wsurfaces != NULL)
    {
      wsurfaces->xdg_toplevel = NULL;
      g_hash_table_remove (surfaces->surfaces_by_xdg_toplevel, xdg_toplevel);

      if (wsurfaces->xdg_surface == NULL)
        {
          g_hash_table_remove (surfaces->surfaces_by_wl_surface, wsurfaces->wl_surface);
        }
    }
}
