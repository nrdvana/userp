#include "local.h"
#include "userp_private.h"

typedef struct userp_dec_frame *frame;

static void frame_init(userp_dec dec, frame f, userp_type t);
static void frame_destroy(userp_dec dec, frame f);

/*APIDOC
## userp_env

### Synopsis

  userp_dec dec= userp_new_dec(scope, root_type, null, buffer, n_bytes);
  if (!dec)
    die("can't initialize decoder");
  userp_symbol mydata_sym= userp_scope_get_symbol(scope, "mydata");
  if (!userp_dec_begin(dec, NULL)
	|| !userp_dec_seek_field(dec, mydata_sym)
	|| !(str= userp_dec_bytes_zerocopy(dec, sizeof(int), USERP_DEC_ARRAY)))
	die("Error loading 'mydata' from stream");
  int *my_data= (int*) str->parts[0].data;

### Description

A Userp decoder object parses a buffer of bytes that you supply.  It either decodes data nodes
into variables of the host language (copy) or marks the boundaries of a host-language-compatible
data node within the byte buffer (nocopy).

### Construction & Lifecycle

Decoder objects are created with `userp_new_dec`, referencing a `userp_scope`, root type, and
optional initial buffer.  The decoder increments the reference count of the scope, which in turn
is holding a reference to a `userp_env`.  The root type reference is also held by the scope,
either directly or by a parent scope.  Decoder objects are also reference counted, though
currently nothing in libuserp makes references to them.

When supplying byte strings for the decoder, you may include a reference to a `userp_buf`. If you
do, the decoder will add references to that buffer as needed.  If you do not, it is assumed that
you guarantee that the byte array has a longer lifespan that the decoder object.  (this is hard to
guarantee if you start mking multiple references to the decoder)

#### userp_new_dec

    userp_dec dec= userp_new_dec(
      env,           // environment to which userp_dec should be attached
      scope,         // userp_scope object, required
      root_type,     // userp_type reference, required, must be found in scope
      buffer_ref,    // optionl reference to userp_buf, used to track lifespan of bytes
      bytes,         // pointer to start of buffer for decoder
      n_bytes        // number of bytes available
    );
    ...
    userp_drop_dec(dec);

This creates a new decoder in the context of `env`.  The decoder is given a scope that determines
what types and symbols are available; it must(*) be identical to the scope that was used while
encoding the bytes.  The `root_type` specifies what to be decoded; it must also be the same as
used by the encoder.  The `buffer_ref` is an optional reference to a `userp_buf` used to track the
lifespan of the pointer `bytes`.  `buffer_ref` does not need to be created from the same
`userp_env`, but in a multithreaded program you must make sure that it comes from a `userp_env`
exclusive to the current thread.  `bytes` is a pointer to the beginning of the encoded data.  It
is optional, and you can supply more bytes later using `userp_dec_feed`.  If the `buffer_ref` is
not null, `bytes` must point within the bytes of that buffer.  If `n_bytes` is zero, `buffer_ref`
and `bytes` are ignored as if they were not supplied.

On success, this returns the new decoder, with a reference count of 1; free this when you are done
with it using `userp_drop_dec`.  On failure, it emits a diagnostic via `env` (which may be fatal)
and then returns NULL.

[*] unless you're really careful

#### userp_grab_dec

    bool success= userp_grab_dec(env, dec);

This adds a reference to the specified `userp_dec` object.  It can fail if the reference count
hits INT_MAX (unlikely) or if `dec` was previously destroyed. (but that case overlaps with
undefined behavior, unless you enable the USERP_MEASURE_TWICE option).  Errors (if any) are
reported via the `env`.  `env` may be null to deliberately skip any safety or error reporting.

#### userp_drop_dec

    userp_drop_dec(env, dec);

Release a reference to the `userp_dec`, possibly destroying it if the last reference is removed.
It is undefined behavior to drop a decoder that was already freed, unless you enable the
USERP_MEASURE_TWICE option.  Errors (if any) are reported via the `env`. `env` may be null to
deliberately skip any safety or error reporting.

*/

userp_dec userp_new_dec(
	userp_env env, userp_scope scope, userp_type root_type,
	userp_buffer buffer_ref, uint8_t *bytes, size_t n_bytes
) {
	userp_dec dec;
	USERP_CLEAR_ERROR(env);
	dec= userp_new_dec_silent(env, scope, root_type, buffer_ref, bytes, n_bytes);
	USERP_DISPATCH_ERR(env);
	return dec;
}

