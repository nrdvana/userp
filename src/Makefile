build:
	[ -d ../build -a ../build/Makefile -nt Makefile -a ../build/Makefile -nt ../configure ] || ( cd .. && ./configure update_build; );
	cd ../build && $(MAKE)

libuserp_OBJS=RBTree.o

RBTree.o: RBTree.c RBTree.h
	$(COMPILE)

out/$(conf_install_libdir)/$(conf_target_libuserp_name): $(libuserp_OBJS)
	$(LINK)
