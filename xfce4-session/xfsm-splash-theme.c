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

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include <math.h>
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
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
  gchar   *name;
  gchar   *description;
  gchar   *theme_file;
  gchar   *logo_file;
  gchar   *chooser_icon_file;
  gchar   *skip_icon_file;
};


static time_t mtime (const gchar *path);
static GdkPixbuf *load_cached_preview (const XfsmSplashTheme *theme);
static void store_cached_preview (const XfsmSplashTheme *theme,
                                  GdkPixbuf             *pixbuf);
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

  resource = g_strdup_printf ("%s/xfsm4/themerc", name);
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

  if (theme->name != NULL)
    new_theme->name = g_strdup (theme->name);

  if (theme->description != NULL)
    new_theme->description = g_strdup (theme->description);

  if (theme->theme_file != NULL)
    new_theme->theme_file = g_strdup (theme->theme_file);

  if (theme->logo_file != NULL)
    new_theme->logo_file = g_strdup (theme->logo_file);

  if (theme->chooser_icon_file != NULL)
    new_theme->chooser_icon_file = g_strdup (theme->chooser_icon_file);

  if (theme->skip_icon_file != NULL)
    new_theme->skip_icon_file = g_strdup (theme->skip_icon_file);

  return new_theme;
}


const gchar*
xfsm_splash_theme_get_name (const XfsmSplashTheme *theme)
{
  return theme->name;
}


const gchar*
xfsm_splash_theme_get_description (const XfsmSplashTheme *theme)
{
  return theme->description;
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
  gint     dred;
  gint     dgreen;
  gint     dblue;
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
      /* calculate differences */
      dred = theme->bgcolor1.red - theme->bgcolor2.red;
      dgreen = theme->bgcolor1.green - theme->bgcolor2.green;
      dblue = theme->bgcolor1.blue - theme->bgcolor2.blue;

      for (i = 0; i < height; ++i, ++y)
        {
          color.red = theme->bgcolor2.red + (i * dred / height);
          color.green = theme->bgcolor2.green + (i * dgreen / height);
          color.blue = theme->bgcolor2.blue + (i * dblue / height);

          gdk_gc_set_rgb_fg_color (gc, &color);
          gdk_draw_line (drawable, gc, x, y, x + width, y);
        }
    }

  g_object_unref (G_OBJECT (gc));
}


GdkPixbuf*
xfsm_splash_theme_generate_preview (const XfsmSplashTheme *theme,
                                    gint                   width,
                                    gint                   height)
{
#define WIDTH   320
#define HEIGHT  240

  GdkPixmap *pixmap;
  GdkPixbuf *pixbuf;
  GdkPixbuf *scaled;
  GdkWindow *root;
  GdkGC     *gc;
  gint       pw, ph;

  /* check for a cached preview first */
  scaled = load_cached_preview (theme);
  if (scaled != NULL)
    return scaled;

  root = gdk_screen_get_root_window (gdk_screen_get_default ());
  pixmap = gdk_pixmap_new (GDK_DRAWABLE (root), WIDTH, HEIGHT, -1);
  xfsm_splash_theme_draw_gradient (theme, GDK_DRAWABLE (pixmap),
                                   0, 0, WIDTH, HEIGHT);

  pixbuf = xfsm_splash_theme_get_logo (theme, WIDTH, HEIGHT);
  if (pixbuf != NULL)
    {
      gc = gdk_gc_new (GDK_DRAWABLE (pixmap));
      gdk_gc_set_function (gc, GDK_COPY);

      pw = gdk_pixbuf_get_width (pixbuf);
      ph = gdk_pixbuf_get_height (pixbuf);

      gdk_draw_pixbuf (GDK_DRAWABLE (pixmap), gc, pixbuf, 0, 0,
                       (WIDTH - pw) / 2, (HEIGHT - ph) / 2,
                       pw, ph, GDK_RGB_DITHER_NONE, 0, 0);

      g_object_unref (G_OBJECT (pixbuf));
      g_object_unref (G_OBJECT (gc));
    }

  pixbuf = gdk_pixbuf_get_from_drawable (NULL, GDK_DRAWABLE (pixmap),
                                         NULL, 0, 0, 0, 0, WIDTH, HEIGHT);
  scaled = gdk_pixbuf_scale_simple (pixbuf, width, height, GDK_INTERP_BILINEAR);

  g_object_unref (pixbuf);
  g_object_unref (pixmap);

  /* store preview */
  store_cached_preview (theme, scaled);

  return scaled;

#undef WIDTH
#undef HEIGHT
}


void
xfsm_splash_theme_destroy (XfsmSplashTheme *theme)
{
  if (theme->name != NULL)
    g_free (theme->name);
  if (theme->description != NULL)
    g_free (theme->description);
  if (theme->theme_file != NULL)
    g_free (theme->theme_file);
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
  static char *suffixes[] = { "svg", "png", "jpeg", "jpg", "xpm", NULL };
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


static time_t
mtime (const gchar *path)
{
  struct stat sb;

  if (path == NULL || stat (path, &sb) < 0)
    return (time_t) 0;

  return sb.st_mtime;
}


static GdkPixbuf*
load_cached_preview (const XfsmSplashTheme *theme)
{
  GdkPixbuf *pixbuf;
  gchar     *resource;
  gchar     *preview;

  resource = g_strconcat ("splash-theme-preview-", theme->name, ".png", NULL);
  preview = xfce_resource_lookup (XFCE_RESOURCE_CACHE, resource);
  g_free (resource);

  if (preview == NULL)
    return NULL;

  if ((mtime (preview) < mtime (theme->theme_file))
      || (theme->logo_file != NULL
        && (mtime (preview) < mtime (theme->logo_file))))
    {
      /* preview is outdated, need to regenerate preview */
      unlink (preview);
      g_free (preview);

      return NULL;
    }

  pixbuf = gdk_pixbuf_new_from_file (preview, NULL);
  g_free (preview);

  return pixbuf;
}


static void
store_cached_preview (const XfsmSplashTheme *theme,
                      GdkPixbuf             *pixbuf)
{
  gchar *resource;
  gchar *preview;

  resource = g_strconcat ("splash-theme-preview-", theme->name, ".png", NULL);
  preview = xfce_resource_save_location (XFCE_RESOURCE_CACHE, resource, TRUE);
  g_free (resource);

  if (preview != NULL)
    {
      gdk_pixbuf_save (pixbuf, preview, "png", NULL, NULL);
      g_free (preview);
    }
}



