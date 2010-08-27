@SET_MAKE@

SUBDIRS =								\
	manual

noinst_DATA =								\
	FAQ								\
	NEWS.pre-4.3							\
	README.Kiosk

EXTRA_DIST =								\
	$(noinst_DATA)

html:
	(cd C && $(MAKE) $(AM_MAKEFLAGS) html)
	(cd fr && $(MAKE) $(AM_MAKEFLAGS) html)
