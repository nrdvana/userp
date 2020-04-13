MAKE = make
PROVE = prove
PERL = perl

all: build/Makefile
	$(MAKE) -C build -j4 all

build/Makefile build/config.h: configure script/automake/*
	$(PERL) script/build_dist.pl -d build --symlink

dist:
	$(PERL) script/build_dist.pl -d dist --clobber --tarball

# These targets pass through to the makefile of the build/ directory

clean:
	$(MAKE) -C build clean

test:
	$(MAKE) -C build test

install:
	$(MAKE) -C build install

.PHONY: test all clean dist test install
