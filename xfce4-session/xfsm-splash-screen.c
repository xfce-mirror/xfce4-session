/* $Id$ */
/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gmodule.h>

#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include <libxfsm/xfsm-splash-engine.h>
#include <libxfsm/xfsm-util.h>

#include <xfce4-session/xfsm-chooser.h>
#include <xfce4-session/xfsm-splash-screen.h>


struct _XfsmSplashScreen
{
  XfsmSplashEngine engine;
  GModule         *module;
};


static void xfsm_splash_screen_load (XfsmSplashScreen *splash,
                                     const gchar      *engine);


XfsmSplashScreen*
xfsm_splash_screen_new (GdkDisplay  *display,
                        const gchar *engine)
{
  XfsmSplashScreen *splash;
  XfsmSplashRc     *splash_rc;
  GdkScreen        *screen;
  gchar             name[128];
  int               monitor;
  XfconfChannel    *channel;

  /* locate monitor with pointer */
  screen = xfce_gdk_screen_get_active (&monitor);

  if (G_UNLIKELY (screen == NULL) || (gdk_screen_get_display (screen) != display))
    {
      screen  = gdk_display_get_default_screen (display);
      monitor = 0;
    }

  /* initialize the screen struct */
  splash = g_new0 (XfsmSplashScreen, 1);
  splash->engine.display = display;
  splash->engine.primary_screen = screen;
  splash->engine.primary_monitor = monitor;

  /* load and setup the engine */
  if (G_LIKELY (engine != NULL && *engine != '\0'))
    {
      xfsm_splash_screen_load (splash, engine);
      if (G_LIKELY (splash->engine.setup != NULL))
        {
          g_snprintf (name, sizeof(name), "/splash/engines/%s", engine);
          channel = xfconf_channel_new_with_property_base ("xfce4-session",
                                                           name);
          splash_rc = xfsm_splash_rc_new (channel);
          g_object_unref (channel);
          splash->engine.setup (&splash->engine, splash_rc);
          xfsm_splash_rc_free (splash_rc);

          gdk_flush ();
        }
    }

  return splash;
}


void
xfsm_splash_screen_start (XfsmSplashScreen *splash,
                          const gchar      *name,
                          GdkPixbuf        *preview,
                          unsigned          steps)
{
  if (G_LIKELY (splash->engine.start != NULL))
    {
      splash->engine.start (&splash->engine, name, preview, steps);
      gdk_flush ();
    }
}


void
xfsm_splash_screen_next (XfsmSplashScreen *splash,
                         const gchar      *text)
{
  if (G_LIKELY (splash->engine.next != NULL))
    {
      splash->engine.next (&splash->engine, text);
      gdk_flush ();
    }
}


int
xfsm_splash_screen_run (XfsmSplashScreen *splash,
                        GtkWidget        *dialog)
{
  int result;

  if (G_LIKELY (splash->engine.run != NULL))
    {
      result = splash->engine.run (&splash->engine, dialog);
    }
  else
    {
      gtk_window_set_screen (GTK_WINDOW (dialog), splash->engine.primary_screen);
      gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

      result = gtk_dialog_run (GTK_DIALOG (dialog));
    }

  return result;
}


int
xfsm_splash_screen_choose (XfsmSplashScreen *splash,
                           GList            *sessions,
                           const gchar      *default_session,
                           gchar           **name_return)
{
  GtkWidget *chooser;
  GtkWidget *label;
  GtkWidget *dialog;
  GtkWidget *entry;
  gchar      title[256];
  int        result;

  g_assert (default_session != NULL);

  if (splash->engine.choose != NULL)
    {
      result = splash->engine.choose (&splash->engine,
                                      sessions,
                                      default_session,
                                      name_return);
    }
  else
    {
again:
      xfsm_splash_screen_next (splash, _("Choose session"));

      chooser = g_object_new (XFSM_TYPE_CHOOSER,
                              "screen", splash->engine.primary_screen,
                              "type", GTK_WINDOW_POPUP,
                              NULL);
      xfsm_window_add_border (GTK_WINDOW (chooser));
      xfsm_chooser_set_sessions (XFSM_CHOOSER (chooser),
                                 sessions, default_session);
      result = xfsm_splash_screen_run (splash, chooser);

      if (result == XFSM_RESPONSE_LOAD)
        {
          if (name_return != NULL)
            *name_return = xfsm_chooser_get_session (XFSM_CHOOSER (chooser));
          result = XFSM_CHOOSE_LOAD;
        }
      else if (result == XFSM_RESPONSE_NEW)
        {
          result = XFSM_CHOOSE_NEW;
        }
      else
        {
          result = XFSM_CHOOSE_LOGOUT;
        }

      gtk_widget_destroy (chooser);

      if (result == XFSM_CHOOSE_NEW)
        {
          xfsm_splash_screen_next (splash, _("Choose session name"));

          dialog = gtk_dialog_new_with_buttons (NULL,
                                                NULL,
                                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                                _("_Cancel"),
                                                GTK_RESPONSE_CANCEL,
                                                _("_OK"),
                                                GTK_RESPONSE_OK,
                                                NULL);
          gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_OK);

          g_snprintf (title, 256, "<big>%s</big>",
                      _("Choose a name for the new session:"));
          label = gtk_label_new (title);
          gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
          gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                              label, TRUE, TRUE, 6);
          gtk_widget_show (label);

          entry = gtk_entry_new ();
          gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                              entry, TRUE, TRUE, 6);
          gtk_widget_show (entry);

          xfsm_window_add_border (GTK_WINDOW (dialog));

again1:
          result = xfsm_splash_screen_run (splash, dialog);

          if (result != GTK_RESPONSE_OK)
            {
              gtk_widget_destroy (dialog);
              goto again;
            }

          if (name_return != NULL)
            {
              *name_return = gtk_editable_get_chars (GTK_EDITABLE (entry),
                                                     0, -1);
              if (strlen (*name_return) == 0)
                {
                  g_free (*name_return);
                  goto again1;
                }
            }

          gtk_widget_destroy (dialog);
          result = XFSM_CHOOSE_NEW;
        }
    }

  return result;
}


void
xfsm_splash_screen_free (XfsmSplashScreen *splash)
{
  if (G_LIKELY (splash->engine.destroy != NULL))
    splash->engine.destroy (&splash->engine);
  if (G_LIKELY (splash->module != NULL))
    g_module_close (splash->module);
  g_free (splash);
}


static void
xfsm_splash_screen_load (XfsmSplashScreen *splash,
                         const gchar      *engine)
{
  void (*init) (XfsmSplashEngine *engine);
  gchar *filename;

  filename = g_module_build_path (LIBDIR "/xfce4/session/splash-engines", engine);
  splash->module = g_module_open (filename, G_MODULE_BIND_LOCAL);
  g_free (filename);

  if (G_LIKELY (splash->module != NULL))
    {
      if (g_module_symbol (splash->module, "engine_init", (gpointer)&init))
        {
          init (&splash->engine);
        }
      else
        {
          g_module_close (splash->module);
          splash->module = NULL;
        }
    }
  else
    {
      g_warning ("Unable to load engine \"%s\": %s", engine, g_module_error ());
    }
}


