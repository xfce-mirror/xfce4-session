@SET_MAKE@

SUBDIRS =								\
	C

noinst_DATA =								\
	FAQ								\
	README.Kiosk

EXTRA_DIST =								\
	$(noinst_DATA)

html:
	(cd C && $(MAKE) $(AM_MAKEFLAGS) html)
