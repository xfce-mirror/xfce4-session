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

#pragma once

#include <glib-object.h>
#include <wayland-client-core.h>

// Function type for optionally overriding the behavior of requests from the client program (our process). *ret_proxy
// always starts as NULL. If TRUE is returned this request is considered handled and *ret_proxy must be either left as
// NULL or set to a newly created proxy (whichever is required by the specific request being handled). If FALSE is
// returned libwayland_shim falls back to it's default behavior, which may create a stub proxy to return if needed.
typedef gboolean (*libwayland_shim_request_handler_func_t) (
  gpointer data,
  struct wl_proxy *proxy,
  uint32_t opcode,
  struct wl_interface const *created_interface,
  uint32_t created_version,
  uint32_t flags,
  union wl_argument *args,
  struct wl_proxy **ret_proxy);

// The handler will be called and can optionally handle all matching requests. There is no way to uninstall a hook, and
// installing too many hooks may hurt performance.
void
libwayland_shim_install_request_hook (
  struct wl_interface const *interface,
  uint32_t opcode,
  libwayland_shim_request_handler_func_t handler,
  gpointer data);

// Sends the request to the compositor as-is.
struct wl_proxy *
libwayland_shim_marshal_passthrough (
  struct wl_proxy *proxy,
  uint32_t opcode,
  const struct wl_interface *interface,
  uint32_t version,
  uint32_t flags,
  union wl_argument *args);
