man_MANS   = @PACKAGE@.3
docdir     = ${datadir}/doc/@PACKAGE@-doc
doc_DATA   = @PACKAGE@.html
DIST       = @PACKAGE@.html

# HTML version of the manual pages
@PACKAGE@.html: @PACKAGE@.3
	groff -mman -Thtml $< > $@ || \
	echo "Failed to convert manual page to HTML. See man libtrace.3." > $@

