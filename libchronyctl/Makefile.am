# Copyright 2023 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

# -----------------------------------------------
lib_LTLIBRARIES = libchronyctl.la

libchronyctl_la_SOURCES  = libchronyctl.c
libchronyctl_la_CFLAGS   = -Wall -Wextra -fPIC
libchronyctl_la_LDFLAGS  = -version-info 1:0:0
libchronyctl_la_LIBADD   = -lm

# Public headers installed to $(includedir)
include_HEADERS = \
        libchronyctl.h \
        chrony_address.h \
        chrony_protocol.h

# -----------------------------------------------
# Test binaries 
# -----------------------------------------------
bin_PROGRAMS  = test_timectl


# Common NTP client test binary (single file, no extra adapter files)
# To add a new backend: #include its header and add a row to backends[] in test_timectl.c.
test_timectl_SOURCES = test_timectl.c
test_timectl_CPPFLAGS = -I$(top_srcdir)/.. -I$(top_srcdir)
test_timectl_LDADD   = libchronyctl.la
