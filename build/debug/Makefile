SRCDIR=../../src
CFLAGS=-MT $@ -MD -MP -MF .deps/$*.Tpo
CXXFLAGS=-MT $@ -MD -MP -MF .deps/$*.Tpo

.PHONY: updatesymlinks

all: updatesymlinks libuserp.so test

updatesymlinks:
	find -type l -regex '.*.[ch]p?p?' | xargs rm
	ln -s $(SRCDIR)/*.h $(SRCDIR)/*.cpp .

test:

libuserp.so: OctetSequence.o BufferTracker.o
	./run liblink output="$@" objs="$<" CXXFLAGS="\"$(CXXFLAGS)\""

OctetSequence.o: OctetSequence.cpp
	./run compile output="$@" sources="$<" CXXFLAGS="\"$(CXXFLAGS)\"" && mv .deps/$*.Tpo .deps/$*.Po
BufferTracker.o: BufferTracker.cpp
	./run compile output="$@" sources="$<" CXXFLAGS="\"$(CXXFLAGS)\"" && mv .deps/$*.Tpo .deps/$*.Po

%.Po:
	touch $@

include .deps/OctetSequence.Po
include .deps/BufferTracker.Po
