simpledir = $(libdir)/xfce4/splash/engines

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

EXTRA_DIST =								\
	fallback.h							\
	fallback.png							\
	preview.h							\
	preview.png


fallback.h: $(srcdir)/fallback.png
	gdk-pixbuf-csource --struct --name=fallback			\
		$(srcdir)/fallback.png > fallback.h

preview.h: $(srcdir)/preview.png
	gdk-pixbuf-csource --struct --name=preview			\
		$(srcdir)/preview.png > preview.h

