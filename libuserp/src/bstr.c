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

This function allocates a new `struct userp_bstr` and optionally also the `data` it points to.
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

*/

userp_buffer userp_new_buffer(userp_env env, void *data, size_t alloc_len, userp_flags flags) {
	userp_buffer buf= NULL;
	if (!USERP_ALLOC_OBJ(env, &buf))
		return NULL;
	if (!data && alloc_len) {
		alloc_len= USERP_BUFFER_DATA_ALLOC_ROUND(alloc_len);
		if (!USERP_ALLOC_ARRAY(env, &data, uint8_t, alloc_len, flags)) {
			USERP_FREE(env, &buf);
			return NULL;
		}
		flags &= ~USERP_BUFFER_DATA_PERSIST;
	}
	buf->data= data;
	buf->env= env;
	buf->alloc_len= alloc_len;
	buf->refcnt= 1;
	buf->addr= 0;
	buf->flags= flags & ~USERP_BUFFER_PERSIST;
	return buf;
}

static void userp_free_buffer(userp_buffer buf) {
	userp_env env;
	// Buffer cannot be freed unless env is set
	assert(buf != NULL);
	assert(buf->env != NULL);
	env= buf->env;
	// Free the data if it came from env->alloc
	if (!(buf->flags & USERP_BUFFER_DATA_PERSIST))
		USERP_FREE(env, &buf->data);
	// userp_env can have a weak reference to a buffer, for diagnostics.
	// If this is that buffer, set the reference to NULL.
	if (&env->err.buf == buf)
		env->err.buf= NULL;
	else if (&env->diag.buf == buf)
		env->diag.buf= NULL;
	// Free the buffer struct
	if (env->measure_twice)
		bzero(buf, sizeof(*buf));
	USERP_FREE(env, &buf);
	// drop strong reference to the env.  This may cause env to be destroyed
	userp_drop_env(env);
}

bool userp_grab_buffer(userp_buffer buf) {
	struct userp_diag diag;
	if (!buf) return false;
	// If the buffer is persistent, the refcnt is not used, and just assume we
	// can make as many references as needed to it.
	if (buf->flags & USERP_BUFFER_PERSIST)
		return true;
	if (!buf->env)
		// don't know where to report the error
		return false;
	if (!buf->refcnt) {
		userp_diag_set(&buf->env->err, USERP_EBADSTATE, "Attempt to grab a destroyed userp_buffer");
		return false;
	}
	if (!++buf->refcnt) {
		userp_diag_set(&buf->env->err, USERP_EALLOC, "Reference count exceeds size_t");
		--buf->refcnt;
		return false;
	}
	return true;
}

void userp_drop_buffer(userp_buffer buf) {
	struct userp_diag diag;
	if (buf && !(buf->flags & USERP_BUFFER_PERSIST)) {
		if (!buf->refcnt)
			userp_diag_set(&buf->env->err, USERP_EBADSTATE, "Attempt to grab a destroyed userp_buffer");
		else if (!--buf->refcnt)
			userp_free_buffer(buf);
	}
}

