SUBDIRS =								\
	scripts								\
	themes

baloudir = $(libdir)/xfce4/splash/engines

balou_LTLIBRARIES = libbalou.la

libbalou_la_SOURCES =							\
	balou-theme.c							\
	balou-theme.h							\
	balou.c								\
	balou.h								\
	config.c							\
	engine.c							\
	gnome-uri.c							\
	gnome-uri.h

libbalou_la_CFLAGS =							\
	@LIBXFCEGUI4_CFLAGS@						\
	-DBALOU_EXPORT_THEME=\"$(libexecdir)/balou-export-theme\"	\
	-DBALOU_INSTALL_THEME=\"$(libexecdir)/balou-install-theme\"

libbalou_la_LDFLAGS =							\
	-avoid-version							\
	-module

