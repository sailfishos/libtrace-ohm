THISDIR  := $(shell pwd)
SYMLINKS := include lib

# Argh, I hate irregularities...
ifeq ($(strip $(PREFIX)),/usr)
ETCDIR   := /etc
else
ETCDIR   := $(PREFIX)/etc
endif


all: $(SYMLINKS) lib/libtrace.so php/Makefile
	$(MAKE) -C php $@ CFLAGS="-O0 -g3"
	$(MAKE) -C python $@

install: all
	$(MAKE) -C php INSTALL_ROOT=$(DESTDIR) \
	               EXTENSION_DIR=$(PREFIX)/lib/php/modules $@
	mkdir -p $(DESTDIR)/$(ETCDIR)/php.d && \
	    install --mode 644 php/nsntrace.ini $(DESTDIR)/$(ETCDIR)/php.d
	$(MAKE) -C python DESTDIR=$(DESTDIR) PREFIX=$(PREFIX) install

clean:
	[ -f php/Makefile ] && $(MAKE) -C php $@ || :
	$(MAKE) -C python $@

distclean:: clean
	[ -f php/Makefile ] && $(MAKE) -C php $@ || :
	$(MAKE) -C python $@
	[ -d lib ] && $(MAKE) -C lib $@ || :
	$(RM) -f $(SYMLINKS) *~
	$(RM) -fr php/{ac*,autom4te.cache,build,config.guess,config.h*,config.nice,config.sub,configure*,.deps,include,install-sh,ltmain.sh,Makefile*,missing,mkinstalldirs,modules,run-tests.php,*~}

test unittest:


#
# preparing the bindings for compilation
#

php/Makefile: php/configure
	cd php && ./configure --with-nsntrace=shared,$(THISDIR)

php/configure:
	cd php && phpize


#
# header and library symlinks
#

# Notes:
#     We create a directory tree resembling a normal installation of the
#     trace library with PREFIX set to $(shell pwd). When configuring and
#     building the actual language bindings we give $(shell pwd) as the
#     installation path (prefix) to the trace library. This prevents
#     (mis)compiling and (mis)linking accidentially against a previously
#     installed version on the system. The rules below are for setting
#     up the necessary symlinks.

include:
	$(RM) -f include
	ln -s $(shell cd ..; pwd) include

lib:
	$(RM) -f lib
	ln -s $(shell cd ..; pwd)/trace lib

lib/libtrace.so:
	$(MAKE) -C lib all
