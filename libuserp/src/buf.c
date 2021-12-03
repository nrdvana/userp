#include "local.h"
#include "userp_private.h"

/*
## Userp Buffers and Strings

### Synopsis:

    userp_buffer buf= userp_new_buffer(env, NULL, 1024, 0);
    if (!buf) ...
    memcpy(buf->data, byte_source, num_bytes);
    
    struct userp_bstr_part p= { .buf= buf, .idx= 0, .data= buf->data, .len= num_bytes };
    userp_bstr str= userp_new_bstr(env, 1, &p);
    if (!str) ...

### Description

To facilitate zero-copy designs, libuserp performs most of its operations on lists of strings
referencing spans of reference-counted buffers.  The buffers are fully exposed to the user, so you
have the option of allocting them and managing their lifecycle on your own terms.

A `userp_bstr` is defined as

```
typedef struct userp_bstr {
    userp_env env;
    size_t part_count, part_alloc;
    struct userp_bstr_part {
        userp_buffer buf;     // reference to the buffer that contains ->data
        uint8_t *data;        // start of data in this segment of the string
        size_t len;           // number of bytes in this part, starting at ->data
        size_t idx;           // logical offset from the start of the bstr
    } parts[];
} userp_bstr;

```

So in other words, the string is a variable-length struct.  This means any time you alter a string,
the number of parts might need to change, and it might get re-allocated (though this seldom happens
because the strings usually have some pre-allocated space at the end)

Each ``userp_bstr_part`` is hodling a reference to the buffer in which ``data`` points.  The buffer
may be reference-counted or static.  It is defined as:

```
typedef struct userp_buffer {
  void           *data;     // must always be non-NULL, points to start of buffer
  userp_env      env;       // if non-null, creator of buffer, will be called to free buffer
  size_t         alloc_len; // if nonzero, indicates how much memory is valid starting at 'data'
  size_t         refcnt;    // if nonzero, indicates this buffer is reference-counted
  int64_t        addr;      // indicates the origin/placement of this buffer, depends on flags
  uint_least32_t flags;     // indicates details of buffer
} userp_buffer;
```

A `struct userp_buffer` may be statically or dynamically allocated.  Static buffers should have
`env = NULL` and `refcnt = 0` so that the library does not try to free them.  (however, if you
need to know when the buffer is no longer being used, then write a special allocator to handle
this, create a special `env` to manage it, and set the initial `refcnt` to 1)  Buffers allocated
by libuserp come from the allocator of `env`, and will have an initial `refcnt` of 1 and the
`env` set.  **If the `refcnt` is 0, libuserp will assume that the buffer has a longer lifespan
than any libuserp object referencing it**.  If you create a buffer with `refcnt = 0`, make sure
that you free all libuserp objects that saw it before invalidating the buffer in any way.

The `alloc_len` field indicates the size of the buffer, but it may be zero to indicate that no
assumptions can be made about the buffer.  A zero value prevents things like trying to resize the
buffer or grow a string into unused space of the buffer.

The `addr` field can be used for several things, but usually to indicate the origin of the buffer.
For instance, if the buffer is memory-mapped, `addr` is the file offset the buffer is viewing. If
the buffer came from a stream, `addr` is the byte count since the start of the stream.  The `addr`
can also be used to store information about where a block-in-progress will be written.

`flags` holds a set of bit-flags adding to the semantics described above.  The following are
currently defined:
```      
  USERP_BUFFER_APPENDABLE // the buffer may be written and appended freely
  USERP_BUFFER_MEMMAP     // the buffer is memory-mapped
  USERP_BUFFER_ALIGN      // the 'addr' field is a bit alignment number, not an address
  USERP_BUFFER_USERBIT_0 .. USERP_BUFFER_USERBIT_7  // for custom use
```

`userp_bstr` is built on top of these buffers.  `userp_bstr` is a variable-length struct that
references byte ranges of buffers.  Because `userp_bstr` is variable-length, its address might
change any time its length is altered, so be sure to only reference a `userp_bstr` from one
official pointer, and pass that official pointer to any libuserp function that modifies the
string.

### Construction & Lifecycle

`userp_bstr` objects are not shared, so there is no reference counting mechanism.  They refer to
reference-counted buffers, so it is inexpensive to create a copy of the `userp_bstr`, and copying
instead of reference-counting saves a bit of complexity.  You can create `userp_bstr` from
dynamic memory using `userp_new_bstr` in which case they should be freed with `userp_free_bstr`.
If careful, you can also allocate them on the stack, but you **must not call any method that
modifies a bstr in that case**.  Stack allocation can be a quick/efficient way to specify the
components of a string in an API call to libuserp.  libuserp will never modify a `userp_bstr`
(since they are officially not a shared object) and will instead make a copy of its own to edit.
Each time you call an API that modifies `userp_bstr` you pass a pointer to the reference, to give
the library an opportunity to resize it, possibly altering the reference.

`userp_buffer` objects *are* shared.  In most cases, the struct and the data it points to should
be dynamically allocated from the allocator of a `userp_env`, using `userp_new_buffer`.  In rare
cases, where you can guarantee that the buffer will live longer than any libuserp object that
might get a reference to it, you could allocate your own buffers in some other manner.

Because of the reference count, there is not a `userp_free_buffer` function, and instead you use
`userp_drop_buffer` to decrement the reference count.

#### userp_new_buffer

    userp_buffer userp_new_buffer(userp_env env, void *data, size_t alloc_len, userp_flags flags);
    
    // Initialize with your own data pointer
    buf= userp_new_buffer(env, my_data, len, 0);
    
    // Ask for an allocation from env's allocator
    buf= userp_new_buffer(env, NULL, len, 0);

This function allocates a new `struct userp_buffer` and optionally also the `data` it points to.
If `data` is NULL and `alloc_len` is nonzero, this instructs the function to allocate `data`
of that size.  If `data` is provided, it becomes the `buffer->data` directly.  `alloc_len` is
likewise used to initialize `buffer->alloc_len`, and may be zero if you don't want the library to
make assumptions about this buffer.

`flags` may include:

  * Any of the `USERP_HINT_*` flags described in `userp_env`, used for allocating `data`
  * USERP_BUFFER_APPENDABLE
    the buffer acts as storage for write operations, and the library may write data into it
    up to alloc_len.
  * USERP_BUFFER_PERSIST
    the buffer struct is long-lived and should not be freed, and the `refcnt` is not used.
  * USERP_BUFFER_DATA_PERSIST
    the `data` is long-lived and should not be freed

#### userp_grab_buffer

    success= userp_grab_buffer(userp_env env, userp_buffer buf);

Acquire an additional reference to the buffer, returning true if successful.  This can fail if the
reference count exceeds the maximum, or the specified environment does not match the one that
created the buffer, or if you have `USERP_MEASURE_TWICE` enabled and the buffer is invalid.

The library checks that `env` matches `buf->env` to help ensure that the buffer is being used in
a safe manner; i.e. that the reference count is not being accessed by parallel threads.  You may
pass an `env` of NULL to skip this check and use `buf->env`.

If the buffer has the flag `USERP_BUFFER_PERSIST`, this function essentially does nothing because
`refcnt` is not used, on the assumption that the buffer lives longer than any references to it.

#### userp_drop_buffer

    userp_drop_buffer(userp_env env, userp_buffer buf);

Release a reference to a `userp_buffer`.  This has no effect if the buffer was allocated with the
flag USERP_BUFFER_PERSIST.  If this is the last reference to the buffer, it and its data may be
freed, except if the `*_PERSIST` flags were set.  This emits an error to the environment if the
environment was not the one that allocated the buffer.  If the buffer was already freed, this
causes undefined behaviour, unless the environment has `USERP_MEASURE_TWICE` enabled, in which
case you can rely on an error being emitted.

If the buffer was holding the last reference to the environment, the environment will also get
freed during this call.

*/

