ACLOCAL_AMFLAGS=-I m4

lib_LTLIBRARIES = libuserp.la
libuserp_la_SOURCES = userp_env.c

AM_CPPFLAGS=-I$(top_srcdir)

tests=$(subst .c,.t,$(wildcard test/*.c))

CP=cp
PROVE=prove

# This contains rules needed to auto-generate source files, but gets overwritten
# with a blank file when distributed.
include dev_rules.mak

.c.t:
	$(COMPILE) -o $@ $<

test: $(tests)
	prove -v test/

.PHONY: test
