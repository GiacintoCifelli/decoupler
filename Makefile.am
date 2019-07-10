
AM_MAKEFLAGS = --no-print-directory

noinst_LTLIBRARIES =

sbin_PROGRAMS = ka

sources =
sources += \
	decoupler.c \
	debug.h debug.c \
	parameters.h parameters.c
ka_SOURCES = $(sources)

AM_CFLAGS = -fplan9-extensions -Wno-unused-result

ka_LDFLAGS = -Wl,--export-dynamic

ka_DEPENDENCIES =

BUILT_SOURCES = version.h

CLEANFILES = $(BUILT_SOURCES)
