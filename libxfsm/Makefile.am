INCLUDES =								\
	-I$(top_srcdir)

# common library
lib_LTLIBRARIES = libxfsm-4.2.la

libxfsm_4_2_la_SOURCES =						\
	xfsm-splash-rc.c						\
	xfsm-splash-rc.h						\
	xfsm-util.h							\
	xfsm-util.c

libxfsm_4_2_la_CFLAGS =							\
	@LIBX11_CFLAGS@							\
	@LIBXFCEGUI4_CFLAGS@

libxfsm_4_2_la_LDFLAGS =						\
	-export-dynamic							\
	-version-info 0:1:0

if HAVE_OS_CYGWIN
libxfsm_4_2_la_LDFLAGS +=						\
	-no-undefined							\
	@LIBX11_LDFLAGS@

libxfsm_4_2_la_LIBADD =							\
	@LIBX11_LIBS@							\
	@LIBXFCEGUI4_LIBS@
endif


libxfsmincludedir =							\
	$(includedir)/xfce4/xfce4-session-4.2/libxfsm

libxfsminclude_HEADERS = 						\
	xfsm-splash-engine.h						\
	xfsm-splash-rc.h


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = xfce4-session-1.0.pc


