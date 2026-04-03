# Shared library target
lib_LTLIBRARIES = libchronyctl.la

# Only this source is compiled into the library
libchronyctl_la_SOURCES = libchronyctl.c
libchronyctl_la_LDFLAGS = -version-info 1:0:0 -shared -lpthread -lm
libchronyctl_la_CFLAGS = -fPIC

# Install header(s)
includedir = $(prefix)/include/chronyctl
include_HEADERS = libchronyctl.h

# Build test app that links dynamically with our new library
# NOTE: This assumes test_chronyctl.c exists in this directory and uses the chronyctl API
bin_PROGRAMS = test_chronyctl
test_chronyctl_SOURCES = test_chronyctl.c
test_chronyctl_CFLAGS = -I$(srcdir)
test_chronyctl_LDADD = libchronyctl.la -lpthread -lm

# Optional: Clean up built .so/.o files by 'make clean'
CLEANFILES = libchronyctl.lo libchronyctl.o libchronyctl.la test_chronyctl

# Optionally add some install paths or rules
# (Automake by default installs to $(libdir) and $(includedir)/chronyctl)
