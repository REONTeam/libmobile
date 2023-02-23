ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -DNDEBUG -DMOBILE_LIBCONF_USE
AM_CFLAGS = $(EXTRA_CFLAGS)

lib_LTLIBRARIES = libmobile.la
libmobile_la_LDFLAGS = -no-undefined -version-info 0:0:0

libmobile_la_SOURCES = \
	atomic.h \
	callback.c \
	callback.h \
	commands.c \
	commands.h \
	compat.h \
	config.c \
	config.h \
	debug.c \
	debug.h \
	dns.c \
	dns.h \
	inet_pton.c \
	mobile.c \
	mobile_data.h \
	relay.c \
	relay.h \
	serial.c \
	serial.h \
	util.c \
	util.h

include_HEADERS = \
	mobile.h \
	mobile_inet.h

pkgconfig_DATA = \
	libmobile.pc

EXTRA_DIST = \
	meson.build \
	meson_options.txt \
	mobile_config.meson.h.in
