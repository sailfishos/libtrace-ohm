lib_LTLIBRARIES = libsimple-trace.la

libsimple_trace_la_SOURCES = simple-trace.c
libsimple_trace_la_CFLAGS  = -Wall -Wextra
libsimple_trace_la_LDFLAGS = -version-info $(LIBTRACE_VERSION_INFO)

INCLUDES = -I$(top_builddir)/include -I.

MAINTAINERCLEANFILES = Makefile.in

clean-local:
	rm -f *~

