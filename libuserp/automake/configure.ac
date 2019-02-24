AC_PREREQ([2.68])
AC_INIT([userp], [0.1], [mike@nrdvana.net])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_HEADERS([config.h])

AC_ARG_ENABLE(debug,  AS_HELP_STRING([--enable-debug], [enable assertions and debug symbols]),
 [ if test "$enableval" != "no"; then CFLAGS="$CFLAGS -O0 -g3"; else CFLAGS="$CFLAGS -O2 -DNDEBUG"; fi; ],
 [ CFLAGS="$CFLAGS -O0 -g3"; ])

AC_ARG_ENABLE(dev,    AS_HELP_STRING([--enable-dev], [enable source generators (requires perl)]),
 [ if test "$enableval" != "no"; then dev_include_makefile="include \$(srcdir)/dev_rules.mak"; fi; ],
 [ dev_include_makefile=; ])
AC_SUBST(dev_include_makefile)

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_AR

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE

# Checks for library functions.
AC_CHECK_FUNCS([snprintf])

LT_INIT
AC_CONFIG_MACRO_DIRS([m4])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
