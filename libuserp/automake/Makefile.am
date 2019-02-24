ACLOCAL_AMFLAGS=-I m4

lib_LTLIBRARIES = libuserp.la
libuserp_la_SOURCES = userp.c rbtree.c

@dev_include_makefile@
realsrcdir=$(top_srcdir)/../src
AM_CPPFLAGS=-I$(realsrcdir)

CP=cp

rbtree.c: $(realsrcdir)/rbtree.c
	$(CP) $< $@
userp.c: $(realsrcdir)/userp.c
	$(CP) $< $@
