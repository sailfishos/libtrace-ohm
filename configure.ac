#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.72])

AC_INIT([libsimple-trace],[1.1.9])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/simple-trace.c])
AC_CONFIG_HEADERS([src/config.h])
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

AC_SUBST(ACLOCAL_AMFLAGS, "-I m4")

m4_define(version_major, `echo $VERSION | cut -d. -f1 | cut -d- -f1`)
m4_define(version_minor, `echo $VERSION | cut -d. -f2 | cut -d- -f1`)
m4_define(version_patch, `echo $VERSION | cut -d. -f3 | cut -d- -f1`)

AC_SUBST(VERSION)
AC_SUBST(VERSION_MAJOR, version_major)
AC_SUBST(VERSION_MINOR, version_minor)
AC_SUBST(VERSION_PATCH, version_patch)
AC_SUBST(VERSION_FULL, version_major.version_minor.version_patch)

# libtool interface version
LIBTRACE_VERSION_INFO="0:0:0"
AC_SUBST(LIBTRACE_VERSION_INFO)

# Disable static libraries.
AC_DISABLE_STATIC

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AM_PROG_CC_C_O
LT_INIT

# Checks for header files.
m4_warn([obsolete],
[The preprocessor macro 'STDC_HEADERS' is obsolete.
  Except in unusual embedded environments, you can safely include all
  C89 headers unconditionally.])dnl
# Autoupdate added the next two lines to ensure that your configure
# script's behavior did not change.  They are probably safe to remove.
AC_CHECK_INCLUDES_DEFAULT
AC_PROG_EGREP

AC_CHECK_HEADERS([stdlib.h string.h sys/time.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
m4_warn([obsolete],
[Update your code to rely only on HAVE_SYS_TIME_H,
then remove this warning and the obsolete code below it.
All current systems provide time.h; it need not be checked for.
Not all systems provide sys/time.h, but those that do, all allow
you to include it and time.h simultaneously.])dnl
AC_CHECK_HEADERS_ONCE([sys/time.h])
# Obsolete code to be removed.
if test $ac_cv_header_sys_time_h = yes; then
  AC_DEFINE([TIME_WITH_SYS_TIME],[1],[Define to 1 if you can safely include both <sys/time.h>
	     and <time.h>.  This macro is obsolete.])
fi
# End of obsolete code.

AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRFTIME
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([gettimeofday memset regcomp strdup])

# Check for Check (unit test framework).
PKG_CHECK_MODULES(CHECK, 
                  check >= 0.9.4,
                  [has_check="yes"], [has_check="no"])
AM_CONDITIONAL(HAVE_CHECK, test "x$has_check" = "xyes")
if test "x$has_check" = "xno"; then
  AC_MSG_WARN([Check framework not found, unit tests are DISABLED.])
fi

# shave
SHAVE_INIT([build-aux], [enable])

AC_CONFIG_FILES([build-aux/shave
		 build-aux/shave-libtool
		 Makefile
		 src/Makefile
		 src/libsimple-trace.pc
		 include/Makefile
		 doc/Makefile
		 tests/Makefile])
AC_OUTPUT


echo "Libtrace version: $VERSION"
echo "     API version: ${LIBTRACE_VERSION_INFO//:/.}"
