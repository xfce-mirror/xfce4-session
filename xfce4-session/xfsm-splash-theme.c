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

#include <math.h>
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include <xfce4-session/xfsm-splash-theme.h>


#define DEFAULT_BGCOLOR   "White"
#define DEFAULT_FGCOLOR   "Black"

struct _XfsmSplashTheme
{
  GdkColor bgcolor;
  GdkColor fgcolor;
  gchar   *logo_file;
};


XfsmSplashTheme*
xfsm_splash_theme_load (const gchar *name)
{
  XfsmSplashTheme *theme;
  const gchar *logo_file;
  gchar *resource;
  gchar *file = NULL;
  XfceRc *rc;

  theme = g_new0 (XfsmSplashTheme, 1);

  resource = xfce_get_homefile (".themes", name, "xfsm4", "themerc", NULL);
  if (g_file_test (resource, G_FILE_TEST_IS_REGULAR))
    file = g_strdup (resource);
  g_free (resource);

  if (file == NULL)
    {
      resource = g_strdup_printf ("themes/%s/xfsm4/themerc", name);
      file = xfce_resource_lookup (XFCE_RESOURCE_DATA, resource);
      g_free (resource);
    }

  if (file != NULL)
    {
      rc = xfce_rc_simple_open (file, TRUE);
      if (rc == NULL)
        {
          g_free (file);
          goto set_defaults;
        }

      gdk_color_parse (xfce_rc_read_entry (rc, "bgcolor", DEFAULT_BGCOLOR),
                       &theme->bgcolor);
      gdk_color_parse (xfce_rc_read_entry (rc, "fgcolor", DEFAULT_FGCOLOR),
                       &theme->fgcolor);

      logo_file = xfce_rc_read_entry (rc, "logo", NULL);

      if (logo_file != NULL)
        {
          resource = g_path_get_dirname (file);
          theme->logo_file = g_build_filename (resource, logo_file, NULL);
          g_free (resource);
        }
      else
        {
          theme->logo_file = NULL;
        }

      xfce_rc_close (rc);
      g_free (file);

      return theme;
    }

set_defaults:
  gdk_color_parse (DEFAULT_BGCOLOR, &theme->bgcolor);
  gdk_color_parse (DEFAULT_FGCOLOR, &theme->fgcolor);
  theme->logo_file = NULL;

  return theme;
}


XfsmSplashTheme*
xfsm_splash_theme_copy (const XfsmSplashTheme *theme)
{
  XfsmSplashTheme *new_theme;

  new_theme = g_new0 (XfsmSplashTheme, 1);
  new_theme->bgcolor = theme->bgcolor;
  new_theme->fgcolor = theme->fgcolor;

  if (theme->logo_file != NULL)
    new_theme->logo_file = g_strdup (theme->logo_file);

  return new_theme;
}


void
xfsm_splash_theme_get_bgcolor (const XfsmSplashTheme *theme,
                               GdkColor *color_return)
{
  memcpy (color_return, &theme->bgcolor, sizeof (*color_return));
}


void
xfsm_splash_theme_get_fgcolor (const XfsmSplashTheme *theme,
                               GdkColor *color_return)
{
  memcpy (color_return, &theme->fgcolor, sizeof (*color_return));
}


GdkPixbuf*
xfsm_splash_theme_get_logo (const XfsmSplashTheme *theme,
                            gint available_width,
                            gint available_height)
{
  GdkPixbuf *scaled;
  GdkPixbuf *logo;
  gint logo_width;
  gint logo_height;
  gdouble wratio;
  gdouble hratio;

  if (theme->logo_file == NULL)
    return NULL;

  logo = gdk_pixbuf_new_from_file (theme->logo_file, NULL);
  if (logo == NULL)
    return NULL;

  logo_width = gdk_pixbuf_get_width (logo);
  logo_height = gdk_pixbuf_get_height (logo);

  if (logo_width > available_width || logo_height > available_height)
    {
      wratio = (gdouble) logo_width / (gdouble) available_width;
      hratio = (gdouble) logo_height / (gdouble) available_height;

      if (hratio > wratio)
        {
          logo_width = rint (logo_width / hratio);
          logo_height = available_height;
        }
      else
        {
          logo_width = available_width;
          logo_height = rint (logo_height / wratio);
        }

      scaled = gdk_pixbuf_scale_simple (logo,
                                        logo_width,
                                        logo_height,
                                        GDK_INTERP_BILINEAR);
      g_object_unref (logo);
      logo = scaled;
    }

  return logo;
}


void
xfsm_splash_theme_destroy (XfsmSplashTheme *theme)
{
  if (theme->logo_file != NULL)
    g_free (theme->logo_file);
  g_free (theme);
}


