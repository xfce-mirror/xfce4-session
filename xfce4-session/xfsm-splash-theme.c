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
#define DEFAULT_FOCOLOR   "White"


struct _XfsmSplashTheme
{
  GdkColor bgcolor1;
  GdkColor bgcolor2;
  GdkColor fgcolor;
  GdkColor focolor1;
  GdkColor focolor2;
  gchar   *logo_file;
  gchar   *chooser_icon_file;
  gchar   *skip_icon_file;
};


static void load_color_pair (const XfceRc *rc,
                             const gchar  *name,
                             GdkColor     *color1_return,
                             GdkColor     *color2_return,
                             const gchar  *color_default);
static GdkPixbuf *xfsm_splash_theme_load_pixbuf (const gchar *path,
                                                 gint         available_width,
                                                 gint         available_height);


XfsmSplashTheme*
xfsm_splash_theme_load (const gchar *name)
{
  XfsmSplashTheme *theme;
  const gchar *image_file;
  const gchar *spec;
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

      xfce_rc_set_group (rc, "Splash Screen");
      load_color_pair (rc, "bgcolor", &theme->bgcolor1, &theme->bgcolor2,
                       DEFAULT_BGCOLOR);

      spec = xfce_rc_read_entry (rc, "fgcolor", DEFAULT_FGCOLOR);
      if (!gdk_color_parse (spec, &theme->fgcolor))
        gdk_color_parse (DEFAULT_FGCOLOR, &theme->fgcolor);

      image_file = xfce_rc_read_entry (rc, "logo", NULL);
      if (image_file != NULL)
        {
          resource = g_path_get_dirname (file);
          theme->logo_file = g_build_filename (resource, image_file, NULL);
          g_free (resource);
        }
      else
        {
          theme->logo_file = NULL;
        }

      image_file = xfce_rc_read_entry (rc, "chooser_icon", NULL);
      if (image_file != NULL)
        {
          resource = g_path_get_dirname (file);
          theme->chooser_icon_file = g_build_filename (resource, image_file,
                                                       NULL);
          g_free (resource);
        }
      else
        {
          theme->chooser_icon_file = NULL;
        }

      image_file = xfce_rc_read_entry (rc, "skip_icon", NULL);
      if (image_file != NULL)
        {
          resource = g_path_get_dirname (file);
          theme->skip_icon_file = g_build_filename (resource, image_file, NULL);
          g_free (resource);
        }
      else
        {
          theme->skip_icon_file = NULL;
        }

      xfce_rc_set_group (rc, "Logout Screen");
      load_color_pair (rc, "focolor", &theme->focolor1, &theme->focolor2,
                       DEFAULT_FOCOLOR);

      xfce_rc_close (rc);
      g_free (file);

      return theme;
    }

set_defaults:
  gdk_color_parse (DEFAULT_BGCOLOR, &theme->bgcolor1);
  gdk_color_parse (DEFAULT_BGCOLOR, &theme->bgcolor2);
  gdk_color_parse (DEFAULT_FGCOLOR, &theme->fgcolor);
  gdk_color_parse (DEFAULT_FOCOLOR, &theme->focolor1);
  gdk_color_parse (DEFAULT_FOCOLOR, &theme->focolor2);
  theme->logo_file = NULL;

  return theme;
}


XfsmSplashTheme*
xfsm_splash_theme_copy (const XfsmSplashTheme *theme)
{
  XfsmSplashTheme *new_theme;

  new_theme = (XfsmSplashTheme *) g_memdup (theme, sizeof (*theme));

  if (theme->logo_file != NULL)
    new_theme->logo_file = g_strdup (theme->logo_file);

  if (theme->chooser_icon_file != NULL)
    new_theme->chooser_icon_file = g_strdup (theme->chooser_icon_file);

  if (theme->skip_icon_file != NULL)
    new_theme->skip_icon_file = g_strdup (theme->skip_icon_file);

  return new_theme;
}


void
xfsm_splash_theme_get_bgcolor (const XfsmSplashTheme *theme,
                               GdkColor *color_return)
{
  memcpy (color_return, &theme->bgcolor1, sizeof (*color_return));
}


void
xfsm_splash_theme_get_fgcolor (const XfsmSplashTheme *theme,
                               GdkColor *color_return)
{
  memcpy (color_return, &theme->fgcolor, sizeof (*color_return));
}


void
xfsm_splash_theme_get_focolors (const XfsmSplashTheme *theme,
                                GdkColor *color1_return,
                                GdkColor *color2_return)
{
  memcpy (color1_return, &theme->focolor1, sizeof (*color1_return));
  memcpy (color2_return, &theme->focolor2, sizeof (*color2_return));
}


GdkPixbuf*
xfsm_splash_theme_get_logo (const XfsmSplashTheme *theme,
                            gint available_width,
                            gint available_height)
{
  return xfsm_splash_theme_load_pixbuf (theme->logo_file,
                                        available_width,
                                        available_height);
}


