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

#include <X11/Xlib.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include <xfce4-session/xfsm-chooser-trash.h>


static void xfsm_chooser_trash_class_init (XfsmChooserTrashClass *klass);
static void xfsm_chooser_trash_init (XfsmChooserTrash *trash);
static void xfsm_chooser_trash_finalize (GObject *object);
static void xfsm_chooser_trash_get_property (GObject      *object,
                                             guint         prop_id,
                                             GValue       *value,
                                             GParamSpec   *pspec);
static void xfsm_chooser_trash_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec);
static gboolean xfsm_chooser_trash_drag_drop (GtkWidget        *widget,
                                                   GdkDragContext   *context,
                                                   gint              x,
                                                   gint              y,
                                                   guint             time);
static void xfsm_chooser_trash_drag_leave (GtkWidget      *widget,
                                           GdkDragContext *context,
                                           guint           time);
static gboolean xfsm_chooser_trash_drag_motion (GtkWidget      *widget,
                                                GdkDragContext *context,
                                                gint            x,
                                                gint            y,
                                                guint           time);
static gboolean xfsm_chooser_trash_expose_event (GtkWidget      *widget,
                                                 GdkEventExpose *event);
static void xfsm_chooser_trash_map (GtkWidget *widget);
static void xfsm_chooser_trash_realize (GtkWidget *widget);
static void xfsm_chooser_trash_unrealize (GtkWidget *widget);
static GdkFilterReturn xfsm_chooser_trash_filter (GdkXEvent *xevent,
                                                  GdkEvent  *event,
                                                  gpointer   data);
static void xfsm_chooser_trash_grab (XfsmChooserTrash *trash,
                                     gint              x,
                                     gint              y);


enum 
{
  PROP_ZERO,
  PROP_BACKGROUND,
};


static GObjectClass *parent_class;

static GtkTargetEntry dnd_targets[] =
{
  { "application/xfsm4-session", 0, 0 },
};

static guint dnd_ntargets = sizeof (dnd_targets) / sizeof (*dnd_targets);


GType
xfsm_chooser_trash_get_type (void)
{
  static GType trash_type = 0;

  if (trash_type == 0)
    {
      static const GTypeInfo trash_info = {
        sizeof (XfsmChooserTrashClass),
        NULL,
        NULL,
        (GClassInitFunc) xfsm_chooser_trash_class_init,
        NULL,
        NULL,
        sizeof (XfsmChooserTrash),
        0,
        (GInstanceInitFunc) xfsm_chooser_trash_init,
      };

      trash_type = g_type_register_static (GTK_TYPE_WINDOW,
                                           "XfsmChooserTrash",
                                           &trash_info,
                                           0);
    }

  return trash_type;
}


static void
xfsm_chooser_trash_class_init (XfsmChooserTrashClass *klass)
{
  GtkWidgetClass *widget_class;
  GObjectClass   *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfsm_chooser_trash_finalize;
  gobject_class->get_property = xfsm_chooser_trash_get_property;
  gobject_class->set_property = xfsm_chooser_trash_set_property;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->drag_drop = xfsm_chooser_trash_drag_drop;
  widget_class->drag_leave = xfsm_chooser_trash_drag_leave;
  widget_class->drag_motion = xfsm_chooser_trash_drag_motion;
  widget_class->expose_event = xfsm_chooser_trash_expose_event;
  widget_class->map = xfsm_chooser_trash_map;
  widget_class->realize = xfsm_chooser_trash_realize;
  widget_class->unrealize = xfsm_chooser_trash_unrealize;

  parent_class = gtk_type_class (gtk_window_get_type ());

  g_object_class_install_property (gobject_class, PROP_BACKGROUND,
      g_param_spec_object ("background", _("Background window"),
        _("Background window"), GDK_TYPE_DRAWABLE,
        G_PARAM_READABLE | G_PARAM_WRITABLE));
}


