PREFIX  ?= /opt/nsn
PACKAGE ?= $(notdir $(shell cd ..; pwd))

MANDIR := $(shell echo $(PREFIX)/share/man/man3 | tr -s '/' '/')
DOCDIR := $(shell echo $(PREFIX)/share/doc/nsn$(PACKAGE)-$(VERSION) | \
                      tr -s '/' '/')

TARGETS = ChangeLog

all: ChangeLog

clean:

distclean: clean ChangeLog
	$(RM) $(filter-out ChangeLog,$(TARGETS)) *~

install:
	mkdir -p $(DESTDIR)/$(MANDIR)
	mkdir -p $(DESTDIR)/$(DOCDIR)
	install --mode 644 lib$(PACKAGE).man $(DESTDIR)/$(MANDIR)/lib$(PACKAGE).3
	install --mode 644 README ChangeLog $(DESTDIR)/$(DOCDIR)
	[ -s $(DIFFS) ] && \
	    install --mode 644 $(DIFFS) $(DESTDIR)/$(DOCDIR) || :

# HTML version of the manual pages
lib$(PACKAGE).html: lib$(PACKAGE).man
	groff -mman -Thtml $< > $@

# generate a changelog using git
ChangeLog:
	@cd ../..; git log $(PACKAGE) > $(PACKAGE)/doc/ChangeLog

# show the manual
view-manual: lib$(PACKAGE).man
	groff -mman -Tascii $< | less

