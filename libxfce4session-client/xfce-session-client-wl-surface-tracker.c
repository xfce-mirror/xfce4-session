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

#include "libwayland-shim.h"
#include "stolen-from-libwayland.h"
#include "xfce-session-client-wl-surface-tracker.h"

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


static Surfaces *all_surfaces = NULL;
static XdgToplevelRealizedCallback xdg_toplevel_realized_callback = NULL;


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
surfaces_get_for_wl_surface (Surfaces *surfaces,
                             struct wl_surface *wl_surface)
{
  return g_hash_table_lookup (surfaces->surfaces_by_wl_surface, wl_surface);
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

          if (xdg_toplevel_realized_callback != NULL)
            {
              xdg_toplevel_realized_callback (wsurfaces->wl_surface, wsurfaces->xdg_toplevel);
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

#ifdef HAVE_FUNC_ATTRIBUTE_CONSTRUCTOR
__attribute__ ((constructor))
#endif
static void
surfaces_init_internal (void)
{
  if (all_surfaces == NULL)
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
}

void
surfaces_init (XdgToplevelRealizedCallback callback)
{
  xdg_toplevel_realized_callback = callback;

#ifndef HAVE_FUNC_ATTRIBUTE_CONSTRUCTOR
  surfaces_init_internal ();
#endif
}

struct xdg_toplevel *
surfaces_get_xdg_toplevel_for_window (GtkWindow *window)
{
  WindowSurfaces *wsurfaces = surfaces_get_for_window (all_surfaces, window);
  return wsurfaces != NULL ? wsurfaces->xdg_toplevel : NULL;
}