static void
xfsm_chooser_trash_init (XfsmChooserTrash *trash)
{
  GdkPixbuf *pb;

  trash->icon_normal = xfce_themed_icon_load ("xfsm-trash-normal", 72);
  trash->icon_drop = xfce_themed_icon_load ("xfsm-trash-hilight", 72);

  if (trash->icon_normal == NULL)
    {
      pb = gtk_widget_render_icon (GTK_WIDGET (trash),
                                   GTK_STOCK_DELETE,
                                   GTK_ICON_SIZE_DND,
                                   NULL);
      trash->icon_normal = gdk_pixbuf_scale_simple (pb, 72, 72,
                                                    GDK_INTERP_HYPER);
      g_object_unref (G_OBJECT (pb));
    }

  if (trash->icon_drop == NULL)
    {
      pb = gtk_widget_render_icon (GTK_WIDGET (trash),
                                   GTK_STOCK_DELETE,
                                   GTK_ICON_SIZE_DND,
                                   NULL);
      trash->icon_drop = gdk_pixbuf_scale_simple (pb, 72, 72,
                                                  GDK_INTERP_HYPER);
      g_object_unref (G_OBJECT (pb));
    }

  gtk_widget_set_size_request (GTK_WIDGET (trash),
                               gdk_pixbuf_get_width (trash->icon_drop),
                               gdk_pixbuf_get_height (trash->icon_drop));
}


