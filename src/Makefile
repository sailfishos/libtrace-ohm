CC        := gcc
WARNFLAGS := -Wall

ifeq ($(strip $(debug)),yes)
DBGFLAGS := -g3
OPTFLAGS := -O0
else
OPTFLAGS := -O2
endif

INCDIRS  := ..

CFLAGS   := $(WARNFLAGS) $(DBGFLAGS) $(OPTFLAGS) $(foreach d,$(INCDIRS),-I$(d))

SOURCES    := trace.c simple-filter.c regexp-filter.c parser.c
A_OBJECTS  := $(patsubst %.c,%.a.o,$(SOURCES))
SO_OBJECTS := $(patsubst %.c,%.so.o,$(SOURCES))

TARGETS    := libtrace.so libtrace.a nsntrace.pc

PREFIX    ?= /opt/nsn
LIBDIR    := $(shell echo $(PREFIX)/lib | tr -s '/' '/')
INCDIR    := $(shell echo $(PREFIX)/include | tr -s '/' '/')
PKGCFGDIR := $(shell echo $(PREFIX)/lib/pkgconfig | tr -s '/' '/')

all: $(TARGETS)

clean:
	$(RM) *.o *~ core*

distclean: clean
	$(RM) $(TARGETS)

install: all
	mkdir -p $(DESTDIR)/$(INCDIR)/trace
	install --mode 644 trace.h list.h $(DESTDIR)/$(INCDIR)/trace
	mkdir -p  $(DESTDIR)/$(LIBDIR)
	install --mode 644 libtrace.a $(DESTDIR)/$(LIBDIR)
	install --mode 755 libtrace.so $(DESTDIR)/$(LIBDIR)
	mkdir -p  $(DESTDIR)/$(PKGCFGDIR)
	install --mode 644 nsntrace.pc $(DESTDIR)/$(PKGCFGDIR)
	
# building the static library
libtrace.a: libtrace.a($(A_OBJECTS))

# building the dynamic library
libtrace.so: $(SO_OBJECTS)
	gcc --shared $^ -o $@

# compiling dynamic and static objects
%.a.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

%.so.o: %.c
	$(CC) -c $(CFLAGS) -fPIC $< -o $@

# building the pkg-config file
nsntrace.pc: nsntrace.pc.in
	cat $< | \
            sed "s:##__PREFIX__##:$(JHBUILDDESTDIR)/$(PREFIX):g" | \
            sed "s:##__LIBDIR__##:$(JHBUILDDESTDIR)/$(LIBDIR):g" | \
            sed "s:##__INCDIR__##:$(JHBUILDDESTDIR)/$(INCDIR):g" > $@
