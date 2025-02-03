xfsm_shutdown_helperdir = $(HELPER_PATH_PREFIX)/xfce4/session
xfsm_shutdown_helper_PROGRAMS = 					\
	xfsm-shutdown-helper

xfsm_shutdown_helper_SOURCES =						\
	main.c

xfsm_shutdown_helper_CFLAGS =						\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/libxfsm                 \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(GTK_CFLAGS) \
	$(NULL)

xfsm_shutdown_helper_LDFLAGS =						\
	$(LIBXFCE4UTIL_LIBS) \
	$(GTK_LIBS) \
	$(NULL)
