/* $Id$ */
/*-
 * Copyright (c) 2003,2004 Benedikt Meurer <benny@xfce.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __XFSM_SPLASH_SCREEN_H__
#define __XFSM_SPLASH_SCREEN_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define	XFSM_SPLASH_SCREEN(obj)						\
	G_TYPE_CHECK_INSTANCE_CAST(obj, xfsm_splash_screen_get_type(),	\
			XfsmSplashScreen)
#define	XFSM_SPLASH_SCREEN_CLASS(klass)					\
	G_TYPE_CHECK_CLASS_CAST(klass, xfsm_splash_screen_get_type(),	\
			XfsmSplashScreenClass)
#define	XFSM_IS_SPLASH_SCREEN(obj)					\
	G_TYPE_CHECK_INSTANCE_TYPE(obj, xfsm_splash_screen_get_type())

typedef struct	_XfsmSplashScreen	XfsmSplashScreen;
typedef struct	_XfsmSplashScreenClass	XfsmSplashScreenClass;

struct _XfsmSplashScreenClass
{
	GtkWindowClass	parent_class;
};

struct _XfsmSplashScreen
{
	GtkWindow	window;

	/* dialog items */
	GtkWidget	*logoImage;
	GtkWidget	*progressBar;

	/* list of logo pictures */
	guint		pictureCount;
	guint		pictureCurrent;
	guint		pictureTimeout;
	guint		pictureTimeoutId;
	GdkPixbuf	**pictures;

	/* */
	guint		progressMax;
	guint		progressCount;
};

/* prototypes */
extern GType		xfsm_splash_screen_get_type(void);
extern GtkWidget	*xfsm_splash_screen_new(const gchar *, guint,
				const gchar *);
extern void		xfsm_splash_screen_set_text(XfsmSplashScreen *,
				const gchar *);
extern void		xfsm_splash_screen_launch(XfsmSplashScreen *,
				const gchar *);

G_END_DECLS

#endif	/* !__XFSM_SPLASH_SCREEN_H__ */
