ACLOCAL_AMFLAGS=-I m4

lib_LTLIBRARIES = libuserp.la
libuserp_la_SOURCES = userp.c rbtree.c

@dev_include_makefile@
realsrcdir=$(top_srcdir)/../src
testsrcdir=$(top_srcdir)/../t
AM_CPPFLAGS=-I$(realsrcdir)

CP=cp
PROVE=prove

rbtree.h rbtree.c: $(realsrcdir)/RBGen.pm
	perl -I$(realsrcdir) -MRBGen -e 'RBGen->new(namespace => "userp_rb")->write_api("rbtree.h")->write_impl("rbtree.c")'

ident_rb.h: $(realsrcdir)/RBGen.pm
	perl -I$(realsrcdir) -MRBGen -e 'RBGen->new(namespace => "userp_rb")->write_wrapper("ident_rb.h", obj_t => "userp_ident_t", node => "name_tree_node", key_t => "const char *", key => "name", tree_t => "userp_ident_by_name_t", cmp => "strcmp")'

userp.c: $(realsrcdir)/userp.c ident_rb.h
	$(CP) $< $@

testfiles=t/02-rbtree.t

test: t $(testfiles)
	$(PROVE) -lv

t:
	mkdir -p t

t/02-rbtree.c: $(testsrcdir)/02-rbtree.c $(testsrcdir)/lib/CTap.*
	perl -I$(testsrcdir)/lib -MCTap -e 'my $$out= shift; CTap->new->load(@ARGV)->write($$out)' $@ $(testsrcdir)/02-rbtree.c

t/02-rbtree.t: t/02-rbtree.c rbtree.o ident_rb.h
	$(COMPILE) -I$(testsrcdir)/lib -o $@ $< rbtree.o