userp_dec userp_new_dec_silent(
	userp_env env, userp_scope scope, userp_type root_type,
	userp_buffer buffer_ref, uint8_t *bytes, size_t n_bytes
) {
	userp_dec dec= NULL;
	frame stack= NULL;
	size_t n_frames= USERP_DEC_FRAME_ALLOC_ROUND(1);
	size_t n_input_parts= 4;//enc->decoder_alloc_input_parts_typical;
	bool got_scope= false;

	if (!env->run_with_scissors) {
		if (!scope || !root_type || !userp_scope_contains_type(scope, root_type)) {
			userp_diag_set(&env->err, USERP_ETYPESCOPE, "Invalid root type");
			return NULL;
		}
		if (n_bytes && bytes && buffer_ref) {
			if (bytes < buffer_ref->data || bytes > buffer_ref->data + buffer_ref->alloc_len) {
				userp_diag_set(&env->err, USERP_EBUFPOINTER, "Byte pointer is not within buffer");
				return NULL;
			}
		}
	}
	// Perform all allocations, and if any fail, free them all
	if (!userp_alloc(env, (void**) &dec, sizeof(*dec) + sizeof(struct userp_bstr_part)*n_input_parts, USERP_HINT_STATIC, "userp_dec")
		|| !USERP_ALLOC_ARRAY(env, &stack, n_frames)
		// decoder holds a reference to the scope
		|| !(got_scope= userp_grab_scope_silent(env, scope))
		|| (n_bytes && bytes && buffer_ref && !userp_grab_buffer_silent(env, buffer_ref))
	) {
		if (got_scope) userp_drop_scope_silent(env, scope);
		USERP_FREE(env, &stack);
		USERP_FREE(env, &dec);
		// The userp_env->diag_code will already be set by one of the allocation functions
		return NULL;
	}
	bzero(dec, sizeof(*dec));
	dec->env= env;
	dec->scope= scope;
	// initialize the first decoder stack element with the root type
	dec->stack= stack;
	dec->stack_lim= n_frames;
	frame_init(dec, &stack[0], root_type);
	// initial storage for input bstr is found at end of this record
	dec->input= &dec->input_inst;
	dec->input_inst.part_alloc= n_input_parts;
	// Initialize buffer, if provided
	if (n_bytes && bytes) {
		dec->input_inst.part_count= 1;
		dec->input_inst.parts[0].data= bytes;
		dec->input_inst.parts[0].len= n_bytes;
		dec->input_inst.parts[0].buf= buffer_ref; // ref count was handled above
	}
	return dec;
}

void dec_free(userp_dec dec) {
	size_t i;
	do {
		frame_destroy(dec, &dec->stack[dec->stack_i]);
	} while (dec->stack_i-- > 0); 
	USERP_FREE(dec->env, &dec->stack);
	if (dec->input == &dec->input_inst) {
		for (i= 0; i < dec->input_inst.part_count; i++) {
			if (dec->input_inst.parts[i].buf)
				userp_drop_buffer(dec->env, dec->input_inst.parts[i].buf);
		}
	} else if (dec->input)
		userp_bstr_free(dec->input);
	userp_drop_scope_silent(dec->env, dec->scope);
	USERP_FREE(dec->env, &dec);
}

bool userp_grab_dec(userp_env env, userp_dec dec) {
	if (env) USERP_CLEAR_ERROR(env);
	if (userp_grab_dec_silent(env, dec))
		return true;
	if (env) USERP_DISPATCH_ERR(env);
	return false;
}

bool userp_grab_dec_silent(userp_env env, userp_dec dec) {
	if (env && env->measure_twice) {
		unimplemented("verify decoder belongs to env");
		// remove decoder from env's set if decoder is getting freed
	}
	if (++dec->refcnt)
		return true;
	// else, overflow
	--dec->refcnt;
	if (env)
		unimplemented("report grab_dec failed");
	return false;
}

void userp_drop_dec(userp_env env, userp_dec dec) {
	if (env) USERP_CLEAR_ERROR(env);
	userp_drop_dec_silent(env, dec);
	if (env) USERP_DISPATCH_ERR(env);
}

void userp_drop_dec_silent(userp_env env, userp_dec dec) {
	if (env && env->measure_twice) {
		unimplemented("verify dec belongs to env");
	}
	if (dec->refcnt && !--dec->refcnt) {
		dec_free(dec);
	}
}

void frame_init(userp_dec dec, frame f, userp_type t) {
	bzero(f, sizeof(*f));
	f->node_type= t;
	unimplemented("initialize frame");
}
void frame_destroy(userp_dec dec, frame f) {
	unimplemented("destroy frame");
}


