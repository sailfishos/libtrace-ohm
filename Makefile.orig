PACKAGE := trace

PREFIX  ?= /usr
SUBDIRS := $(PACKAGE) test doc #bindings

REPOTOP  := $(shell while [ ! -d .git -a "`pwd`" != "/" ]; do cd ..; done; \
                    [ -d .git ] && pwd || { echo "unknown"; exit 1; })

SPECFILE := rpmbuild/SPECS/nsn$(PACKAGE).spec
SPECSTUB := packaging/rpm/nsn$(PACKAGE).spec.in
#DIFFS    := $(shell git show HEAD | grep '^commit ' | \
#                        head -1 | cut -d ' ' -f 2).diff

#RPM_VERSION := $(shell $(REPOTOP)/scripts/scm/rpm-version --version)
#RPM_RELEASE := $(shell $(REPOTOP)/scripts/scm/rpm-version --release)
#RPM_ARCH    := $(shell rpm -q --queryformat "%{ARCH}\n" rpm)
#RPM_BUILD    = rpmbuild --define "_topdir `pwd`/rpmbuild" \
#                        --define "_tmppath `pwd`/rpmbuild/tmp"

TARBALL     := rpmbuild/SOURCES/nsn$(PACKAGE)-$(RPM_VERSION).tar.gz
TARTMP      := nsn$(PACKAGE)-$(shell echo $$USER-`date +%Y%m%d-%H%M%S`).tar.gz

export PACKAGE PREFIX INCDIR LIBDIR PKGCFGDIR MANDIR DOCDIR VERSION debug \
       DIFFS

####################
# standard top-level Makefile targets

all install:
	$(foreach d,$(SUBDIRS),$(MAKE) -C $(d) VERSION=$(RPM_VERSION) $@ &&) :

build: all

unittest: all
	$(MAKE) -C test unittest

clean:
	$(foreach d,$(SUBDIRS),$(MAKE) -C $(d) $@ &&) :

distclean: clean
	$(foreach d,$(SUBDIRS),$(MAKE) -C $(d) $@ &&) :
	$(RM) -fr *~ rpms rpmbuild

#rpm rpms: distclean $(SPECFILE) $(TARBALL) $(DIFFILE)
#	mkdir -p rpmbuild/{tmp,BUILD,RPMS,SRPMS} rpms && \
#        $(RPM_BUILD) -bb $(SPECFILE) && \
#	$(RPM_BUILD) -bs $(SPECFILE) && \
#	mv rpmbuild/RPMS/$(RPM_ARCH)/*.rpm rpms && \
#	mv rpmbuild/SRPMS/*.src.rpm rpms && \
#	$(RM) -fr rpmbuild

#$(SPECFILE): $(SPECSTUB)
#	mkdir -p rpmbuild/SPECS && \
#	rv="$(RPM_VERSION)" && rr="$(RPM_RELEASE)" && \
#	cat $< | sed "s/##__VERSION__##/$$rv/g;s/##__RELEASE__##/$$rr/g" > $@

#$(TARBALL): doc/$(DIFFS)
#	mkdir -p rpmbuild/SOURCES && \
#	rv="$(RPM_VERSION)" && \
#	tar -C .. -cvzf $(TARTMP) $(PACKAGE) && \
#	mv $(TARTMP) $(TARBALL) && \
#	rm -f doc/$(DIFFS)


#doc/$(DIFFS):
#	git diff -c . > $@