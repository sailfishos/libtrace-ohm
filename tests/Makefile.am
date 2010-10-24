if HAVE_CHECK
TESTS = check-libtrace
noinst_PROGRAMS = check-libtrace
else
TESTS =
endif

check_libtrace_SOURCES = check-libtrace.c \
			 check-libtrace-context.c \
			 check-libtrace-module.c \
			 check-libtrace-flags.c \
			 check-libtrace-target.c \
			 check-libtrace-format.c \
			 check-libtrace-default.c
check_libtrace_CFLAGS  = -I$(top_builddir)/include \
			  @CHECK_CFLAGS@
check_libtrace_LDADD   = $(top_builddir)/src/libsimple-trace.la \
			  @CHECK_LIBS@
