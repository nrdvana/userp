/* This file includes anything on the local system needed for libuserp.
 * It will be modified when creating the distribution, and likely further
 * modified by the build tooling like autoconf when configured for a target.
 *
 * This content is just a placeholder that provides reasonable defaults
 * for a standard modern Linux system.
 */

// Adjust these as needed according to config macros
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define HAVE_POSIX_FILES
