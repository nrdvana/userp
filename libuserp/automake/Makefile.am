ACLOCAL_AMFLAGS=-I m4

lib_LTLIBRARIES = libuserp.la
libuserp_la_SOURCES = userp.c rbtree.c

@dev_include_makefile@
realsrcdir=$(top_srcdir)/../src
AM_CPPFLAGS=-I$(realsrcdir)

CP=cp
PROVE=prove

rbtree.h rbtree.c: $(realsrcdir)/RBGen.pm
	perl -I$(realsrcdir) -MRBGen -e 'RBGen->new(node_type => "userp_rbnode_t")->write_api("rbtree.h")->write_impl("rbtree.c")'

userp.c: $(realsrcdir)/userp.c
	$(CP) $< $@
