@SET_MAKE@

SUBDIRS =								\
	C								\
	fr

noinst_DATA =								\
	FAQ								\
	README.Kiosk

EXTRA_DIST =								\
	$(noinst_DATA)

html:
	(cd C && $(MAKE) $(AM_MAKEFLAGS) html)
	(cd fr && $(MAKE) $(AM_MAKEFLAGS) html)
