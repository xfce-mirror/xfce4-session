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

#include <libxfce4util/libxfce4util.h>

#include <engines/balou/balou-theme.h>


static void       load_color_pair (const XfceRc *rc,
                                   const gchar  *name,
                                   GdkColor     *color1_return, 
                                   GdkColor     *color2_return,
                                   const gchar  *color_default);
static GdkPixbuf  *load_pixbuf    (const gchar *path,
                                   gint         available_width,
                                   gint         available_height);


struct _BalouTheme
{
  GdkColor  bgcolor1;
  GdkColor  bgcolor2;
  GdkColor  fgcolor;
  gchar    *name;
  gchar    *description;
  gchar    *font;
  gchar    *theme_file;
  gchar    *logo_file;
};


#define DEFAULT_BGCOLOR "White"
#define DEFAULT_FGCOLOR "Black"
#define DEFAULT_FONT    "Sans Bold 12"


BalouTheme*
balou_theme_load (const gchar *name)
{
  BalouTheme  *theme;
  const gchar *image_file;
  const gchar *spec;
  gchar       *resource;
  gchar       *file;
  XfceRc      *rc;

  theme = g_new0 (BalouTheme, 1);

  resource = g_strdup_printf ("%s/balou/themerc", name);
  file = xfce_resource_lookup (XFCE_RESOURCE_THEMES, resource);
  g_free (resource);

  if (file != NULL)
    {
      rc = xfce_rc_simple_open (file, TRUE);
      if (rc == NULL)
        {
          g_free (file);
          goto set_defaults;
        }

      theme->theme_file = g_strdup (file);

      xfce_rc_set_group (rc, "Info");
      theme->name = g_strdup (xfce_rc_read_entry (rc, "Name", name));
      theme->description = g_strdup (xfce_rc_read_entry (rc, "Description", _("No description given")));

      xfce_rc_set_group (rc, "Splash Screen");
      load_color_pair (rc, "bgcolor", &theme->bgcolor1, &theme->bgcolor2,
                       DEFAULT_BGCOLOR);

      spec = xfce_rc_read_entry (rc, "fgcolor", DEFAULT_FGCOLOR);
      if (!gdk_color_parse (spec, &theme->fgcolor))
        gdk_color_parse (DEFAULT_FGCOLOR, &theme->fgcolor);

      spec = xfce_rc_read_entry (rc, "font", DEFAULT_FONT);
      theme->font = g_strdup (spec);

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

      xfce_rc_close (rc);
      g_free (file);

      return theme;
    }

set_defaults:
  gdk_color_parse (DEFAULT_BGCOLOR, &theme->bgcolor1);
  gdk_color_parse (DEFAULT_BGCOLOR, &theme->bgcolor2);
  gdk_color_parse (DEFAULT_FGCOLOR, &theme->fgcolor);
  theme->font = g_strdup (DEFAULT_FONT);
  theme->logo_file = NULL;

  return theme;
}


const gchar*
balou_theme_get_name (const BalouTheme *theme)
{
  return theme->name;
}


const gchar*
balou_theme_get_description (const BalouTheme *theme)
{
  return theme->description;
}


const gchar*
balou_theme_get_font (const BalouTheme *theme)
{
  return theme->font;
}


void
balou_theme_get_bgcolor (const BalouTheme *theme,
                         GdkColor         *color_return)
{
  *color_return = theme->bgcolor1;
}


void
balou_theme_get_fgcolor (const BalouTheme *theme,
                         GdkColor         *color_return)
{
  *color_return = theme->fgcolor;
}


GdkPixbuf*
balou_theme_get_logo (const BalouTheme *theme,
                      gint              available_width,
                      gint              available_height)
{
  return load_pixbuf (theme->logo_file,
                      available_width,
                      available_height);
}


void
balou_theme_draw_gradient (const BalouTheme *theme,
                           GdkDrawable      *drawable,
                           GdkGC            *gc,
                           GdkRectangle      logobox,
                           GdkRectangle      textbox)
{
  GdkColor color;
  gint     dred;
  gint     dgreen;
  gint     dblue;
  gint     i;

  if (gdk_color_equal (&theme->bgcolor1, &theme->bgcolor2))
    {
      gdk_gc_set_rgb_fg_color (gc, &theme->bgcolor1);
      gdk_draw_rectangle (drawable, gc, TRUE, logobox.x, logobox.y,
                          logobox.width, logobox.height);
      gdk_draw_rectangle (drawable, gc, TRUE, textbox.x, textbox.y,
                          textbox.width, textbox.height);
    }
  else
    {
      /* calculate differences */
      dred = theme->bgcolor1.red - theme->bgcolor2.red;
      dgreen = theme->bgcolor1.green - theme->bgcolor2.green;
      dblue = theme->bgcolor1.blue - theme->bgcolor2.blue;

      for (i = 0; i < logobox.height; ++i)
        {
          color.red = theme->bgcolor2.red + (i * dred / logobox.height);
          color.green = theme->bgcolor2.green + (i * dgreen / logobox.height);
          color.blue = theme->bgcolor2.blue + (i * dblue / logobox.height);

          gdk_gc_set_rgb_fg_color (gc, &color);
          gdk_draw_line (drawable, gc, logobox.x, logobox.y + i,
                         logobox.x + logobox.width, logobox.y + i);
        }

    if (textbox.width != 0 && textbox.height != 0)
      {
        gdk_gc_set_rgb_fg_color (gc, &theme->bgcolor1);
        gdk_draw_rectangle (drawable, gc, TRUE, textbox.x, textbox.y,
                            textbox.width, textbox.height);
      }
    }
}


void
balou_theme_destroy (BalouTheme *theme)
{
  if (theme->name != NULL)
    g_free (theme->name);
  if (theme->description != NULL)
    g_free (theme->description);
  if (theme->theme_file != NULL)
    g_free (theme->theme_file);
  if (theme->logo_file != NULL)
    g_free (theme->logo_file);
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
load_pixbuf (const gchar *path,
             gint         available_width,
             gint         available_height)
{
  static char *suffixes[] = { "svg", "png", "jpeg", "jpg", "xpm", NULL };
  GdkPixbuf *scaled;
  GdkPixbuf *pb = NULL;
  gint pb_width;
  gint pb_height;
  gdouble wratio;
  gdouble hratio;
  gchar *file;
  guint n;

  if (G_UNLIKELY (path == NULL))
    return NULL;

  pb = gdk_pixbuf_new_from_file (path, NULL);
  if (G_UNLIKELY (pb == NULL))
    {
      for (n = 0; pb == NULL && suffixes[n] != NULL; ++n)
        {
          file = g_strdup_printf ("%s.%s", path, suffixes[n]);
          pb = gdk_pixbuf_new_from_file (file, NULL);
          g_free (file);
        }
    }

  if (G_UNLIKELY (pb == NULL))
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