extern userp_buffer userp_new_buffer(userp_env env, void *data, size_t alloc_len, userp_buffer_flags flags) {
	userp_buffer buf= NULL;
	if (!USERP_ALLOC_OBJ(env, &buf))
		return NULL;
	buf->data= data;
	buf->env= env;
	buf->alloc_len= alloc_len;
	buf->refcnt= 1;
	buf->flags= flags;
	if (!buf->data && alloc_len) {
		// Round the buffer up to a power of 2, unless this is marked as a static allocation
		if (!(flags & USERP_HINT_STATIC)) {
			alloc_len |= alloc_len >> 1;
			alloc_len |= alloc_len >> 2;
			alloc_len |= alloc_len >> 4;
			alloc_len |= alloc_len >> 8;
			alloc_len |= alloc_len >> 16;
			#if SIZE_MAX > 0xFFFFFFFF
			alloc_len |= alloc_len >> 32;
			#endif
			alloc_len= USERP_BUFFER_DATA_ALLOC_ROUND(alloc_len);
		}
		// Let the allocator know that this is buffer data.  (allows the allocator to walk
		// back the pointer to get to this buffer object itself)
		if (!userp_alloc(env, (void**) &buf->data, alloc_len,
				(flags&USERP_ALLOC_FLAG_MASK)|USERP_POINTER_IS_BUFFER_DATA)
		) {
			USERP_FREE(env, &buf);
			return NULL;
		}
		buf->alloc_len= alloc_len;
		buf->flags |= USERP_BUFFER_DATA_ALLOC;
	}
	userp_grab_env(env);
	return buf;
}

