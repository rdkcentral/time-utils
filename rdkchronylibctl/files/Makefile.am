ACLOCAL_AMFLAGS = -I m4

# -----------------------------------------------
# Shared library: libchronyctl.so
# -----------------------------------------------
lib_LTLIBRARIES = libchronyctl.la

libchronyctl_la_SOURCES  = libchronyctl.c
libchronyctl_la_CFLAGS   = -Wall -Wextra -fPIC
libchronyctl_la_LDFLAGS  = -version-info 1:0:0 -shared
libchronyctl_la_LIBADD   = -lm

# Public headers installed to $(includedir)
include_HEADERS = \
	libchronyctl.h \
	addressing.h \
	candm.h

# -----------------------------------------------
# Test binary (not installed)
# -----------------------------------------------
noinst_PROGRAMS = test_chronyctl

test_chronyctl_SOURCES = test_chronyctl.c
test_chronyctl_LDADD   = libchronyctl.la
