#! /bin/sh
CFLAGS="$1"
CXXFLAGS="$2"

if ! [[ "$CFLAGS" == *-g* && "$CXXFLAGS" == *-g* ]]; then
	echo ' *'
	echo ' * Error: You need to configure the project with ./configure CFLAGS="-O0 -g" CPPFLAGS="-O0 -g"'
	echo ' *'
	false
elif ! [[ "$CFLAGS" == *-O0* && "$CXXFLAGS" == *-O0* ]]; then
	echo ' *'
	echo ' * Warning: project was not configured with "-O0"... variables and paramerers (and even'
	echo " *          stack frames) might get optimized to the point gdb can't report them."
	echo ' *'
fi