static void userp_free_buffer(userp_buffer buf) {
	userp_env env= buf->env;

	// Free the data if it came from env->alloc
	if (buf->flags & USERP_BUFFER_DATA_ALLOC)
		userp_alloc(env, (void**) &buf->data, 0, USERP_POINTER_IS_BUFFER_DATA);

	// Free the buffer struct
	USERP_FREE(env, &buf);

	// drop strong reference to the env.  This may cause env to be destroyed
	userp_drop_env(env);
}

bool userp_grab_buffer(userp_buffer buf) {
	// Refcount will be zero if buffer is not dynamically allocated
	if (buf->refcnt && !++buf->refcnt) { // check for rollover
		--buf->refcnt; // back up to UINT_MAX
		if (buf->env)
			userp_diag_set(&buf->env->err, USERP_EALLOC, "Refcount limit reached for userp_buffer");
		return false;
	}
	return true;
}

bool userp_drop_buffer(userp_buffer buf) {
	if (buf->refcnt && !--buf->refcnt) {
		userp_free_buffer(buf);
		return true;
	}
	return false;
}

#ifdef UNIT_TEST

UNIT_TEST(buf_new_dynamic) {
	userp_env env= userp_new_env(logging_alloc, userp_file_logger, stdout, 0);
	userp_buffer buf= userp_new_buffer(env, NULL, 1024, 0);
	userp_drop_buffer(buf);
	userp_drop_env(env);
}
/*OUTPUT
/alloc 0x0+ to \d+ = 0x\w+/
/alloc 0x0+ to \d{2} = 0x\w+/
/alloc 0x0+ to \d{4} = 0x\w+ POINTER_IS_BUFFER_DATA/
/alloc 0x\w+ to 0 = 0x0+ POINTER_IS_BUFFER_DATA/
/alloc 0x\w+ to 0 = 0x0+/
/alloc 0x\w+ to 0 = 0x0+/
*/

char static_buffer[1024];
UNIT_TEST(buf_new_static) {
	userp_env env= userp_new_env(logging_alloc, userp_file_logger, stdout, 0);
	userp_buffer buf= userp_new_buffer(env, static_buffer, sizeof(static_buffer), 0);
	userp_drop_buffer(buf);
	userp_drop_env(env);
}
/*OUTPUT
/alloc 0x0+ to \d+ = 0x\w+/
/alloc 0x0+ to \d{2} = 0x\w+/
/alloc 0x\w+ to 0 = 0x0+/
/alloc 0x\w+ to 0 = 0x0+/
*/

UNIT_TEST(buf_on_stack) {
	uint8_t stack_buffer[1024];
	userp_env env= userp_new_env(logging_alloc, userp_file_logger, stdout, 0);
	struct userp_buffer buf= { .data= stack_buffer, .alloc_len= sizeof(stack_buffer) };
	userp_drop_buffer(&buf);
	userp_drop_env(env);
}
/*OUTPUT
/alloc 0x0+ to \d+ = 0x\w+/
/alloc 0x\w+ to 0 = 0x0+/
*/

#endif