/*APIDOC
### Attributes

#### env

   userp_env env= userp_dec_env(dec);

Return the reference to the `userp_env` object this decoder is attached to.  The reference count
is unchanged.

#### scope

    userp_scope scope= userp_dec_scope(dec);

Return the scope this decoder was created from.  The reference count is unchanged.

#### reader

    userp_reader_fn userp_dec_reader

    void userp_dec_set_reader(
      userp_dec dec,
      userp_reader_fn my_callback,
      void *callback_param
    );

If you decide to construct the decoder before all data is available, you can add a callback that
will get called any time the decoder reaches the end of input and needs more bytes.  Without this
callback, the decode functions will emit an EOF error and return false.  With this callback, you
can perform a blocking operation to fetch the next chunk of data and `userp_dec_feed` it to the
decoder, avoiding the error condition.

You may pass NULL to un-set the reader.

*/

userp_env userp_dec_env(userp_dec dec) {
	return dec->env;
}

userp_scope userp_dec_scope(userp_dec dec) {
	return dec->scope;
}

void userp_dec_set_reader(userp_dec dec, userp_reader_fn reader, void *callback_data) {
	dec->reader= reader;
	dec->reader_cb_data= callback_data;
}

/*APIDOC
#### node_info

    struct userp_node_info {
        userp_type node_type;
        size_t node_depth;
        size_t subtype_count;
        const userp_type *subtypes;
        size_t array_dim_count;
        const size_t *array_dims;
        size_t elem_count;
        const uint8_t
            *data_start,
            *data_limit;
    } *userp_node_info;
    const userp_node_info info= userp_dec_node_info(dec);

Return the information about the "current node".  If the decoder hits the end of the input before
it can fully populate this struct, it calls the reader callback (if any) and if it still can't
complete the node into, it returns NULL and emits the error `USERP_EOF` via the `userp_env`
associated with this decoder.  Any other decoder error will likewise emit an error and return
NULL.

When iterating an array or record, after the final element has been decoded this function will
return a `userp_node_info` with `node_type == NULL`, indicating that there are no further nodes,
until you call `userp_dec_end` to return to the parent node.  `node_depth` will still be set to
one higher than the parent node.  The root node has a `node_depth == 0` and after decoding it the
`node_depth` remains 0.

The struct returned is **only valid until the next call to userp_dec_node_info** and may be
recycled.  Attempting to alter this struct (which is deliberately marked const) results in
undefined behavior.  Do not make shallow copies of this struct either, as the referenced arrays
like `subtypes` and `array_dims` are similarly transient.

*/

const userp_node_info userp_dec_node_info(userp_dec dec) {
	unimplemented("userp_dec_node_info");
}

/*APIDOC

### Tree Navigation Functions

#### userp_dec_begin

  size_t n_elems;
  bool success= userp_dec_begin(dec);

Begin iterating the array or record elements of the current node.  The current node becomes the
parent node, and the new current node is element 0 of the array, or the first encoded field of
the record.  You may `userp_dec_begin` an empty array or record, but the current element will have
a type of NULL and the only valid cursor operation will be to call `userp_dec_end` or
`userp_dec_rewind`.

If the current node is not an array or record, this returns false and sets an error flag.

#### userp_doc_end

    bool success= userp_dec_end(dec);

Stop iterating an array or record, return to that parent node, and then set the current node to
whatever follows it.

*/

bool userp_dec_begin(userp_dec dec) {
	unimplemented("userp_dec_begin");
}

bool userp_dec_end(userp_dec dec) {
	unimplemented("userp_dec_end");
}

/*APIDOC
#### userp_dec_seek_elem

    bool success= userp_dec_seek_elem(dec, elem_idx);

Seek to the specified field name or array index, changing the current node to that element.

If the element does not exist, the current node is unchanged and the function
returns false.  If there is no current array or record being iterated, this returns false and
sets a error flag on the `userp_env`.

*/
bool userp_dec_seek_elem(userp_dec dec, size_t elem_idx) {
	unimplemented("userp_dec_seek");
}

/*APIDOC
#### userp_dec_seek_field

  userp_symbol sym= userp_scope_get_symbol(scope, "my_field");
  bool success= userp_dec_seek_field(dec, sym);

Seek to the named field of the most recent parent record node.  If the record does not contain
this field, this returns false.  If there is no record being iterated, or the symbol is NULL, or
the symbol is not part of the current scope (regardless of its string value) this returns false
and sets an error flag in `userp_env`.

*/
bool userp_dec_seek_field(userp_dec dec, userp_symbol fieldname) {
	unimplemented("userp_dec_seek_field");
}

