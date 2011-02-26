micedir = $(libdir)/xfce4/session/splash-engines

mice_LTLIBRARIES = libmice.la

libmice_la_SOURCES =							\
	preview.h							\
	slide.h								\
	mice.c

libmice_la_CFLAGS =							\
	-I$(top_srcdir)							\
	$(LIBXFCE4UI_CFLAGS)						\
	$(XFCONF_CFLAGS)

libmice_la_LDFLAGS =							\
	-avoid-version							\
	-no-undefined							\
	-module

libmice_la_LIBADD =							\
	$(top_builddir)/libxfsm/libxfsm-4.6.la				\
	$(LIBXFCE4UI_LIBS)						\
	$(XFCONF_LIBS)

libmice_la_DEPENDENCIES =						\
	$(top_builddir)/libxfsm/libxfsm-4.6.la

noinst_PROGRAMS =							\
	generate

generate_SOURCES =							\
	generate.c

generate_CFLAGS =							\
	$(LIBXFCE4UI_CFLAGS)

generate_LDADD =							\
	$(LIBXFCE4UI_LIBS)

if MAINTAINER_MODE

BUILT_SOURCES =								\
	slide.h								\
	preview.h

preview.h: $(srcdir)/preview.png
	$(AM_V_GEN) gdk-pixbuf-csource --static --stream --name=preview $< > $@

slide.h: $(srcdir)/slide.png
	$(AM_V_GEN) gdk-pixbuf-csource --static --stream --name=slide $< > $@

DISTCLEANFILES =							\
	$(BUILT_SOURCES)

endif

EXTRA_DIST =								\
	preview.png							\
	slide.png
