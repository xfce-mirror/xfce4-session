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

if HAVE_CYGWIN
libsimple_la_LDFLAGS +=							\
	-no-undefined							\
	-export-symbols $(top_srcdir)/xfce4-session/xfsm-splash-rc.def	\
	@XFCE_MCS_MANAGER_LDFLAGS@
libsimple_la_LIBADD =							\
	@XFCE_MCS_MANAGER_LIBS@
endif

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

