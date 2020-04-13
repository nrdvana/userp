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

# needed for default automake rule when processing test/ files
$(tests): .deps/test

include .deps/test/*.Po

.deps/test:
	mkdir -p .deps/test

.o.t:
	$(COMPILE) -o $@ $<

test: $(tests)
	prove -v test/

.PHONY: test
