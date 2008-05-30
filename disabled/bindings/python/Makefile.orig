MODULE     := _nsntrace
SOURCES    := nsntrace.c
SETUP      := nsntrace-setup.py
TARGETS    := $(MODULE).so

PREFIX     ?= /opt/nsn
PY_VERSION := $(shell python -V 2>&1 | sed 's/^.* //g')
PY_MAJMIN  := $(shell python -V 2>&1 | sed 's/^.* //g' | cut -d '.' -f 1,2)

PY_SITEDIR := $(PREFIX)/lib/python$(PY_MAJMIN)/site-packages
PY_ROOT    := $(shell [ -z "$(DESTDIR)" ] || echo "--root $(DESTDIR)")

all: $(TARGETS)

$(MODULE).so: $(SOURCES)
	python $(SETUP) build_ext

install: all
	python $(SETUP) install \
	    $(PY_ROOT) --install-lib $(PY_SITEDIR) --optimize 2
clean:
	python $(SETUP) clean
	$(RM) -fr $(TARGETS) *~ *.o

distclean: clean
	$(RM) -fr build

test:
