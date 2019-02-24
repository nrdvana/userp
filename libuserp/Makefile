MAKE = make
PROVE = prove
PERL = perl

all: build/Makefile
	$(MAKE) -C build -j4 all

autogen_files: build/Makefile
	$(MAKE) -C build autogen_files

build/Makefile build/config.h: configure automake/configure
	if [ -e build/config.status ]; then cd build && ./config.status; \
	else ./configure; fi # contains default for --enable-dev and --enable-debug

automake/configure: automake/configure.ac automake/Makefile.am
	cd automake && autoreconf

clean:
	$(MAKE) -C build clean

dist:
	env PROJ_ROOT=. $(PERL) scripts/build_dist_tarball.pl

test:
	$(MAKE) -C build daemonproxy
	$(PROVE) -j4

install:
	$(MAKE) -C build install

.PHONY: test all clean dist install
