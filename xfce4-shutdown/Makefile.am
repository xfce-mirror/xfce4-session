man_MANS = xfce4-shutdown.1

EXTRA_DIST = 								\
	$(man_MANS)

sbin_PROGRAMS = xfce4-shutdown

xfce4_shutdown_SOURCES =						\
	main.c

xfce4_shutdown_CFLAGS =							\
	@LIBXFCE4UTIL_CFLAGS@						\
	-DPATH_SHUTDOWNALLOW=\"$(sysconfdir)/xfce4/shutdown.allow\"

xfce4_shutdown_LDADD =							\
	@LIBXFCE4UTIL_LIBS@