/*APIDOC
#### userp_dec_skip

Skip the current node and move to the next element in the parent array or record.

To skip more than one element, use `userp_seek_elem`.

*/
bool userp_dec_skip(userp_dec dec) {
	unimplemented("userp_dec_skip");
}

/*APIDOC

### Decoding Functions

#### userp_dec_int

  int int_out;
  bool success= userp_dec_int(dec, &int_out);

Decode the current node as an integer, and move to the next node if successful.

If the current node is an integer-compatible type and fits within an `int`, this stores
the value into `*int_out` and moves the internal iterator to the next node, and returns true.
If one of those pre-conditions is not true, this returns false and emits an error via
the `userp_env`.

#### userp_dec_int_n

    long long int_out;
    bool success= userp_dec_int_n(dec, &int_out, sizeof(int_out), true);
    uint64_t u64;
    bool success= userp_dec_int_n(dec, &u64, sizeof(u64), false);

Decode the current node as an arbitrary-sized integer.  Same as userp_dec_int but you can
specify any byte length.  The entire number of bytes will be filled in host-endian order,
and optionally signed.  Decoding a negative number into an unsigned integer is an error.

#### userp_dec_bigint

  char buffer[256];
  size_t len= sizeof(buffer);
  int sign;
  bool success= userp_dec_intbuf(dec, buffer, &len, &sign);

Like `userp_dec_int` but stores the bytes of the integer into a buffer.  The bufer is filled to
a multiple of `sizeof(long)`, and the `len` is updated to the number of bytes written.
If `sign` is supplied, the sign bit will be written here instead of generating twos-complement
encoding.  If `sign` is not supplied, the value will always be two's-complement up to the number
of bytes written.  (the remainder of the buffer is not filled)

If the current node is not an integer, or it does not fit into the buffer, this returns false and
sets an error on the `userp_env`.  (to find the size of buffer required, inspect the node_info)

*/
bool userp_dec_int(userp_dec dec, int *out) {
	unimplemented("userp_dec_int");
}
bool userp_dec_int_n(userp_dec dec, void *intbuf, size_t word_size, bool is_signed) {
	unimplemented("userp_dec_int_n");
}
bool userp_dec_bigint(userp_dec dec, void *intbuf, size_t *len, int *sign) {
	unimplemented("userp_dec_bigint");
}

/*
#### userp_dec_symbol

  userp_symbol sym_out;
  bool success= userp_dec_symbol(dec, &sym_out);

Decode the current node as a Symbol, and move to the next node if successful.

If the current node is a symbol-compatible value (such as plain Symbol or an enumerated
integer value) return the Symbol.  Else returns NULL and sets an error flag in the `userp_env`.

*/
bool userp_dec_symbol(userp_dec dec, userp_symbol *out) {
	unimplemented("userp_dec_symbol");
}

/*
#### userp_dec_typeref

  userp_type type_out;
  bool success= userp_dec_typeref(dec, &type_out);

Decode the current node as a Type reference, and move to the next node if successful.

If the current node is a reference to a type and the type is a valid reference in the current
scope stack, return the type and advance to the next node.  Else it returns NULL and sets an
error flag in the `userp_env`.

*/
bool userp_dec_typeref(userp_dec dec, userp_type *out) {
	unimplemented("userp_dec_typeref");
}

/*
#### userp_dec_float

    float f;
    bool success= userp_dec_float(dec, &f);

Decode the current node as a `float` and move to the next node.

If the current node is a floating-point record that fits in `float`, or is an integer which can be
losslessly loadded into a `float` (unless truncate options are enabled in the environent), this
stores the value into `*out` and returns true.  Else it returns false and sets an error flag.

#### userp_dec_double

    double d;
    bool success= userp_dec_double(dec, &d);

Same as `userp_dec_float` but with storage into a double.

*/
bool userp_dec_float(userp_dec dec, float *out) {
	unimplemented("userp_dec_float");
}
bool userp_dec_double(userp_dec dec, double *out) {
	unimplemented("userp_dec_double");
}

