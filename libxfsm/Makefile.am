AM_CPPFLAGS =								\
	-I$(top_srcdir)

# common library
lib_LTLIBRARIES = libxfsm-4.6.la

libxfsm_4_6_la_SOURCES =						\
	xfsm-splash-rc.c						\
	xfsm-splash-rc.h						\
	xfsm-util.h							\
	xfsm-util.c

libxfsm_4_6_la_CFLAGS =							\
	$(LIBX11_CFLAGS)						\
	$(LIBXFCE4UI_CFLAGS)						\
	$(XFCONF_CFLAGS)

libxfsm_4_6_la_LDFLAGS =						\
	-export-dynamic							\
	-version-info 0:0:0						\
	$(LIBX11_LDFLAGS)

if HAVE_OS_CYGWIN
libxfsm_4_6_la_LDFLAGS +=						\
	-no-undefined
endif

libxfsm_4_6_la_LIBADD =							\
	$(LIBX11_LIBS)							\
	$(LIBXFCE4UI_LIBS)						\
	$(XFCONF_LIBS)


libxfsmincludedir =							\
	$(includedir)/xfce4/xfce4-session-4.6/libxfsm

libxfsminclude_HEADERS = 						\
	xfsm-splash-engine.h						\
	xfsm-splash-rc.h


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = xfce4-session-2.0.pc


