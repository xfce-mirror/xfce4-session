micedir = $(libdir)/xfce4/splash/engines

mice_LTLIBRARIES = libmice.la

libmice_la_SOURCES =							\
	preview.h							\
	slide.h								\
	mice.c

libmice_la_CFLAGS =							\
	-I$(top_srcdir)							\
	@LIBXFCEGUI4_CFLAGS@

libmice_la_LDFLAGS =							\
	-avoid-version							\
	-no-undefined							\
	-module

libmice_la_LIBADD =							\
	$(top_builddir)/libxfsm/libxfsm-4.6.la

if HAVE_OS_CYGWIN
libmice_la_LIBADD +=							\
	@XFCE_MCS_MANAGER_LIBS@
endif

libmice_la_DEPENDENCIES =						\
	$(top_builddir)/libxfsm/libxfsm-4.6.la

noinst_PROGRAMS =							\
	generate

generate_SOURCES =							\
	generate.c

generate_CFLAGS =							\
	@LIBXFCEGUI4_CFLAGS@

generate_LDADD =							\
	@LIBXFCEGUI4_LIBS@


EXTRA_DIST =								\
	preview.h							\
	preview.png							\
	slide.h								\
	slide.png

preview.h: $(srcdir)/preview.png
	gdk-pixbuf-csource --struct --name=preview			\
		$(srcdir)/preview.png > preview.h

slide.h: $(srcdir)/slide.png
	gdk-pixbuf-csource --struct --name=slide			\
		$(srcdir)/slide.png > slide.h

