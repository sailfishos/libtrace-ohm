man_MANS   = libtrace.3
docdir     = ${datadir}/doc/libtrace-doc
doc_DATA   = libtrace.html
DIST       = libtrace.html

# HTML version of the manual pages
libtrace.html: libtrace.3
	groff -mman -Thtml $< > $@ || \
	echo "Failed to convert manual page to HTML. See man libtrace.3." > $@