/*
#### userp_dec_bytes

  int buffer[500];
  size_t len= sizeof(buffer);
  int elem_size= sizeof(*buffer);
  bool success= userp_dec_bytes(dec, buffer, &len, elem_size, flags);

Copy out the bytes of the current node, as-is.  This is intended primarily to dump out arrays of
fixed-length integers (like reading char[], int[], float[] and so on).  This can be an error-prone
operation, so you should first verify that the type of the current node matches your expectations.
A good way to do this is to declare a type that matches your struct or array (which can be
generated at compile time) and then call `userp_type_has_equiv_encoding` to find out if it is
binary-compatible with the current node's type.

The flags determine which kind of copying is performed and which circumstances it is permitted for.

Flags:
  - USERP_COPY_STRNUL - ensure that the array is made of bytes and does not contain NUL, and then
    append a NUL at the end in the destination buffer.  (the copy is only performed if the buffer
	is large enough for string + NUL byte)
  - USERP_COPY_ARRAY - permit copying arrays
  - USERP_COPY_STRUCT - permit copying structs
  - USERP_COPY_STRUCT_DYN_TAIL - permit copying one struct with a variable length final field
  - USERP_FORCE_COPY - copy the protocol bytes even if it contains dynamic protocol things that
    don't make sense in a C struct or array.

As a further safeguard, if `elem_size` is nonzero, this function will verify that the array
element or struct size matches that.  As a special case for uses like

  struct { int a, b, c, d;  int len; int array[]; }

if the fixed-length portion of the record is immediately followed by an array field which has a
fixed-length element count, then it will fall back to checking if the `elem_size` matches the size
of the static record plus the size of the array length.

On success, it returns true and the number of bytes copied into the buffer is stored into `len`.
If the buffer is too small, or the `elem_size` was given and does not match, or the type does not
meet the restrictions of the `flags` in effect, this returns false and sets an error code in
`userp_env`.  **On failure, if the error code is `USERP_EBUFSIZE`, this function will stil update
the len argument to let you know how many bytes were required.**

*/
bool userp_dec_bytes(userp_dec dec, void *out, size_t *length_inout, size_t elem_size, userp_flags flags) {
	unimplemented("userp_dec_bytes");
}

/*
### userp_dec_bytes_zerocopy

  size_t len= sizeof(buffer);
  int elem_size= sizeof(*buffer);
  userp_bstrings str= userp_dec_bytes_zerocopy(dec, size_t elem_size, int flags);

This is the same as `userp_dec_bytes` but instead of copying bytes into a user-supplied buffer,
the library provides a temporary view of the source buffer itself so that the application can
perform its own copying, or use the data directly.

If successful (under the same conditions described for `userp_dec_bytes`) this returns a reference
to a `userp_bstrings` struct.  The struct is owned by the decoder and only guaranteed to live
until the next call to a userp_dec function that performs decoding.  However, you may inspect the
buffers it points to, which may have a reference count available, allowing you to hold onto them
without needing to make a copy of the buffer data.

In ideal circumstances, if you gave the decoder a single buffer to parse, there will only be one
byte range of that same buffer referenced in the return value.  However it is not guaranteed
because sometimes the library might need to make small changes to the data, and it does this by
allocating temporary buffers to hold the edits, paired with references to the original buffers
for any data that didn't need changed.

On failure, this function returns NULL and sets a code in `userp_env` as usual.

*/
userp_bstrings userp_dec_bytes_zerocopy(userp_dec dec, size_t elem_size, userp_flags flags) {
	unimplemented("userp_dec_bytes_zerocopy");
}

/* Zerocopy API

libuserp offers several decoding functions to facilitate reading data from the
stream without copying it.  Using these functions exposes a bit of the underlying
buffering of the library.  Userp tracks all source data in shared buffers with
reference counting.  It then has another layer of "buffer ranges" objects which
hold references to the shared buffers.  Shared buffers are bound to a single
userp_env object, because userp_env objects define the allocation function used
to resize or free the buffers.

If you dynamically fill your buffers, (such as from a reader callback) then they
have been dynamicallly allocated and need freed.  In order to make sure that
the zerocopy pointers you have don't get freed until you are done with them, you
need to inform the library about which ones you are still using.
Use `userp_buffer_ranges_clone` to create a buffer_ranges object of your own that
has strong references to the underlying buffer objects.  If you do this, the
userp_env will not be freed until all buffer ranges objects you hold have been freed.

However, you can skip most of this complexity if you know for a fact that all the
buffers userp is parsing are under your control.  For example, if you memory-map
a file and then tell userp to wrap that buffer, userp knows it did not allocate
that buffer so it will not free it, and all the pointers returned in the buffer
ranges can safely be used by your program until you unmap the file.  Also, if the
entire stream is found in one buffer like that, you can always assume that there
will be exactly one range returned by any successful Zerocopy API call, making it
safe to write code like

  userp_buffer_ranges ranges;
  if ((ranges= SOME_API_zerocopy(...))) {
	my_data_structure->something= (MySomething*) ranges->range[0].start;
  }

*/
