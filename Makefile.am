ACLOCAL_AMFLAGS = -I m4

# https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
lt_current  = 0
lt_revision = 0
lt_age      = 0

AM_CPPFLAGS = -DNDEBUG -DMOBILE_LIBCONF_USE
AM_CFLAGS = $(EXTRA_CFLAGS)

lib_LTLIBRARIES = libmobile.la
libmobile_la_LDFLAGS = -no-undefined \
	-version-info $(lt_current):$(lt_revision):$(lt_age)

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
	global.h \
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
