SUBDIRS =								\
	C								\
	fr

noinst_DATA =								\
	FAQ								\
	README.Kiosk

EXTRA_DIST = $(noinst_DATA)

html:
	make -C C html
	make -C fr html
