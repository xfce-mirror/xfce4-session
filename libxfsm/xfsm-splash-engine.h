/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
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

#ifndef __XFSM_SPLASH_ENGINE_H__
#define __XFSM_SPLASH_ENGINE_H__

#include <time.h>

#include <gtk/gtk.h>

#include <libxfsm/xfsm-splash-rc.h>


#define XFSM_CHOOSE_LOGOUT  0
#define XFSM_CHOOSE_LOAD    1
#define XFSM_CHOOSE_NEW     2


typedef struct _XfsmSplashEngine XfsmSplashEngine;
typedef struct _XfsmSplashConfig XfsmSplashConfig;
typedef struct _XfsmSessionInfo  XfsmSessionInfo;


struct _XfsmSplashEngine
{
  /* provided by the session manager */
  GdkDisplay  *display;
  GdkScreen   *primary_screen;
  int          primary_monitor;


  /* to be filled in by the engine */
  gpointer     user_data;

  /* load config from rc and display the splash window */
  void (*setup)   (XfsmSplashEngine *engine,
                   XfsmSplashRc     *rc);

  /* tells the engine that the session will start now, where
     steps is the approx. number of apps started in the session,
     name is the name of the session and the preview at 52x42
     (OPTIONAL) */
  void (*start)   (XfsmSplashEngine *engine,
                   const gchar      *name,
                   GdkPixbuf        *preview,
                   unsigned          steps);

  /* place, run, hide the dialog and return the result of the run.
     override on demand (OPTIONAL) */
  int  (*run)     (XfsmSplashEngine *engine,
                   GtkWidget        *dialog);

  /* display the text (OPTIONAL) */
  void (*next)    (XfsmSplashEngine *engine,
                   const gchar      *text);

  /* choose a session (OPTIONAL), should return XFSM_CHOOSE_* */
  int  (*choose)  (XfsmSplashEngine *engine,
                   GList            *sessions,
                   const gchar      *default_session,
                   gchar           **name_return);

  void (*destroy) (XfsmSplashEngine *engine);

  gpointer _reserved[8];
};


struct _XfsmSplashConfig
{
  /* provided by the session manager */
  XfsmSplashRc  *rc;


  /* to be filled in by the config (freed by the session manager) */
  gchar   *name;
  gchar   *description;
  gchar   *version;
  gchar   *author;
  gchar   *homepage;

  /* config internals (config is responsible for freeing during destroy) */
  gpointer user_data;

  /* generate preview for engine, should be 320x240! (OPTIONAL) */
  GdkPixbuf *(*preview) (XfsmSplashConfig *config);

  /* display a configuration dialog (OPTIONAL) */
  void (*configure) (XfsmSplashConfig *config,
                     GtkWidget        *parent);

  void (*destroy) (XfsmSplashConfig *config);

  gpointer _reserved[8];
};


struct _XfsmSessionInfo
{
  gchar     *name;      /* name of the session */
  time_t     atime;     /* last access time */
  GdkPixbuf *preview;   /* preview icon (52x42) */
};


#endif /* !__XFSM_SPLASH_ENGINE_H__ */
