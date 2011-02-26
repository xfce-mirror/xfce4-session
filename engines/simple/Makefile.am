simpledir = $(libdir)/xfce4/session/splash-engines

simple_LTLIBRARIES = libsimple.la

libsimple_la_SOURCES =							\
	fallback.h							\
	preview.h							\
	simple.c

libsimple_la_CFLAGS =							\
	-I$(top_srcdir)							\
	$(LIBX11_CFLAGS)						\
	$(LIBXFCE4UI_CFLAGS)						\
	$(XFCONF_CFLAGS)

libsimple_la_LDFLAGS =							\
	-avoid-version							\
	-no-undefined							\
	-module								\
	$(LIBX11_LDFLAGS)

libsimple_la_LIBADD =							\
	$(top_builddir)/libxfsm/libxfsm-4.6.la				\
	$(LIBX11_LIBS)							\
	$(LIBXFCE4UI_LIBS)						\
	$(XFCONF_LIBS)

libsimple_la_DEPENDENCIES =						\
	$(top_builddir)/libxfsm/libxfsm-4.6.la

if MAINTAINER_MODE

BUILT_SOURCES =								\
	fallback.h							\
	preview.h

fallback.h: $(srcdir)/fallback.png
	$(AM_V_GEN) gdk-pixbuf-csource --static --stream --name=fallback $< > $@

preview.h: $(srcdir)/preview.png
	$(AM_V_GEN) gdk-pixbuf-csource --static --stream --name=preview $< > $@

DISTCLEANFILES =							\
	$(BUILT_SOURCES)

endif

EXTRA_DIST =								\
	fallback.png							\
	preview.png
