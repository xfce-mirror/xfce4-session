SUBDIRS = 								\
	images

noinst_DATA =								\
	xfce4-session.xml

docdir = $(datadir)/xfce4/doc/C
doc_DATA =								\
	xfce4-session.html

EXTRA_DIST = 								\
	$(noinst_DATA)							\
	$(doc_DATA)

xfce4-session.html: $(srcdir)/xfce4-session.xml
	xsltproc $(top_srcdir)/docs/xfce-nochunk.xsl			\
		$(srcdir)/xfce4-session.xml
