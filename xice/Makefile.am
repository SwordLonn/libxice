#
# Makefile.am for the Xice Glib ICE library
#
# (C) 2006, 2007 Collabora Ltd.
# (C) 2006, 2007 Nokia Corporation. All rights reserved.
#
# Licensed under MPL 1.1/LGPL 2.1. See file COPYING.

include $(top_srcdir)/common.mk

lib_LTLIBRARIES = libxice.la

libxice_la_SOURCES =
libxice_la_DEPENDENCIES = \
	$(top_builddir)/agent/libagent.la \
	libxice.sym

libxice_la_LIBADD = \
	$(top_builddir)/agent/libagent.la

libxice_la_LDFLAGS = \
	-export-symbols $(srcdir)/libxice.sym \
	$(LIBXICE_LT_LDFLAGS)


AM_CFLAGS = \
	$(ERROR_CFLAGS) \
	$(GLIB_CFLAGS) \
	-I $(top_srcdir) \
	-I $(top_srcdir)/agent \
	-I $(top_srcdir)/random \
	-I $(top_srcdir)/socket \
	-I $(top_srcdir)/stun

test-symbols.sh::
	chmod +x $(srcdir)/$@

libxice-symbols-test.c: libxice.sym
	rm -f $@
	while read s; do echo "void $$s(void) { }" ; done < $? > $@

libxice-symbols-test.o: libxice-symbols-test.c
	$(CC) $(CFLAGS) -c -o $@ $?

libxice.symbols: libxice-symbols-test.o
	rm -f $@
	$(top_srcdir)/scripts/make-symbol-list.sh $? > $@

CLEANFILES += libxice.symbols libxice-symbols-test.c libxice-symbols-test.o

check_SCRIPTS = test-symbols.sh
check_DATA = libxice.symbols

TESTS = $(check_SCRIPTS)

EXTRA_DIST = $(check_SCRIPTS) libxice.sym libxice.ver

pkginclude_HEADERS = xice.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = xice.pc

