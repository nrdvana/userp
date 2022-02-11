#include "local.h"
#include "userp_private.h"

typedef struct userp_node_info_private node_info;
typedef struct userp_dec_frame *frame;

// static void frame_init(userp_dec dec, frame f, userp_type t);
// static void frame_destroy(userp_dec dec, frame f);

static userp_error_t userp_decode_vint(userp_node_info_private *node, userp_dec_input *in, bool is_signed);
static userp_error_t userp_decode_vsize(size_t *out, userp_dec_input *in);
static bool userp_dec_input_next_buffer(userp_dec_input *in);

/*APIDOC
## userp_dec

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

*

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

*

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

*

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
	return false;
}

bool userp_dec_end(userp_dec dec) {
	unimplemented("userp_dec_end");
	return false;
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
	return false;
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
	return false;
}

/*APIDOC
#### userp_dec_skip

Skip the current node and move to the next element in the parent array or record.

To skip more than one element, use `userp_seek_elem`.

*/
bool userp_dec_skip(userp_dec dec) {
	unimplemented("userp_dec_skip");
	return false;
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
	return false;
}
bool userp_dec_int_n(userp_dec dec, void *intbuf, size_t word_size, bool is_signed) {
	unimplemented("userp_dec_int_n");
	return false;
}
bool userp_dec_bigint(userp_dec dec, void *intbuf, size_t *len, int *sign) {
	unimplemented("userp_dec_bigint");
	return false;
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
	return false;
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
	return false;
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
	return false;
}
bool userp_dec_double(userp_dec dec, double *out) {
	unimplemented("userp_dec_double");
	return false;
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
bool userp_dec_bytes(userp_dec dec, void *out, size_t *length_inout, size_t elem_size, int flags) {
	unimplemented("userp_dec_bytes");
	return false;
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
struct userp_bstr* userp_dec_bytes_zerocopy(userp_dec dec, size_t elem_size, int flags) {
	unimplemented("userp_dec_bytes_zerocopy");
	return NULL;
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

/*
### userp_dec_input_next_buffer

Make the next bstr_part (having at least one byte) the current part, returning true if
such a buffer part exists, and false otherwise.

*/

bool userp_dec_input_next_buffer(userp_dec_input *in) {
	assert(in != NULL);
	assert(in->str != NULL);
	if (in->str_part >= in->str->part_count)
		return false;
	for (size_t i= in->str_part + 1; i < in->str->part_count; i++) {
		if (in->str->parts[i].len) {
			assert(in->str->parts[i].len <= (SIZE_MAX >> 3));
			in->str_part= i;
			in->buf_lim= in->str->parts[i].data + in->str->parts[i].len;
			in->bits_left= in->str->parts[i].len << 3;
			return true;
		}
	}
	return false;
}

/*
### userp_dec_input_skip_bits

  if (userp_dec_input_skip_bits(input, count)) ...

Skip forward by N bits, possibly iterating into new buffer parts.

### userp_dec_input_skip_bytes

  if (userp_dec_input_skip_bytes(input, count)) ...

Like skip_bits, but aligned to whole bytes, and a count of bytes.
Any partial "current" byte is not included in the count.
This can skip a full size_t worth of bytes, not limited to 1/8 as a bit count.

*/

inline static bool userp_dec_input_skip_bits(userp_dec_input *in, size_t bit_count) {
	while (in->bits_left < bit_count) {
		bit_count -= in->bits_left;
		if (!userp_dec_input_next_buffer(in))
			return false;
	}
	in->bits_left -= bit_count;
	return true;
}

inline static bool userp_dec_input_skip_bytes(userp_dec_input *in, size_t byte_count) {
	while ((in->bits_left >> 3) < byte_count) {
		byte_count -= (in->bits_left >> 3);
		if (!userp_dec_input_next_buffer(in))
			return false;
	}
	in->bits_left= ((in->bits_left >> 3) - bit_count) << 3;
	return true;
}

/*
### userp_dec_input_align

  if (userp_dec_input_align(input, pow2_bits)) ...

Skip some number of bits or bytes to reach the next multiple of pow2_bits.

*/

inline static bool userp_dec_input_align(userp_dec_input *in, unsigned pow2) {
	assert(pow2 < sizeof(size_t)*8);
	if (pow2 <= 3) {
		// Aligning to a byte or less, no need to seek
		in->bits_left= (in->bits_left >> pow2) << pow2;
		return in->bits_left? true : userp_dec_input_next_buffer(in);
	}
	else {
		size_t mask= (1<<pow2)-1;
		size_t remainder;
		// Alignment requests up to the alignment of the buffer itself
		// can rely on manipulating the buffer pointers as integers.
		// However, the time to check this probably isn't worth the savings.
		//if (pow2 <= in->str->env->buffer_align) {
			// can use the pointer address for the alignment
			remainder= mask & ((((intptr_t)in->buf_lim) << 3) - in->bits_left);
		//} else {
			// else the alignment is greater than known by the pointer, and
			// need to inspect the overall offset from the start of the block.
			userp_bstr_part *part= in->str->parts + in->str_part;
			remainder= ((part->ofs << 3) + (part->len << 3) - in->bits_left) & mask;
		//}
		return !remainder? true : userp_dec_input_skip_bits(mask + 1 - remainder);
	}
}

/*
### userp_decode_bits

  err= userp_decode_bits(decoder, input, bit_count);

*/

userp_error_t
userp_decode_bits(userp_node_info_private *node, userp_dec_input *in, size_t bits) {
	assert(bits > 0);
	//if (bits < 64) { // signed storage, so can't be a full 64
	//	
	//}
	unimplemented("decode_bits");
}

/*
### userp_decode_vint

  err= userp_decode_vint(&dest_node, src_input, is_signed);

Decode a variable-length integer from the decoder input, and store it in the dest_node either as
a plain integer, or as a BigInt (sign, number of "limbs", and userp_bstr of those limbs)

Variable length integers are 1 byte followed either by 1-7 additional bytes, or an array of 8-byte
"limbs" of a BigInt.  For signed integers, the BigInt notation has a dedicated sign bit, but the
1-8 byte encodings use the low bit as the sign bit.

#### Unsigned:

    0b.......1              7-bit number
    0b....XXX0 0x.... (...) 4 + X*8 bit number (little-endian)
    0bSXXX0000              Sign + 3-bit limb count (8 bytes each). if X=0, read larger word
    0bS0000000 0x....       Sign, 16-bit limb count. if X=0, read larger word
    0bS0000000 0x0000 0x........  Sign, 32-bit limb count. If X=0, read larger word, etc
    (Sign must equal zero, or error)

#### Signed:

    0b......S1              6-bit number, Sign
    0b...SXXX0 0x.... (...) 3 + X*8 bit number, Sign
	(bigint encodings remain the same)

*/

userp_error_t
userp_decode_vint(userp_node_info_private *node, userp_dec_input *in, bool is_signed) {
	// switch to bytes for the moment
	size_t bytes_left= in->bits_left >> 3;
	uint64_t val;
	if (!bytes_left) {
		if (!dec_input_next_buffer(in))
			return USERP_EOVERRUN;
		bytes_left= in->bits_left >> 3;
	}
	val= buf_lim[-bytes_left--];
	if (val & 0xF) {
		if (val & 1) {
			node->pub.intval= val >> 1;
		} else {
			int n= (val & 0x0E) >> 1;
			int shift= 0;
			val >>= 4;
			while (n--) {
				if (!bytes_left) {
					if (!dec_input_next_buffer(in))
						return USERP_EOVERRUN;
					bytes_left= in->bits_left >> 3;
				}
				val |= ((uint64_t) buf_lim[-bytes_left--]) << shift;
				shift += 8;
			}
			node->pub.intval= val;
		}
		if (is_signed)
			node->pub.intval= (node->pub.intval & 1)? -(node->pub.intval >> 1) : (node->pub.intval >> 1);
		node->pub.flags= USERP_NODEFLAG_INT;
	}
	else {
		int sign= val >> 7;
		size_t limbs= (val >> 4) & 0x7;
		for (int n= 16; !limbs && n <= sizeof(size_t)*8; n <<= 1) {
			for (int shift= 0; shift < n: shift+= 8) {
				if (!bytes_left) {
					if (!dec_input_next_buffer(in))
						return USERP_EOVERRUN;
					bytes_left= in->bits_left >> 3;
				}
				limbs |= ((uint16_t) buf_lim[-bytes_left--]) << shift;
			}
		}
		if (!limbs || limbs > (SIZE_MAX >> 3))
			return USERP_EOVERFLOW;
		size_t cur_part= in->str_part;
		size_t byte_ofs= in->str.parts[in->str_part].len - bytes_left;
		if (!dec_input_skip(in, limbs << 3))
			return USERP_EOVERRUN;
		userp_bstr_set_substr(&node->pub.data, in->str, cur_part, byte_ofs, limbs << 3);
		node->pub.flags= USERP_NODEFLAG_BIGINT;
	}
	in->bits_left= bytes_left << 3;
	return 0;
}

/*
### userp_decode_vsize

  err= userp_decode_vsize(&size, src_input);

Decode a variable-length unsigned integer no larger than a size_t.  If the integer is larger, this
returns USERP_EOVERFLOW.  This is just like decode_vint but is always unsigned and only succeeds
if the value fits in a size_t.

*/

userp_error_t
userp_decode_vsize(size_t *out, userp_dec_input *in) {
	// Optimize the common case of sizes 12 bits or less.
	if (in->bits_left >= 23) {
		size_t bytes_left= in->bits_left >> 3;
		uin8_t *p= in->buf_lim - bytes_left;
		if (*p & 1) {
			*out= *p >> 1;
			in->bits_left= (bytes_left-1) << 3;
			return true;
		} else if ((sel & 0xF) == 1) {
			*out= userp_load_le16(p) >> 4;
			in->bits_left= (bytes_left-2) << 3;
			return true;
		}
	}
	node_info node;
	userp_error_t err= userp_decode_vint(&node, in);
	if (!err && node->pub.flags == USERP_NODEFLAG_INT && node->pub.intval <= SIZE_MAX) {
		*out= node->pub.intval;
		return true;
	}
	return false;
}

// Decode an unsigned variable-length size_t
// This macro uses 'goto' for errors, which the caller must handle.
// Caller must also have an 'err' variable.
#define DECODE_SIZE(dest) \
    do { \
		if (in->bits_left >= 8 && (in->buf_lim[-(in->bits_left >> 3)] & 1)) { \
			(dest)= in->buf_lim[-(in->bits_left >> 3)] >> 1; \
		else { \
			size_t tmp; \
			if ((err= userp_decode_vsize(&tmp, in))) \
				goto fail_decode_size; \
			(dest)= tmp; \
		} \
	} while (0)

// Decode an unsigned variable-length integer (must fit in size_t)
// Then resolve relative type reference, if relative
// This macro uses 'goto' for errors, which the caller must handle.
// Caller must also have an 'err' variable.
#define DECODE_TYPEREF(dest) \
    do { \
		size_t tmp; \
		DECODE_SIZE(tmp); \
		if (!(tmp & 1) && (tmp >> 1) <= scope->type_count) \
			(dest)= tmp >> 1; \
		else if (!((dest)= userp_scope_resolve_relative_typeref(scope, tmp))) \
			goto fail_typeref; \
	} while (0)

// Decode an unsigned variable-length integer (must fit in size_t)
// Then resolve relative symbol reference, if relative
// This macro uses 'goto' for errors, which the caller must handle.
// Caller must also have an 'err' variable.
#define DECODE_SYMREF(dest) \
    do { \
		size_t tmp; \
		DECODE_SIZE(tmp); \
		if (!(tmp & 1) && (tmp >> 1) <= scope->symbol_count) \
			(dest)= tmp >> 1; \
		else if (!((dest)= userp_scope_resolve_relative_symref(scope, tmp))) \
			goto fail_symref; \
	} while (0)

// Decode the next N bits into an unsigned int64, where 0 < N <= 64
// Returns false if and only if it would overrun the buffer.
bool userp_decode_bits(uint64_t *out, size_t bits, struct userp_bit_io *in) {
	uint64_t val;
	assert(bits > 0 && bits <= 64);
	// There might be 0-7 leftover bits from the previous byte, so reading more
	// than 57 bits has the potential to need to touch 9 bytes.  Below that requires
	// at most 8 bytes, and if the buffer is large enough, can be done in a single read.
	int remainder= in->bits_left & 7;
	if (bits <= 57 && in->bits_left >= 57) {
		val= !remainder? userp_load_le64(in->buf_lim - (in->bits_left >> 3))
			: userp_load_le64(in->buf_lim - (in->bits_left >> 3) - 1) >> (8 - remainder);
		in->bits_left -= bits;
	}
	else {
		// else just iteratively read bytes into the accumulator
		int shift= 0;
		if (remainder) {
			val= in->buf_end[-(in->bits_left>>3) - 1] >> (8 - remainder);
			shift= remainder;
			in->bits_left -= remainder;
		} else {
			val= 0;
		}
		// loop starts on whole-byte boundary, and bits_left will be a multiple of 8
		while (shift < bits) {
			if (!in->bits_left)
				if (!userp_dec_input_next_buffer(in))
					return false;
			val |= (uint64_t)in->buf_end[-in->bits_left>>3] << shift;
			in->bits_left -= 8;
			shift += 8;
		}
	}
	if (bits < 64)
		val= val << (64 - bits) >> (64 - bits); // clear out upper bits
	*out= val;
	return true;
}

// Decode the next N bits as twos-complement, where 0 < N <= 64
// Returns false if and only if it would overrun the buffer.
bool userp_decode_bits_twos(int64_t *out, size_t bits, struct userp_bit_io *in) {
	int64_t val;
	assert(bits > 0 && bits <= 64);
	// There might be 0-7 leftover bits from the previous byte, so reading more
	// than 57 bits has the potential to need to touch 9 bytes.  Below that requires
	// at most 8 bytes, and if the buffer is large enough, can be done in a single read.
	int remainder= in->bits_left & 7;
	if (bits <= 57 && in->bits_left >= 57) {
		val= !remainder? userp_load_le64(in->buf_lim - (in->bits_left >> 3))
			: userp_load_le64(in->buf_lim - (in->bits_left >> 3) - 1) >> (8 - remainder);
		in->bits_left -= bits;
	}
	else {
		// else just iteratively read bytes into the accumulator
		int shift= 0;
		if (remainder) {
			val= in->buf_end[-(in->bits_left>>3) - 1] >> (8 - remainder);
			shift= remainder;
			in->bits_left -= remainder;
		} else {
			val= 0;
		}
		// loop starts on whole-byte boundary, and bits_left will be a multiple of 8
		while (shift < bits) {
			if (!in->bits_left)
				if (!userp_dec_input_next_buffer(in))
					return false;
			val |= (uint64_t)in->buf_end[-in->bits_left>>3] << shift;
			in->bits_left -= 8;
			shift += 8;
		}
	}
	if (bits < 64)
		val= val << (64 - bits) >> (64 - bits); // clear out upper bits and sign extend
	*out= val;
	return true;
}

bool userp_dec_init_node(userp_dec dec, userp_node_info_private *node, userp_dec_input *in) {
	userp_error_t err;
	size_t sz;
	struct type_entry *type_entry;
	node->pub.value_type= node->pub.node_type;
	top:
	type_entry= userp_scope_get_type_entry(dec->scope, node->pub.value_type);
	assert(type_entry != NULL);
	switch (type_entry->typeclass) {
	case TYPE_CLASS_ANY:
	case TYPE_CLASS_TYPEREF:
		{
			userp_type t;
			DECODE_TYPEREF(t); // failures goto error cases below
			// If type=ANY, use the typeref to continue reading the value
			if (type_entry->typeclass == TYPE_CLASS_ANY) {
				node->pub.value_type= t;
				if (t) goto top;
				else goto fail_null_type;
			}
			// else if type=TYPEREF, give the type ref back to the user
			else {
				node->as_typeref= t;
				node->pub.flags= USERP_NODE_IS_TYPEREF;
				return true;
			}
		}
	case TYPE_CLASS_SYMREF:
		{
			userp_symbol s;
			DECODE_SYMREF(s); // failures goto error cases below
			node->as_symref= s;
			node->pub.flags= USERP_NODE_IS_SYMBOL;
			return true;
		}
	case TYPE_CLASS_INT_POW2ALIGN_U8:
		{
			size_t bytes_left= in->bits_left >> 3;
			if (bytes_left) {
				node->pub.intval= in->buf_lim[-bytes_left];
				in->bits_left= (bytes_left - 1) << 3;
				node->pub.flags= USERP_NODE_IS_ALIGNED_INT;
				return true;
			}
		}
		// fall through to the generic fixed-width integer implementation
		if (0) {
	case TYPE_CLASS_INT_POW2ALIGN:
			// The type class is not set to "INT_POW2_ALIGNED" unless the buffer alignment
			// requirement is at least as large as this integer type.  So, can use the
			// buffer pointer directly to determine alignment.
			intptr_t size= type_entry->as_int->size;
			intptr_t pos= (intptr_t)in->buf_lim - (in->bits_left >> 3);
			pos= (pos + size - 1) & ~(size-1);
			// Are there enough bytes left in the buffer?
			if (pos + size <= (intptr_t) in->buf_lim) {
				node->pub.data_start= (uint8_t*) pos;
				in->bits_left= ((intptr_t)in->buf_lim - pos - size) << 3;
				node->pub.flags= USERP_NODE_IS_ALIGNED_INT;
				return true;
			}
			// else not enough bytes, but could be collected from the next buffer(s) if any.
		}
		// fall through to the generic fixed-width integer implementation
	case TYPE_CLASS_INT:
		{
			// Unsigned can read 63 bits, and signed can read 64 bits.
			// Beyond that needs BigInts.
			if (type_entry->as_int->twos && type_entry->as_int->bits <= 64) {
				if (type_entry->as_int->align)
	case TYPE_CLASS_INT_ALIGNED_TWOS_AS_INT64:
					userp_dec_input_align(in, type_entry->as_int->align);
	case TYPE_CLASS_INT_TWOS_AS_INT64:
				if (!userp_decode_bits_twos(&node->pub.intval, type_entry->as_int->bits, in))
					goto fail_overrun;
			} else if (!type_entry->as_int->twos && type_entry->as_int->bits < 64) {
				if (type_entry->as_int->align)
	case TYPE_CLASS_INT_ALIGNED_BITS_AS_INT64:
					userp_dec_input_align(in, type_entry->as_int->align);
	case TYPE_CLASS_INT_BITS_AS_INT64:
				uint64_t dest;
				if (!userp_decode_bits(&dest, type_entry->as_int->bits, in))
					goto fail_overrun;
				node->pub.intval= dest;
			} else {
				// needs returned as a BigInt
				unimplemented("Decode BigInt bits");
			}
			return true;
		}
		// variable-width integers
	case TYPE_CLASS_VINT:
		{
			err= userp_decode_vint(node, in, false);
			if (0)
	case TYPE_CLASS_VINT_SIGNED:
				err= userp_decode_vint(node, in, true);
			if (err) goto fail_decode_vint;
			// TODO: apply transform, like scale and bias
			...
			return true;
		}
	}
	
	CATCH(fail_decode_size) {
		...
	}
	CATCH(fail_decode_typeref) {
		...
	}
	CATCH(fail_decode_symref) {
		...
	}
}

struct record_context {
	userp_type type;
	struct userp_type_record *rec;
	struct field_detail *field_detail;
	size_t field_detail_alloc;
};

/*
On a constrained memory system, the decoder doesn' want to have to track too many details.
For a record, ideally this could be as small as a pointer to the type and idx of the current field
and the total count of possible fields.  It would iterate through the "always" fields like normal,

*/

static bool userp_dec_rec_begin(userp_dec dec, struct record_context *ctx, userp_type rectype) {
	struct type_entry *type_entry= userp_scope_get_type_entry(dec->scope, rectype);
	assert(type_entry && type_entry->typeclass == TYPE_CLASS_RECORD);
	ctx->type= rectype;
	ctx->rec= (struct userp_type_record*) type_entry->typeobj;

	// If there are any dynamic fields (Often or Seldom placement or `other_field_type`) the
    // record begins with a selector to indicate which Often fields and how many Seldom fields.
	if (ctx->rec->often_field_count || ctx->rec->seldom_field_count || ctx->rev->other_field_type) {
		// If a selector is supplied, it is both the often-presence flags (lower bits)
		// and the other_field_count (upper bits)
		if (dec->in->has_selector) {
			ctx->often_field_presence= dec->in->selector;
			ctx->extra_field_count= (ctx->rec->often_field_count < sizeof(dec->in->selector)*8)
				? dec->in->selector >> ctx->rec->often_field_count
				: 0;
			dec->in->has_selector= false;
		}
		// Else the often-presence bits are the next N bits from here,
		// and the other_field_count (if any) is encoded after that.
		else {
			if (ctx->rec->often_field_count) {
				if (ctx->rec->often_field_count <= sizeof(ctx->often_field_presence)*8) {
					if (!(ctx->often_field_presence= userp_decode_bits(dec->in, ctx->rec->often_field_count))
						&& dec->in->failed)
						goto fail_presence_bits;
				}
				else {
					ctx->often_field_presence_vec= userp_dec_get_contiguous_bits(dec->in,
						&ctx->often_field_presence_shift);
				}
			}
			if (ctx->rec->other_field_type) {
				if (!(ctx->extra_field_count= userp_decode_vqty(dec->in))
					&& dec->in->failed)
					goto fail_field_count;
			}
			else if (ctx->rec->seldom_field_count) {
				if (!(ctx->extra_field_count= userp_decode_bits(dec->in, log2_sizet(ctx->rec->seldom_field_count)))
					&& dec->in->failed)
					goto fail_field_count;
			}
		}
		// If the extra_field_count is nonzero, read that many field indicators.
		// Field indicators are vqty if there are arbitrary "other" fields.
		if (ctx->extra_field_count) {
			size_n n= ctx->extra_field_count;
			if (!USERP_ALLOC_ARRAY(dec->env, &ctx->extra_fields, n))
				goto fail_alloc;
			if (ctx->rec->other_field_type) {
				if (userp_decode_vqty_sizevec(dec->in, ctx->extra_fields, n) < n)
					goto fail_field_list;
			}
			else {
				if (userp_decode_bits_sizevec(dec->in, ctx->extra_fields, n, log2_sizet(n), 0) < n)
					goto fail_field_list;
			}
			for (i= 0; i < n; i++)
				if (!userp_scope_verify_extra_field_ref(dec->scope, ctx->extra_fields[i]))
					goto fail_field_list;
		}
	}
	// Apply record alignment
	if (ctx->rec->align)
		if (!userp_decode_align(ctx->rec->align))
			goto fail_align;
	// Record the location of the start of the record's data
	ctx->data_start.bitpos= dec->in->bitpos;
	ctx->data_start.part= dec->in->part;
	// For each "Conditional" field with placement outside of the official static area,
	// check its condition and enlarge the static area if needed.
	...
	// Record is fully initialized here.
	// The total number of present fields has not been determined, but that can be lazily
	// calculated if needed.
	return true;
}

bool userp_dec_rec_seek_field(userp_dec dec, size_t field_id) {
	unimplemented("seek field");
	return false;
}

#ifdef UNIT_TEST

struct varqty_test {
	const char *data;
	size_t size;
	uint64_t expected;
};
static const struct varqty_test varqty_tests[]= {
	{ .expected= 0x00000000, .data= "\x00", .size= 1 },
	{ .expected= 0x00000001, .data= "\x02", .size= 1 },
	{ .expected= 0x0000007F, .data= "\xFE", .size= 1 },
	{ .expected= 0x00000080, .data= "\x01\x02", .size= 2 },
	{ .expected= 0x000000FF, .data= "\xFD\x03", .size= 2 },
	{ .expected= 0x00000100, .data= "\x01\x04", .size= 2 },
	{ .expected= 0x00003FFF, .data= "\xFD\xFF", .size= 2 },
	{ .expected= 0x00004000, .data= "\x03\x00\x02\x00", .size= 4 },
	{ .expected= 0x1FFFFFFF, .data= "\xFB\xFF\xFF\xFF", .size= 4 },
	{ .expected= 0x20000000, .data= "\x07\x00\x00\x00\x20", .size= 5 },
	{ .expected= 0xFFFFFFFF, .data= "\x07\xFF\xFF\xFF\xFF", .size= 5 },
	{ .expected= 0x01FFFFFFFFll, .data= "\x0F\xFF\xFF\xFF\xFF\x01\x00\x00\x00", .size= 9 },
	{ .expected= 0xFFFFFFFFFFFFFFFFll, .data= "\x0F\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", .size= 9 },
	{ .data= NULL, .size= 0 }
};

UNIT_TEST(decode_ints_varqty) {
	userp_env env= userp_new_env(NULL, userp_file_logger, stdout, 0);
	int i;
	struct userp_bstr_part parts[2];
	struct userp_bstr str= {
		.parts= parts, .part_count= 0, .env= env
	};
	struct userp_bit_io in= {
		.str= &str, .accum_bits= 0, .part= parts,
	};
	uint64_t value;
	const struct varqty_test *test= varqty_tests;

	while (test->data) {
		str.part_count= 1;
		str.parts[0].data= (uint8_t*) test->data;
		str.parts[0].len= test->size;
		str.parts[0].ofs= 0;
		in.part= str.parts;
		in.pos= in.part->data;
		in.lim= in.part->data + in.part->len;
		in.accum_bits= 0;

		printf("# \"");
		for (i= 0; i < test->size; i++)
			printf("\\x%02X", (int)(test->data[i]&0xFF));
		printf("\"\n");

		size_t got= userp_decode_vqty_u64vec(&value, 1, &in);
		if (got) {
			printf("actual=%08llX expected=%08llX\n", (long long)value, (long long)test->expected);
			if (in.pos < in.lim) printf("  leftover bytes: %d\n", (int)(in.lim - in.pos));
		} else {
			printf("failed to parse: ");
			userp_diag_print(&env->err, stdout);
			printf("\n");
		}
		// Try the "quick" version if it fits in size_t
		if (sizeof(size_t) <= sizeof(uint64_t) || !(test->expected >> (8*sizeof(size_t)))) {
			size_t s= 0;
			in.part= str.parts;
			in.pos= in.part->data;
			in.lim= in.part->data + in.part->len;
			
			if (!userp_decode_vqty_quick(&s, &in)) {
				printf("  quick: faild to parse: ");
				userp_diag_print(&env->err, stdout);
				printf("\n");
			}
			else if (s == test->expected) {
				printf("  quick: got expected value\n");
				if (in.pos < in.lim) printf("  leftover bytes: %d\n", (int)(in.lim - in.pos));
			}
			else {
				printf("  quick: wrong value: %lld\n", (long long) s);
			}
		}
		else {
			printf("  quick: larger than size_t\n");
		}
		
		// Now do it again with a buffer split at each possible position
		for (i= 0; i < test->size; i++) {
			str.part_count= 2;
			str.parts[0].len= i;
			str.parts[1].ofs= i;
			str.parts[1].data= str.parts[0].data + i;
			str.parts[1].len= test->size - i;
			in.part= str.parts;
			in.pos= in.part->data;
			in.lim= in.part->data + in.part->len;
			in.accum_bits= 0;
			got= userp_decode_vqty_u64vec(&value, 1, &in);
			if (got && value == test->expected) {
				printf("  works with split at %d\n", i);
				if (in.pos < in.lim) printf("  leftover bytes: %d\n", (int)(in.lim - in.pos));
			} else {
				printf("  failed with split at %d: ", i);
				userp_diag_print(&env->err, stdout);
				printf("\n");
			}
		}
		
		test++;
	}
	userp_drop_env(env);
}
/*OUTPUT
# "\\x00"
actual=00000000 expected=00000000
  quick: got expected value
  works with split at 0
# "\\x02"
actual=00000001 expected=00000001
  quick: got expected value
  works with split at 0
# "\\xFE"
actual=0000007F expected=0000007F
  quick: got expected value
  works with split at 0
# "\\x01\\x02"
actual=00000080 expected=00000080
  quick: got expected value
  works with split at 0
  works with split at 1
# "\\xFD\\x03"
actual=000000FF expected=000000FF
  quick: got expected value
  works with split at 0
  works with split at 1
# "\\x01\\x04"
actual=00000100 expected=00000100
  quick: got expected value
  works with split at 0
  works with split at 1
# "\\xFD\\xFF"
actual=00003FFF expected=00003FFF
  quick: got expected value
  works with split at 0
  works with split at 1
# "\\x03\\x00\\x02\\x00"
actual=00004000 expected=00004000
  quick: got expected value
(  works with split at \d\n)+
# "\\xFB\\xFF\\xFF\\xFF"
actual=1FFFFFFF expected=1FFFFFFF
  quick: got expected value
(  works with split at \d\n)+
# "\\x07\\x00\\x00\\x00\\x20"
actual=20000000 expected=20000000
  quick: got expected value
(  works with split at \d\n)+
# "\\x07\\xFF\\xFF\\xFF\\xFF"
actual=FFFFFFFF expected=FFFFFFFF
  quick: got expected value
(  works with split at \d\n)+
# "\\x0F\\xFF\\xFF\\xFF\\xFF\\x01\\x00\\x00\\x00"
actual=1FFFFFFFF expected=1FFFFFFFF
  quick: (got expected value|larger than size_t)
(  works with split at \d\n)+
# "\\x0F\\xFF\\xFF\\xFF\\xFF\\xFF\\xFF\\xFF\\xFF"
actual=FFFFFFFFFFFFFFFF expected=FFFFFFFFFFFFFFFF
  quick: (got expected value|larger than size_t)
(  works with split at \d\n)+
*/

#endif /* UNT_TEST */
