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
	-I$(top_srcdir)							\
	$(LIBXFCE4UI_CFLAGS)						\
	$(XFCONF_CFLAGS)						\
	-DBALOU_EXPORT_THEME=\"$(libexecdir)/balou-export-theme\"	\
	-DBALOU_INSTALL_THEME=\"$(libexecdir)/balou-install-theme\"

libbalou_la_LDFLAGS =							\
	-avoid-version							\
	-no-undefined							\
	-module

libbalou_la_LIBADD =							\
	$(top_builddir)/libxfsm/libxfsm-4.6.la				\
	$(LIBXFCEGUI4_LIBS)						\
	$(XFCONF_LIBS)


libbalou_la_DEPENDENCIES =						\
	$(top_builddir)/libxfsm/libxfsm-4.6.la
