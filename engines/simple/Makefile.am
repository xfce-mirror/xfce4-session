simpledir = $(libdir)/xfce4/splash/engines

simple_LTLIBRARIES = libsimple.la

libsimple_la_SOURCES =							\
	fallback.h							\
	preview.h							\
	simple.c

libsimple_la_CFLAGS =							\
	-I$(top_srcdir)							\
	@LIBX11_CFLAGS@							\
	@LIBXFCEGUI4_CFLAGS@

libsimple_la_LDFLAGS =							\
	-avoid-version							\
	-no-undefined							\
	-module

libsimple_la_LIBADD =							\
	$(top_builddir)/xfce4-session/libxfsm-4.2.la

if HAVE_OS_CYGWIN
libsimple_la_LIBADD +=							\
	@XFCE_MCS_MANAGER_LIBS@
endif

libsimple_la_DEPENDENCIES =						\
	$(top_builddir)/xfce4-session/libxfsm-4.2.la
	
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

