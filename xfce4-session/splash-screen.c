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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* !HAVE_CONFIG_H */

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4util/i18n.h>
#include <libxfce4util/util.h>
#include <libxfcegui4/libxfcegui4.h>

#include <xfce4-session/splash-fallback.h>
#include <xfce4-session/splash-screen.h>
#include <xfce4-session/util.h>

/* max number of pictures for a splash theme */
#define	MAX_PICTURES	25

/* default theme */
#define	SPLASH_DEFAULT_THEME	"Default"

/* static prototypes */
static void	xfsm_splash_screen_class_init(XfsmSplashScreenClass *);
static void	xfsm_splash_screen_init(XfsmSplashScreen *);
static void	xfsm_splash_screen_finalize(GObject *);
static gboolean	xfsm_splash_screen_timeout(XfsmSplashScreen *);
static gboolean	xfsm_splash_screen_load_theme(XfsmSplashScreen *,const gchar *);
static gboolean xfsm_splash_screen_load_theme_from_dir(XfsmSplashScreen *,
			const gchar *);

/* */
static GObjectClass	*parent_class = NULL;

/*
 */
GType
xfsm_splash_screen_get_type(void)
{
	static GType splash_screen_type = 0;

	if (!splash_screen_type) {
		static const GTypeInfo splash_screen_info = {
			sizeof(XfsmSplashScreenClass),
			NULL,
			NULL,
			(GClassInitFunc)xfsm_splash_screen_class_init,
			NULL,
			NULL,
			sizeof(XfsmSplashScreen),
			0,
			(GInstanceInitFunc)xfsm_splash_screen_init
		};

		splash_screen_type = g_type_register_static(GTK_TYPE_WINDOW,
				"XfsmSplashScreen", &splash_screen_info, 0);
	}

	return(splash_screen_type);
}

/*
 */
static void
xfsm_splash_screen_class_init(XfsmSplashScreenClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS(klass);
	gobject_class->finalize = xfsm_splash_screen_finalize;
	parent_class = gtk_type_class(gtk_window_get_type());
}

/*
 */
static void
xfsm_splash_screen_init(XfsmSplashScreen *splash)
{
	GtkWidget *vbox;

	/* */
	splash->pictureCount = 0;
	splash->pictureCurrent = 0;
	splash->pictures = NULL;

	/* */
	splash->progressCount = 0;
	splash->progressMax = 0;

	/* set initial window options */
	gtk_window_set_position(GTK_WINDOW(splash), GTK_WIN_POS_CENTER_ALWAYS);
	gtk_window_set_decorated(GTK_WINDOW(splash), FALSE);
	gtk_window_stick(GTK_WINDOW(splash));

	/* set window manager type hint */
	netk_gtk_window_set_type(GTK_WINDOW(splash), NETK_WINDOW_SPLASHSCREEN);

	/* always use a "Watch cursor" on the splash window */
	gtk_widget_realize(GTK_WIDGET(splash));
	gdk_window_set_cursor(GTK_WIDGET(splash)->window,
			gdk_cursor_new(GDK_WATCH));

	/* */
#if GTK_CHECK_VERSION(2, 2, 0)
	gdk_window_set_skip_pager_hint(GTK_WIDGET(splash)->window, TRUE);
	gdk_window_set_skip_taskbar_hint(GTK_WIDGET(splash)->window, TRUE);
#endif

	/* */
	vbox = gtk_vbox_new(FALSE, 1);
	gtk_container_add(GTK_CONTAINER(splash), vbox);
	gtk_widget_show(vbox);

	/* the logo image */
	splash->logoImage = gtk_image_new();
	gtk_box_pack_start(GTK_BOX(vbox), splash->logoImage, TRUE, TRUE, 0);
	gtk_widget_show(splash->logoImage);

	/* the progress bar */
	splash->progressBar = gtk_progress_bar_new();
	gtk_box_pack_start(GTK_BOX(vbox), splash->progressBar, FALSE, TRUE, 0);
	gtk_widget_show(splash->progressBar);
}

