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

if HAVE_CYGWIN
libbalou_la_LDFLAGS +=							\
	-no-undefined							\
	-export-symbols $(top_srcdir)/xfce4-session/xfsm-splash-rc.def	\
	@XFCE_MCS_MANAGER_LDFLAGS@
libbalou_la_LIBADD =							\
	@XFCE_MCS_MANAGER_LIBS@
endif
