/*
 * Copyright (c) 2007 The Xfce Development Team
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

/**
 * SECTION:libxfce4session-client-config
 * @title: libxfce4session-client config
 * @short_description: libxfce4session-client config macros
 * @stability: Stable
 * @include: libxfce4session-client/libxfce4session-client.h
 *
 * Variables and functions to check the libxfce4session-client version.
 **/

#include "libxfce4session-client-config.h"
#include "libxfce4session-client-visibility.h"


/**
 * libxfce4session_client_major_version:
 *
 * A constat that evaluates to the major version of libxfce4session-client.
 *
 */
const guint libxfce4session_client_major_version = LIBXFCE4SESSION_CLIENT_MAJOR_VERSION;

/**
 * libxfce4session_client_minor_version:
 *
 * A constat that evaluates to the minor version of libxfce4session-client.
 *
 */
const guint libxfce4session_client_minor_version = LIBXFCE4SESSION_CLIENT_MINOR_VERSION;

/**
 * libxfce4session_client_micro_version:
 *
 * A constat that evaluates to the micro version of libxfce4session-client.
 *
 */
const guint libxfce4session_client_micro_version = LIBXFCE4SESSION_CLIENT_MICRO_VERSION;



/**
 * libxfce4session_client_check_version:
 * @required_major: the required major version.
 * @required_minor: the required minor version.
 * @required_micro: the required micro version.
 *
 * Checks that the <systemitem class="library">libxfce4session-client</systemitem> library
 * in use is compatible with the given version. Generally you would pass in
 * the constants #LIBXFCE4SESSION_CLIENT_MAJOR_VERSION, #LIBXFCE4SESSION_CLIENT_MINOR_VERSION and
 * #LIBXFCE4SESSION_CLIENT_MICRO_VERSION as the three arguments to this function; that produces
 * a check that the library in use is compatible with the version of
 * <systemitem class="library">libxfce4session-client</systemitem> the extension was
 * compiled against.
 *
 * <example>
 * <title>Checking the runtime version of the libxfce4session-client library</title>
 * <programlisting>
 * const gchar *mismatch;
 * mismatch = libxfce4session_client_check_version (LIBXFCE4SESSION_CLIENT_MAJOR_VERSION,
 *                                      LIBXFCE4SESSION_CLIENT_MINOR_VERSION,
 *                                      LIBXFCE4SESSION_CLIENT_MICRO_VERSION);
 * if (G_UNLIKELY (mismatch != NULL))
 *   g_error ("Version mismatch: %<!---->s", mismatch);
 * </programlisting>
 * </example>
 *
 * Return value: %NULL if the library is compatible with the given version,
 *               or a string describing the version mismatch. The returned
 *               string is owned by the library and must not be freed or
 *               modified by the caller.
 **/
const gchar *
libxfce4session_client_check_version (guint required_major,
                                      guint required_minor,
                                      guint required_micro)
{
  if (required_major > LIBXFCE4SESSION_CLIENT_MAJOR_VERSION)
    return "libxfce4session-client version too old (major mismatch)";
  if (required_major < LIBXFCE4SESSION_CLIENT_MAJOR_VERSION)
    return "libxfce4session-client version too new (major mismatch)";
  if (required_minor > LIBXFCE4SESSION_CLIENT_MINOR_VERSION)
    return "libxfce4session-client version too old (minor mismatch)";
  if (required_minor == LIBXFCE4SESSION_CLIENT_MINOR_VERSION && required_micro > LIBXFCE4SESSION_CLIENT_MICRO_VERSION)
    return "libxfce4session-client version too old (micro mismatch)";
  return NULL;
}



#define __LIBXFCE4SESSION_CLIENT_CONFIG_C__
#include "libxfce4session-client-visibility.c"
