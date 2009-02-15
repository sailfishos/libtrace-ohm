lib_LTLIBRARIES = libtrace.la
#bin_PROGRAMS    = trace-test

libtrace_la_SOURCES = simple-trace.c
libtrace_la_CFLAGS  = -Wall -Wextra

INCLUDES = -I$(top_builddir)/include -I.

MAINTAINERCLEANFILES = Makefile.in

trace_test_SOURCES = trace-test.c
trace_test_CFLAGS  =
trace_test_LDADD   = libtrace.la

clean-local:
	rm -f *~

