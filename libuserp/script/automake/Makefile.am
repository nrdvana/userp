build_script_dir = ../script

ACLOCAL_AMFLAGS=-I m4

lib_LTLIBRARIES = libuserp.la
libuserp_la_SOURCES = diag.c

check_PROGRAMS = unittest
unittest_SOURCES = unittest.c
unittest_autoscan = diag.c
unittest_autogen_tests = $(addprefix t/,$(patsubst %.c,%.t,$(unittest_autoscan)))

AM_CPPFLAGS=-I$(top_srcdir)

CP=cp
PROVE=prove

# This contains rules needed to auto-generate source files, but gets overwritten
# with a blank file when distributed.
include dev_rules.mak

test: unittest
	env TEST_ENTRYPOINT=unittest prove -lv t/

.PHONY: test
