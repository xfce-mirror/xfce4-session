simpledir = $(libdir)/xfce4/splash/engines

simple_LTLIBRARIES = libsimple.la

libsimple_la_SOURCES =							\
	fallback.h							\
	preview.h							\
	simple.c

libsimple_la_CFLAGS =							\
	@LIBX11_CFLAGS@							\
	@LIBXFCEGUI4_CFLAGS@

libsimple_la_LDFLAGS =							\
	-avoid-version							\
	-module


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