/*
 */
static void
xfsm_splash_screen_finalize(GObject *object)
{
	XfsmSplashScreen *splash;
	guint n;

	g_return_if_fail(XFSM_IS_SPLASH_SCREEN(object));

	splash = XFSM_SPLASH_SCREEN(object);

	if (splash->pictures != NULL) {
		for (n = 0; n < splash->pictureCount; n++)
			g_object_unref(splash->pictures[n]);

		g_free(splash->pictures);
	}

	if (splash->pictureTimeoutId)
		g_source_remove(splash->pictureTimeoutId);

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

/*
 */
GtkWidget *
xfsm_splash_screen_new(const gchar *splashTheme, guint progressMax,
		const gchar *initialText)
{
	XfsmSplashScreen *splash;

	/* XXX */
	splash = XFSM_SPLASH_SCREEN(g_object_new(xfsm_splash_screen_get_type(),
				NULL));

	/* prevent divide by null */
	if ((splash->progressMax = progressMax) == 0)
		splash->progressMax = 1;

	/* NULL means use Default theme */
	if (splashTheme == NULL)
		splashTheme = SPLASH_DEFAULT_THEME;

	/* */
	if (!xfsm_splash_screen_load_theme(splash, splashTheme)) {
		/* fallback to builtin logo */
		splash->pictures = g_new(GdkPixbuf *, 1);
		splash->pictures[0] = inline_icon_at_size(fallback_logo,
				350, 350);
		splash->pictureCurrent = 0;
		splash->pictureCount = 1;
		splash->pictureTimeout = 0;
	}

	/* init splash screen */
	gtk_image_set_from_pixbuf(GTK_IMAGE(splash->logoImage),
			splash->pictures[0]);
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(splash->progressBar),
			0.0);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(splash->progressBar),
			initialText);

	/* */
	if (splash->pictureTimeout > 0) {
		splash->pictureTimeoutId = g_timeout_add(
				splash->pictureTimeout,
				(GSourceFunc)xfsm_splash_screen_timeout,
				splash);
	}

	return(GTK_WIDGET(splash));
}

/*
 */
void
xfsm_splash_screen_set_text(XfsmSplashScreen *splash, const gchar *text)
{
	g_return_if_fail(text != NULL);
	g_return_if_fail(XFSM_IS_SPLASH_SCREEN(splash));

	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(splash->progressBar),
			text);
}

/*
 */
void
xfsm_splash_screen_launch(XfsmSplashScreen *splash, const gchar *program)
{
	gchar *buffer;

	g_return_if_fail(program != NULL);
	g_return_if_fail(XFSM_IS_SPLASH_SCREEN(splash));

	/* */
	if (splash->pictureTimeout == 0 && splash->pictureCount > 0) {
		splash->pictureCurrent = (splash->pictureCurrent + 1) %
			splash->pictureCount;
		gtk_image_set_from_pixbuf(GTK_IMAGE(splash->logoImage),
				splash->pictures[splash->pictureCurrent]);
	}

	/* */
	if (splash->progressCount++ > splash->progressMax)
		splash->progressCount = splash->progressMax;
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(splash->progressBar),
		(double)splash->progressCount / (double)splash->progressMax);

	/* */
	buffer = g_strdup_printf(_("Starting %s..."), program);
	xfsm_splash_screen_set_text(splash, buffer);
	g_free(buffer);

	/* give splash screen time to update its visuals */
	g_main_context_iteration(NULL, FALSE);
}

/*
 */
static gboolean
xfsm_splash_screen_timeout(XfsmSplashScreen *splash)
{
	g_return_val_if_fail(XFSM_IS_SPLASH_SCREEN(splash), FALSE);

	/* */
	splash->pictureCurrent = (splash->pictureCurrent + 1) %
		splash->pictureCount;
	gtk_image_set_from_pixbuf(GTK_IMAGE(splash->logoImage),
			splash->pictures[splash->pictureCurrent]);

	/* give splash screen time to update its visuals */
	g_main_context_iteration(NULL, FALSE);

	/* keep the timeout running */
	return(TRUE);
}