//bool userp_grow_buffer(userp_buffer buf, size_t alloc_len) {
//	if (!buf->env) return false;
//	if (!(buf->flags & USERP_BUFFER_ENV_ALLOC)) {
//		userp_env_set_error(buf->env, USERP_EINVAL, "buffer is not resizable");
//		return false;
//	}
//	if (buf->alloc_len >= alloc_len) return true;
//	alloc_len= USERP_BUFFER_DATA_ALLOC_ROUND(alloc_len);
//	return USERP_ALLOC_ARRAY(buf->env, &buf->data, uint8_t, alloc_len, USERP_HINT_DYNAMIC);
//}
//
//userp_bstr userp_new_bstr(userp_env env, int part_alloc_count) {
//	userp_bstr str= NULL;
//	if (!USERP_ALLOC(env, &str, SIZEOF_USERP_BSTR(part_alloc_count)))
//		return NULL;
//	str->env= env;
//	str->part_count= 0;
//	str->part_alloc= part_alloc_count;
//	return str;
//}
//
//bool userp_free_bstr(userp_bstr *str) {
//	size_t i;
//	for (i= 0; i < (*str)->part_count; i++) {
//		if ((*str)->parts[i].buf && (*str)->parts[i].buf->refcnt)
//			userp_drop_buffer((*str)->parts[i].buf);
//	}
//	USERP_FREE((*str)->env, str);
//}
//
//void* userp_bstr_append_bytes(userp_bstr *str, size_t n_bytes, const void* src_bytes) {
//	userp_buffer buf;
//	struct userp_bstr_part *part;
//	void *ret= NULL;
//	// In order to append to an existing buffer, it must have an 'env', it must be flagged
//	// USERP_BUFFER_APPENDABLE, and must have a refcnt of 1.
//	if (str->part_count) {
//		part= &(*str)->parts[str->part_count-1];
//		buf= part->buf;
//		if (buf && buf->env && (buf->flags | USERP_BUFFER_APPENDABLE)) {
//			// This should always be true, but if not, don't touch this buffer segment
//			if (part->data >= buf->data && part->len < buf->alloc_len && (parts->data - buf->data) <= buf->alloc_len - part->len) {
//				// Can the buffer already hold an additional n_bytes?
//				if ((part->data - buf->data) + part->len + n_bytes <= buf->alloc_len)
//					ret= part->data + part->len;
//			}
//		}
//	}
//	if (!ret) { // need a new buffer?
//		// Ensure room for a new part
//		if ((*str)->part_count >= (*str)->part_alloc && !userp_bstr_append_parts(str, 1, NULL))
//			return NULL;
//		// Then allocate another buffer for this new part
//		if (!(buf= userp_new_buffer((*str)->env, NULL, n_bytes, USERP_BUFFER_APPENDABLE)))
//			return NULL;
//		// Add the part
//		part= &(*str)->parts[(*str)->part_count++];
//		part->buf= buf;
//		part->data= buf->data;
//		part->len= 0;
//		ret= part->data;
//	}
//	if (src_bytes) {
//		memcpy(ret, src_bytes, n_bytes);
//		part->len += n_bytes;
//	}
//	return part->data;
//}
//
//bool userp_bstr_append_parts(userp_bstr *str, size_t n_parts, const struct userp_bstr_part *src_parts) {
//	size_t n, i, ofs_diff;
//	struct userp_bstr_part *new_parts;
//	// is there room for this number of parts?
//	if ((*str)->part_count + n_parts > (*str)->part_alloc) {
//		// round up to multiple of 16, leaving at least 8 slots open
//		n= USERP_BSTR_PART_ALLOC_ROUND( (*str)->part_count + n_parts );
//		// realloc, possibly changing *str
//		if (!USERP_ALLOC((*str)->env, str, SIZEOF_USERP_BSTR((*str)->part_count + n_parts)))
//			return false;
//		(*str)->part_alloc= n;
//	}
//	// If src_parts is not given, then the only change is enlarging the string object, above.
//	// If it is given, then also copy the parts onto the end of the string and update the
//	// offsets and grab references to the buffers.
//	if (src_parts) {
//		new_parts= (*str)->parts + (*str)->part_count;
//		memcpy(new_parts, src_parts, sizeof(struct userp_bstr_part)*n_parts);
//		ofs_diff= ((*str)->part_count == 0? 0 : (new_parts[-1].ofs + new_parts[-1].len)) - src_parts[0].ofs;
//		for (i= 0; i < n_parts; i++) {
//			new_parts[i].ofs += ofs_diff;
//			if (i > 0 && new_parts[i].ofs < new_parts[i-1].ofs + new_parts[i-1].len) {
//				// invalid indexing on string
//				(*str)->env->diag_pos= i;
//				fatal_error((*str)->env, USERP_ESTRINDEX, "src_parts[" USERP_DIAG_POS "].ofs is invalid");
//				return false;
//			}
//		}
//		for (i= 0; i < n_parts; i++)
//			userp_grab_buffer(new_parts[i].buf);
//		(*str)->part_count += n_parts;
//	}
//	return true;
//}

#ifdef HAVE_POSIX_FILES

bool userp_bstr_append_file(userp_bstr *str, int fd, size_t length) {
	int got;
	void *dest= userp_bstr_append_bytes(str, length, NULL);
	if (!dest)
		return false;
	got= read(fd, dest, length);
	if (got > 0) {
		((*str)->parts + (*str)->part_count - 1)->len += got;
		return true;
	}
	else if (got == 0
		|| errno == EAGAIN
		|| errno == EINTR
		|| errno == EWOULDBLOCK
	) {
		userp_env_set_error((*str)->env, USERP_EEOF, "No bytes ready to read from stream");
	}
	else {
		(*str)->env->diag_pos= errno;
		userp_env_set_error((*str)->env, USERP_ESYSERROR, "System error " USERP_DIAG_POS " while calling read()");
	}
	return false;
}

#endif

#ifdef HAVE_POSIX_MEMMAP

bool userp_bstr_map_file(userp_bstr *str, int fd, int64_t offset, int64_t length) {
	// Does the string already have a part adjacent or overlapping with this request?
}

#endif

/*
bool userp_bstr_splice(userp_bstr *dst, size_t dst_ofs, size_t dst_len, userp_bstr *src, size_t src_ofs, size_t src_len) {
}

void userp_bstr_crop(userp_bstr *str, size_t trim_head, size_t trim_tail) {
}
*/
