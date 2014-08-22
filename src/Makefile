build:
	[ -d ../build ] || mkdir ../build
	[ ../build/Makefile -nt Makefile -a ../build/Makefile -nt ../configure ] || ( cd ../build && ../configure; );
	cd ../build && $(MAKE)

libuserp_OBJS=RBTree.o

RBTree.o: RBTree.c RBTree.h
	$(COMPILE)

out/$(conf_install_libdir)/$(conf_target_libuserp_name): $(libuserp_OBJS)
	$(LINK)
