simpledir = $(libdir)/xfce4/splash/engines

simple_LTLIBRARIES = libsimple.la

libsimple_la_SOURCES =							\
	fallback.h							\
	simple.c

libsimple_la_CFLAGS =							\
	@LIBXFCEGUI4_CFLAGS@

libsimple_la_LDFLAGS =							\
	-avoid-version							\
	-module


EXTRA_DIST =								\
	fallback.h							\
	fallback.png

fallback.h: $(srcdir)/fallback.png
	gdk-pixbuf-csource --struct --name=fallback			\
		$(srcdir)/fallback.png > fallback.h
