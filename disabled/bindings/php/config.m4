dnl
dnl $Id: $
dnl

PHP_ARG_WITH(nsntrace, for NSN SOA trace support,
[  --with-nsntrace[=DIR]        Include NSN SOA trace support])

if test "$PHP_NSNTRACE" != "no"; then
  if test -r $PHP_NSNTRACE/include/trace/trace.h; then
    NSNTRACE_DIR=$PHP_NSNTRACE
  else
    AC_MSG_CHECKING(for NSN SOA trace in default path)
    for i in /opt/nsn /usr/local /usr; do
      if test -r $i/include/trace/trace.h; then
        NSNTRACE_DIR=$i
        AC_MSG_RESULT(found in $i)
        break
      fi
    done
  fi

  if test -z "$NSNTRACE_DIR"; then
    AC_MSG_RESULT(not found)
    AC_MSG_ERROR(Please (re)install the NSN SOA trace distribution)
  fi

  PHP_CHECK_LIBRARY(trace, trace_open, 
  [
    PHP_ADD_INCLUDE($NSNTRACE_DIR/include)
    PHP_ADD_LIBRARY_WITH_PATH(trace, $NSNTRACE_DIR/$PHP_LIBDIR, NSNTRACE_SHARED_LIBADD)
    AC_DEFINE(HAVE_NSNTRACE,1,[ ])
  ], [
    AC_MSG_ERROR(nsntrace module requires NSN SOA libtrace >= 0.0.0)
  ], [
    -L$NSNTRACE_DIR/$PHP_LIBDIR
  ])

  PHP_NEW_EXTENSION(nsntrace, nsntrace.c, $ext_shared)
  PHP_SUBST(NSNTRACE_SHARED_LIBADD)
fi