GdkPixbuf*
xfsm_splash_theme_get_skip_icon (const XfsmSplashTheme *theme,
                                 gint available_width,
                                 gint available_height)
{
  return xfsm_splash_theme_load_pixbuf (theme->skip_icon_file,
                                        available_width,
                                        available_height);
}


GdkPixbuf*
xfsm_splash_theme_get_chooser_icon (const XfsmSplashTheme *theme,
                                    gint available_width,
                                    gint available_height)
{
  return xfsm_splash_theme_load_pixbuf (theme->chooser_icon_file,
                                        available_width,
                                        available_height);
}


void
xfsm_splash_theme_draw_gradient (const XfsmSplashTheme *theme,
                                 GdkDrawable *drawable,
                                 gint x,
                                 gint y,
                                 gint width,
                                 gint height)
{
  GdkColor color;
  GdkGC   *gc;
  gint     i;

  gc = gdk_gc_new (drawable);
  if (gc == NULL)
    return;

  if (gdk_color_equal (&theme->bgcolor1, &theme->bgcolor2))
    {
      gdk_gc_set_rgb_fg_color (gc, &theme->bgcolor1);
      gdk_draw_rectangle (drawable, gc, TRUE, x, y, width, height);
    }
  else
    {
      for (i = y; i < y + height; ++i)
        {
          color.red = theme->bgcolor2.red + (i * (theme->bgcolor1.red
                - theme->bgcolor2.red) / height);
          color.green = theme->bgcolor2.green + (i * (theme->bgcolor1.green
                - theme->bgcolor2.green) / height);
          color.blue = theme->bgcolor2.blue + (i * (theme->bgcolor1.blue
                - theme->bgcolor2.blue) / height);

          gdk_gc_set_rgb_fg_color (gc, &color);
          gdk_draw_rectangle (drawable, gc, TRUE, x, y + i, width, 1);
        }
    }

  g_object_unref (G_OBJECT (gc));
}


void
xfsm_splash_theme_destroy (XfsmSplashTheme *theme)
{
  if (theme->logo_file != NULL)
    g_free (theme->logo_file);
  if (theme->chooser_icon_file != NULL)
    g_free (theme->chooser_icon_file);
  if (theme->skip_icon_file != NULL)
    g_free (theme->skip_icon_file);
  g_free (theme);
}


static void
load_color_pair (const XfceRc *rc,
                 const gchar  *name,
                 GdkColor     *color1_return, 
                 GdkColor     *color2_return,
                 const gchar  *color_default)
{
  const gchar *spec;
  gchar      **s;

  spec = xfce_rc_read_entry (rc, name, color_default);
  if (spec == NULL)
    {
      gdk_color_parse (color_default, color1_return);
      gdk_color_parse (color_default, color2_return);
    }
  else
    {
      s = g_strsplit (spec, ":", 2);

      if (s[0] == NULL)
        {
          gdk_color_parse (color_default, color1_return);
          gdk_color_parse (color_default, color2_return);
        }
      else if (s[1] == NULL)
        {
          if (!gdk_color_parse (s[0], color1_return))
            gdk_color_parse (color_default, color1_return);
          *color2_return = *color1_return;
        }
      else
        {
          if (!gdk_color_parse (s[0], color2_return))
            gdk_color_parse (color_default, color2_return);
          if (!gdk_color_parse (s[1], color1_return))
            *color1_return = *color2_return;
        }

      g_strfreev (s);
    }
}


static GdkPixbuf*
xfsm_splash_theme_load_pixbuf (const gchar *path,
                               gint available_width,
                               gint available_height)
{
  static char *suffixes[] = { "svg", "png", "jpeg", "xpm", NULL };
  GdkPixbuf *scaled;
  GdkPixbuf *pb = NULL;
  gint pb_width;
  gint pb_height;
  gdouble wratio;
  gdouble hratio;
  gchar *file;
  guint n;

  if (path == NULL)
    return NULL;

  if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
    {
      pb = gdk_pixbuf_new_from_file (path, NULL);
    }

  if (pb == NULL)
    {
      for (n = 0; pb == NULL && suffixes[n] != NULL; ++n)
        {
          file = g_strdup_printf ("%s.%s", path, suffixes[n]);
          pb = gdk_pixbuf_new_from_file (file, NULL);
          g_free (file);
        }
    }

  if (pb == NULL)
    return NULL;

  pb_width = gdk_pixbuf_get_width (pb);
  pb_height = gdk_pixbuf_get_height (pb);

  if (pb_width > available_width || pb_height > available_height)
    {
      wratio = (gdouble) pb_width / (gdouble) available_width;
      hratio = (gdouble) pb_height / (gdouble) available_height;

      if (hratio > wratio)
        {
          pb_width = rint (pb_width / hratio);
          pb_height = available_height;
        }
      else
        {
          pb_width = available_width;
          pb_height = rint (pb_height / wratio);
        }

      scaled = gdk_pixbuf_scale_simple (pb,
                                        pb_width,
                                        pb_height,
                                        GDK_INTERP_BILINEAR);
      g_object_unref (pb);
      pb = scaled;
    }

  return pb;
}



