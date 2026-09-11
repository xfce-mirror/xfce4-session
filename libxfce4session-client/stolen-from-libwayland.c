/*
 * Copyright © 2008-2012 Kristian Høgsberg
 * Copyright © 2010-2012 Intel Corporation
 * Copyright © 2011 Benjamin Franzke
 * Copyright © 2012 Collabora, Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "stolen-from-libwayland.h"

// wayland-private.h
#define WL_CLOSURE_MAX_ARGS 20

// wayland-private.h
enum wl_arg_type
{
  WL_ARG_INT = 'i',
  WL_ARG_UINT = 'u',
  WL_ARG_FIXED = 'f',
  WL_ARG_STRING = 's',
  WL_ARG_OBJECT = 'o',
  WL_ARG_NEW_ID = 'n',
  WL_ARG_ARRAY = 'a',
  WL_ARG_FD = 'h',
};

// wayland-private.h
struct argument_details
{
  enum wl_arg_type type;
  int nullable;
};

// connection.c
static const char *
get_next_argument (const char *signature, struct argument_details *details)
{
  details->nullable = 0;
  for (; *signature; ++signature)
    {
      switch (*signature)
        {
        case WL_ARG_INT:
        case WL_ARG_UINT:
        case WL_ARG_FIXED:
        case WL_ARG_STRING:
        case WL_ARG_OBJECT:
        case WL_ARG_NEW_ID:
        case WL_ARG_ARRAY:
        case WL_ARG_FD:
          details->type = *signature;
          return signature + 1;
        case '?':
          details->nullable = 1;
        }
    }
  details->type = '\0';
  return signature;
}

// connection.c
static void
wl_argument_from_va_list (const char *signature, union wl_argument *args, int count, va_list ap)
{
  int i;
  const char *sig_iter;
  struct argument_details arg;

  sig_iter = signature;
  for (i = 0; i < count; i++)
    {
      sig_iter = get_next_argument (sig_iter, &arg);

      switch (arg.type)
        {
        case WL_ARG_INT:
          args[i].i = va_arg (ap, int32_t);
          break;
        case WL_ARG_UINT:
          args[i].u = va_arg (ap, uint32_t);
          break;
        case WL_ARG_FIXED:
          args[i].f = va_arg (ap, wl_fixed_t);
          break;
        case WL_ARG_STRING:
          args[i].s = va_arg (ap, const char *);
          break;
        case WL_ARG_OBJECT:
          args[i].o = va_arg (ap, struct wl_object *);
          break;
        case WL_ARG_NEW_ID:
          args[i].o = va_arg (ap, struct wl_object *);
          break;
        case WL_ARG_ARRAY:
          args[i].a = va_arg (ap, struct wl_array *);
          break;
        case WL_ARG_FD:
          args[i].h = va_arg (ap, int32_t);
          break;
        }
    }
}

// The following functions may be unnecessary, as they are unmodified from libwayland, however it is possible that some
// builds of libwayland could inline the call they make to wl_proxy_marshal_array_flags(), which means we need to
// override all functions that call it.

// wayland-client.c
__attribute__ ((__visibility__ ("default"))) struct wl_proxy *
wl_proxy_marshal_flags (struct wl_proxy *proxy, uint32_t opcode, const struct wl_interface *interface, uint32_t version, uint32_t flags, ...)
{
  union wl_argument args[WL_CLOSURE_MAX_ARGS];
  va_list ap;
  const struct wl_interface *proxy_interface = wl_proxy_get_interface (proxy);

  va_start (ap, flags);
  wl_argument_from_va_list (proxy_interface->methods[opcode].signature,
                            args, WL_CLOSURE_MAX_ARGS, ap);
  va_end (ap);

  return wl_proxy_marshal_array_flags (proxy, opcode, interface, version, flags, args);
}

// wayland-client.c
__attribute__ ((__visibility__ ("default"))) void
wl_proxy_marshal (struct wl_proxy *proxy, uint32_t opcode, ...)
{
  union wl_argument args[WL_CLOSURE_MAX_ARGS];
  va_list ap;
  const struct wl_interface *proxy_interface = wl_proxy_get_interface (proxy);

  va_start (ap, opcode);
  wl_argument_from_va_list (proxy_interface->methods[opcode].signature,
                            args, WL_CLOSURE_MAX_ARGS, ap);
  va_end (ap);

  wl_proxy_marshal_array_constructor (proxy, opcode, args, NULL);
}

// wayland-client.c
__attribute__ ((__visibility__ ("default"))) void
wl_proxy_marshal_array (struct wl_proxy *proxy, uint32_t opcode, union wl_argument *args)
{
  wl_proxy_marshal_array_constructor (proxy, opcode, args, NULL);
}

// wayland-client.c
__attribute__ ((__visibility__ ("default"))) struct wl_proxy *
wl_proxy_marshal_constructor (struct wl_proxy *proxy,
                              uint32_t opcode,
                              const struct wl_interface *interface,
                              ...)
{
  union wl_argument args[WL_CLOSURE_MAX_ARGS];
  va_list ap;
  const struct wl_interface *proxy_interface = wl_proxy_get_interface (proxy);

  va_start (ap, interface);
  wl_argument_from_va_list (proxy_interface->methods[opcode].signature,
                            args, WL_CLOSURE_MAX_ARGS, ap);
  va_end (ap);

  return wl_proxy_marshal_array_constructor (proxy, opcode,
                                             args, interface);
}

// wayland-client.c
__attribute__ ((__visibility__ ("default"))) struct wl_proxy *
wl_proxy_marshal_constructor_versioned (struct wl_proxy *proxy,
                                        uint32_t opcode,
                                        const struct wl_interface *interface,
                                        uint32_t version,
                                        ...)
{
  union wl_argument args[WL_CLOSURE_MAX_ARGS];
  va_list ap;
  const struct wl_interface *proxy_interface = wl_proxy_get_interface (proxy);

  va_start (ap, version);
  wl_argument_from_va_list (proxy_interface->methods[opcode].signature,
                            args, WL_CLOSURE_MAX_ARGS, ap);
  va_end (ap);

  return wl_proxy_marshal_array_constructor_versioned (proxy, opcode,
                                                       args, interface,
                                                       version);
}

// wayland-client.c
__attribute__ ((__visibility__ ("default"))) struct wl_proxy *
wl_proxy_marshal_array_constructor (struct wl_proxy *proxy,
                                    uint32_t opcode,
                                    union wl_argument *args,
                                    const struct wl_interface *interface)
{
  return wl_proxy_marshal_array_constructor_versioned (proxy, opcode,
                                                       args, interface,
                                                       wl_proxy_get_version (proxy));
}

// wayland-client.c
__attribute__ ((__visibility__ ("default"))) struct wl_proxy *
wl_proxy_marshal_array_constructor_versioned (struct wl_proxy *proxy,
                                              uint32_t opcode,
                                              union wl_argument *args,
                                              const struct wl_interface *interface,
                                              uint32_t version)
{
  return wl_proxy_marshal_array_flags (proxy, opcode, interface, version, 0, args);
}