static void
xfsm_chooser_trash_finalize (GObject *object)
{
  XfsmChooserTrash *trash;

  g_return_if_fail (XFSM_IS_CHOOSER_TRASH (object));

  trash = XFSM_CHOOSER_TRASH (object);

  g_object_unref (G_OBJECT (trash->icon_normal));
  g_object_unref (G_OBJECT (trash->icon_drop));

  if (trash->background != NULL)
    g_object_unref (G_OBJECT (trash->background));

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
xfsm_chooser_trash_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  XfsmChooserTrash *trash;

  trash = XFSM_CHOOSER_TRASH (object);

  switch (prop_id)
    {
    case PROP_BACKGROUND:
      g_value_set_object (value, trash->background);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
xfsm_chooser_trash_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  XfsmChooserTrash *trash;

  trash = XFSM_CHOOSER_TRASH (object);

  switch (prop_id)
    {
    case PROP_BACKGROUND:
      if (trash->background != NULL)
        g_object_unref (G_OBJECT (trash->background));
      trash->background = GDK_DRAWABLE (g_value_dup_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static gboolean
xfsm_chooser_trash_drag_drop (GtkWidget        *widget,
                                       GdkDragContext   *context,
                                       gint              x,
                                       gint              y,
                                       guint             time)
{
  gtk_drag_finish (context, TRUE, TRUE, time);
  return TRUE;
}


static void
xfsm_chooser_trash_drag_leave (GtkWidget      *widget,
                               GdkDragContext *context,
                               guint           time)
{
  XfsmChooserTrash *trash;

  trash = XFSM_CHOOSER_TRASH (widget);

  if (trash->drag_hilight)
    {
      trash->drag_hilight = FALSE;
      gtk_widget_queue_draw (widget);
    }
}


static gboolean
xfsm_chooser_trash_drag_motion (GtkWidget      *widget,
                                GdkDragContext *context,
                                gint            x,
                                gint            y,
                                guint           time)
{
  XfsmChooserTrash *trash;

  trash = XFSM_CHOOSER_TRASH (widget);

  if (!trash->drag_hilight)
    {
      trash->drag_hilight = TRUE;
      gtk_drag_unhighlight (widget);
      gtk_widget_queue_draw (widget);
    }

  return TRUE;
}


static gboolean
xfsm_chooser_trash_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
  XfsmChooserTrash *trash;
  gint width, height;
  GdkPixbuf *icon;

  trash = XFSM_CHOOSER_TRASH (widget);

  gdk_drawable_get_size (GDK_DRAWABLE (widget->window), &width, &height);

  if (trash->drag_hilight)
    icon = trash->icon_drop;
  else
    icon = trash->icon_normal;

  /* FIXME: nasty hack to get around the fact that sometimes right after
   * the Xserver is up, the background is drawn white instead of using
   * my background pixmap, I suppose my GtkStyle is wrong. More research
   * here is required.
   */
  if (trash->background != NULL)
    {
      gdk_draw_drawable (GDK_DRAWABLE (widget->window),
                         widget->style->base_gc[GTK_STATE_NORMAL],
                         trash->pixmap,
                         0, 0, 0, 0,
                         width, height);
    }

  gdk_draw_pixbuf (GDK_DRAWABLE (widget->window),
                   widget->style->base_gc[GTK_STATE_NORMAL],
                   icon,
                   0, 0,
                   0, 0,
                   width,
                   height,
                   GDK_RGB_DITHER_NONE,
                   0, 0);

  return FALSE;
}


static void
xfsm_chooser_trash_map (GtkWidget *widget)
{
  XfsmChooserTrash *trash;
  gint x, y;

  trash = XFSM_CHOOSER_TRASH (widget);

  if (trash->background != NULL)
    {
      gtk_window_get_position (GTK_WINDOW (trash), &x, &y);
      xfsm_chooser_trash_grab (trash, x, y);
    }

  GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void
xfsm_chooser_trash_realize (GtkWidget *widget)
{
  XfsmChooserTrash *trash;
  GtkStyle *style;
  gint w, h;
  gint n;

  trash = XFSM_CHOOSER_TRASH (widget);

  if (trash->background != NULL)
    {
      gtk_widget_get_size_request (widget, &w, &h);

      trash->pixmap = gdk_pixmap_new (GDK_DRAWABLE (trash->background),
                                      w, h, -1);

      style = gtk_style_copy (widget->style);

      for (n = 0; n < 5; ++n)
        {
          if (style->bg_pixmap[n] != NULL)
            g_object_unref (G_OBJECT (style->bg_pixmap[n]));
          style->bg_pixmap[n] = trash->pixmap;
          g_object_ref (G_OBJECT (trash->pixmap));
        }

      g_object_unref (widget->style);
      widget->style = style;
    }

  GTK_WIDGET_CLASS (parent_class)->realize (widget);

  if (trash->background != NULL)
    {
      gdk_window_set_back_pixmap (widget->window, trash->pixmap, FALSE);
      gdk_window_add_filter (widget->window, xfsm_chooser_trash_filter, trash);
    }

  gtk_drag_dest_set (widget, GTK_DEST_DEFAULT_ALL,
                     dnd_targets, dnd_ntargets,
                     GDK_ACTION_MOVE);
}


static void
xfsm_chooser_trash_unrealize (GtkWidget *widget)
{
  XfsmChooserTrash *trash;

  trash = XFSM_CHOOSER_TRASH (widget);

  if (trash->background != NULL)
    {
      gdk_window_remove_filter (widget->window,
                                xfsm_chooser_trash_filter,
                                trash);

      g_object_unref (G_OBJECT (trash->pixmap));
      trash->pixmap = NULL;
    }

  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}


static GdkFilterReturn
xfsm_chooser_trash_filter (GdkXEvent *xevent,
                           GdkEvent  *event,
                           gpointer   data)
{
  XfsmChooserTrash *trash;
  XConfigureEvent  *xev = (XConfigureEvent *) xevent;

  trash = XFSM_CHOOSER_TRASH (data);

  switch (xev->type)
    {
    case ConfigureNotify:
      if (trash->background != NULL)
        xfsm_chooser_trash_grab (trash, xev->x, xev->y);
      break;
    }

  return GDK_FILTER_CONTINUE;
}


static void
xfsm_chooser_trash_grab (XfsmChooserTrash *trash,
                         gint              x,
                         gint              y)
{
  gint w, h;
  GdkGC *gc;

  gdk_drawable_get_size (GDK_DRAWABLE (trash->pixmap), &w, &h);

  gc = gdk_gc_new (GDK_DRAWABLE (trash->pixmap));
  gdk_gc_set_function (gc, GDK_COPY);

  gdk_draw_drawable (GDK_DRAWABLE (trash->pixmap), gc,
                     GDK_DRAWABLE (trash->background),
                     x, y, 0, 0, w, h);

  g_object_unref (G_OBJECT (gc));
}



