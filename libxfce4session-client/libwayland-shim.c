/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Sophie Winter
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define _GNU_SOURCE

#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libwayland-shim.h"
#if WAYLAND_VERSION_MAJOR < 1 || (WAYLAND_VERSION_MAJOR == 1 && WAYLAND_VERSION_MINOR < 24)
#include "stolen-from-libwayland.h"
#endif

// More than this library needs, to be safe.
#define MAX_HOOKS 16

struct request_hook
{
  const char *interface_name;
  uint32_t opcode;
  libwayland_shim_request_handler_func_t handler;
  gpointer data;
};

static struct wl_proxy *(*real_wl_proxy_marshal_array_flags) (
  struct wl_proxy *proxy,
  uint32_t opcode,
  const struct wl_interface *created_interface,
  uint32_t created_version,
  uint32_t flags,
  union wl_argument *args) = NULL;

static struct request_hook request_hooks[MAX_HOOKS];
static gint n_request_hooks = 0;

static gpointer
libwayland_shim_init_once (gpointer data)
{
#define INIT_SYM(name) \
  if (!(real_##name = dlsym (RTLD_NEXT, #name))) \
    { \
      g_warning ("libwayland_shim: dlsym failed to load %s", #name); \
      return GUINT_TO_POINTER (FALSE); \
    }

  INIT_SYM (wl_proxy_marshal_array_flags);

#undef INIT_SYM

  return GUINT_TO_POINTER (TRUE);
}

static inline gboolean
libwayland_shim_init (void)
{
  static GOnce once = G_ONCE_INIT;

  g_once (&once, libwayland_shim_init_once, NULL);
  return GPOINTER_TO_UINT (once.retval);
}

void
libwayland_shim_install_request_hook (
  struct wl_interface const *interface,
  uint32_t opcode,
  libwayland_shim_request_handler_func_t handler,
  gpointer data)
{
  g_return_if_fail (interface != NULL);
  g_return_if_fail (handler != NULL);

  g_assert (n_request_hooks < MAX_HOOKS);

  if (libwayland_shim_init ())
    {
      request_hooks[n_request_hooks] = (struct request_hook) {
        .interface_name = interface->name,
        .opcode = opcode,
        .handler = handler,
        .data = data,
      };
      g_atomic_int_add (&n_request_hooks, 1);
    }
}

struct wl_proxy *
libwayland_shim_marshal_passthrough (
  struct wl_proxy *proxy,
  uint32_t opcode,
  const struct wl_interface *interface,
  uint32_t version,
  uint32_t flags,
  union wl_argument *args)
{
  gboolean shim_inited = libwayland_shim_init ();
  g_assert (shim_inited);
  return real_wl_proxy_marshal_array_flags (proxy, opcode, interface, version, flags, args);
}

static struct wl_proxy *
validate_request_result (
  struct wl_proxy *created_proxy,
  struct wl_proxy *request_proxy,
  uint32_t opcode,
  struct wl_interface const *create_interface,
  uint32_t create_version,
  uint32_t flags,
  union wl_argument *args)
{
  if (create_interface)
    {
      if (!created_proxy)
        {
          const struct wl_interface *interface = wl_proxy_get_interface (request_proxy);
          g_critical (
            "libwayland_shim: request %s.%s should have created object of type %s, but handler created nothing; forwarding to compositor",
            interface->name,
            interface->methods[opcode].name,
            create_interface->name);
          return real_wl_proxy_marshal_array_flags (request_proxy, opcode, create_interface, create_version, flags, args);
        }
      else if (strcmp (wl_proxy_get_interface (created_proxy)->name, create_interface->name) != 0)
        {
          const struct wl_interface *request_interface = wl_proxy_get_interface (request_proxy);
          const struct wl_interface *created_interface = wl_proxy_get_interface (created_proxy);
          g_critical (
            "libwayland_shim: request %s.%s should have created object of type %s, but handler created object of type %s; forwarding to compositor",
            request_interface->name,
            request_interface->methods[opcode].name,
            create_interface->name,
            created_interface->name);
          wl_proxy_destroy (created_proxy);
          return real_wl_proxy_marshal_array_flags (request_proxy, opcode, create_interface, create_version, flags, args);
        }
      else
        {
          return created_proxy;
        }
    }
  else
    {
      if (created_proxy)
        {
          const struct wl_interface *request_interface = wl_proxy_get_interface (request_proxy);
          const struct wl_interface *created_interface = wl_proxy_get_interface (created_proxy);
          g_warning (
            "libwayland_shim: request %s.%s should not have created anything, but handler created object of type %s\n",
            request_interface->name,
            request_interface->methods[opcode].name,
            created_interface->name);
          wl_proxy_destroy (created_proxy);
        }
      return NULL;
    }
}

// Overrides the function in wayland-client.c in libwayland, handles requests made by the client program and optionally
// forwards them to libwayland/the compositor
__attribute__ ((__visibility__ ("default"))) struct wl_proxy *
wl_proxy_marshal_array_flags (
  struct wl_proxy *proxy,
  uint32_t opcode,
  struct wl_interface const *create_interface,
  uint32_t create_version,
  uint32_t flags,
  union wl_argument *args)
{
  if (libwayland_shim_init ())
    {
      gint n_hooks = g_atomic_int_get (&n_request_hooks);
      if (n_hooks > 0)
        {
          const struct wl_interface *interface = wl_proxy_get_interface (proxy);
          for (gint i = 0; i < n_hooks; i++)
            {
              if (request_hooks[i].opcode == opcode && g_strcmp0 (request_hooks[i].interface_name, interface->name) == 0)
                {
                  struct wl_proxy *ret_proxy = NULL;
                  if (request_hooks[i].handler (request_hooks[i].data,
                                                proxy,
                                                opcode,
                                                create_interface,
                                                create_version,
                                                flags,
                                                args,
                                                &ret_proxy))
                    {
                      return validate_request_result (ret_proxy, proxy, opcode, create_interface, create_version, flags, args);
                    }
                }
            }
        }

      // Forward the request on to libwayland without modification, this is the most common path
      return real_wl_proxy_marshal_array_flags (proxy, opcode, create_interface, create_version, flags, args);
    }
  else
    {
      g_error ("libshim-wayland initialization failed; we have no way to forward requests to libwayland");
    }
}
