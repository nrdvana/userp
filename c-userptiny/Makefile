PERL=perl
CC=gcc
CFLAGS=-O0 -ggdb -Wall

test: unittest
	prove -lv

unittest.c: userptiny.c userptiny.h t/*.c config.h ../libuserp/script/UnitTestGen.pm
	$(PERL) ../libuserp/script/UnitTestGen.pm --include config.h --entrypoint=unittest.c --testdir=t userptiny.c t/*.c

unitest: unittest.c
	$(CC) -o unittest $(CFLAGS) unittest.c

.PHONY: test
