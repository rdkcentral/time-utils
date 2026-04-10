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
# Test binaries (not installed)
# -----------------------------------------------
noinst_PROGRAMS = test_chronyctl test_timectl

# Original chrony-specific test (kept for reference)
test_chronyctl_SOURCES = test_chronyctl.c
test_chronyctl_LDADD   = libchronyctl.la

# Common NTP client test binary (single file, no extra adapter files)
# To add a new backend: #include its header and add a row to backends[] in test_timectl.c.
test_timectl_SOURCES = test_timectl.c
test_timectl_LDADD   = libchronyctl.la
