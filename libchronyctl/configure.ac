AC_INIT([libchronyctl], [1.0], [])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_PROG_CC
AM_PROG_AR
LT_INIT([shared disable-static])

# Check for required math library (pow() used in float_to_double)
AC_CHECK_LIB([m], [pow], [], [AC_MSG_ERROR([math library (libm) not found])])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