/*
 */
static gboolean
xfsm_splash_screen_load_theme(XfsmSplashScreen *splash, const gchar *theme)
{
	gchar *dir;

	/* */
	dir = xfce_get_userfile("splash", theme, NULL);
	if (xfsm_splash_screen_load_theme_from_dir(splash, dir)) {
		g_free(dir);
		return(TRUE);
	}
	g_free(dir);

	/* */
	dir = g_build_filename(SPLASH_THEMES_DIR, theme, NULL);
	if (xfsm_splash_screen_load_theme_from_dir(splash, dir)) {
		g_free(dir);
		return(TRUE);
	}
	g_free(dir);

	return(FALSE);
}

/*
 */
static gboolean 
xfsm_splash_screen_load_theme_from_dir(XfsmSplashScreen *splash,
                                       const gchar *dir)
{
	gboolean succeed;
	gchar buffer[LINE_MAX];
	gchar *p;
	gchar *file;
	gchar *name;
	gchar **imagelist;
	FILE *fp;
	guint n;

	succeed = FALSE;
	splash->pictures = NULL;
	splash->pictureCount = 0;
	splash->pictureTimeout = 0;
	name = NULL;
	imagelist = NULL;

	file = g_build_filename(dir, "splash.theme", NULL);
	
	if ((fp = fopen(file, "r")) == NULL)
		goto end;

	/* read first line */
	if (fgets(buffer, LINE_MAX, fp) == NULL)
		goto end;

	/* */
	if (strncmp(buffer, "[Splash Theme]", 14) != 0) {
		g_warning("No a splash theme file: %s", file);
		goto end;
	}

	/* */
	while (fgets(buffer, LINE_MAX, fp) != NULL) {
		/* strip leading and trailing whitespace */
		p = g_strstrip(buffer);

		/* ignore comments and empty lines */
		if (*p == '#' || *p == '\0' || *p == '\n')
			continue;

		/* */
		if (strncmp(p, "name=", 5) == 0) {
			if (name != NULL)
				g_free(name);
			name = g_strdup(p + 5);
		}
		else if (strncmp(p, "timeout=", 8) == 0) {
			splash->pictureTimeout = strtoul(p + 8, NULL, 10);
		}
		else if (strncmp(p, "imagelist=", 10) == 0) {
			if (imagelist != NULL)
				g_strfreev(imagelist);
			imagelist = g_strsplit(p + 10, ",", MAX_PICTURES);
		}

		/* slightly ignore everything else */
	}

	/* close theme description file */
	(void)fclose(fp); fp = NULL;

	/* check for required settings */
	if (name == NULL) {
		g_warning("Splash theme file %s contains no name field", file);
		goto end;
	}
	else if (imagelist == NULL) {
		g_warning("Splash theme file %s contains no imagelist field",
				file);
		goto end;
	}

	/* Ok, everything around, lets try to load the pixbufs */
	splash->pictures = g_new0(GdkPixbuf *, MAX_PICTURES);

	for (n = 0; imagelist[n] != NULL; n++) {
		GdkPixbuf *pb;
		gchar *path;

		path = g_build_filename(dir, imagelist[n], NULL);
		if ((pb = gdk_pixbuf_new_from_file(path, NULL)) != NULL)
			splash->pictures[splash->pictureCount++] = pb;
		g_free(path);
	}

	/* Does any file got loaded? */
	if (splash->pictureCount > 0)
		succeed = TRUE;
	else
		g_free(splash->pictures);
		

end:
	if (name != NULL)
		g_free(name);
	if (imagelist != NULL)
		g_strfreev(imagelist);
	if (fp != NULL)
		(void)fclose(fp);
	g_free(file);
	return(succeed);
}

