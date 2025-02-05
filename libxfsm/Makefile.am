AM_CPPFLAGS =								\
	-I$(top_srcdir)

noinst_LTLIBRARIES = libxfsm.la

libxfsm_la_SOURCES =                    \
	xfsm-util.h							\
	xfsm-util.c							\
	xfsm-shutdown-common.h

libxfsm_la_CFLAGS =							\
	$(LIBXFCE4UI_CFLAGS)                    \
	$(LIBXFCE4UTIL_CFLAGS)                  \
	$(XFCONF_CFLAGS)

if HAVE_OS_CYGWIN
libxfsm_la_LDFLAGS =						\
	-no-undefined
endif

libxfsm_la_LIBADD =							\
	$(LIBXFCE4UI_LIBS)						\
	$(LIBXFCE4UTIL_LIBS)                    \
	$(XFCONF_LIBS)

EXTRA_DIST = \
	meson.build \
	$(NULL)
