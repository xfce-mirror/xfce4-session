SUBDIRS =								\
	scripts								\
	themes

baloudir = $(libdir)/xfce4/splash/engines

balou_LTLIBRARIES = libbalou.la

libbalou_la_SOURCES =							\
	balou-theme.c							\
	balou-theme.h							\
	balou.c								\
	balou.h								\
	config.c							\
	engine.c

libbalou_la_CFLAGS =							\
	@LIBXFCEGUI4_CFLAGS@

libbalou_la_LDFLAGS =							\
	-avoid-version							\
	-module

