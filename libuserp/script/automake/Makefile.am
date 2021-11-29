build_script_dir = ../script

ACLOCAL_AMFLAGS=-I m4

lib_LTLIBRARIES = libuserp.la
libuserp_la_SOURCES = diag.c env.c bstr.c dec.c

check_PROGRAMS = unittest
unittest_SOURCES = unittest.c
unittest_autoscan = diag.c env.c buf.c
unittest_autogen_tests = $(addprefix t/,$(patsubst %.c,%.t,$(unittest_autoscan)))

AM_CPPFLAGS=-I$(top_srcdir)

CP=cp
PERL=perl
PROVE=prove

# This contains rules needed to auto-generate source files, but gets overwritten
# with a blank file when distributed.
include dev_rules.mak

test: unittest $(unittest_autogen_tests)
	env UNITTEST_ENTRYPOINT=unittest prove -lv t/

.PHONY: test
