#include "local.h"
#include "userp_private.h"

typedef struct userp_dec_frame *frame;

// static void frame_init(userp_dec dec, frame f, userp_type t);
// static void frame_destroy(userp_dec dec, frame f);

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
returns USERP_EOVERFLOW.

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
#define DECODE_SYMREF(dest) \
    do { \
		size_t tmp; \
		DECODE_SIZE(tmp); \
		if (!(tmp & 1) && (tmp >> 1) <= scope->symbol_count) \
			(dest)= tmp >> 1; \
		else if (!((dest)= userp_scope_resolve_relative_symref(scope, tmp))) \
			goto fail_typeref; \
	} while (0)

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
#define DEC_OP_END
#define DEC_OP_SRC_ALIGN
#define DEC_OP_DST_ALIGN
#define DEC_OP_DST_SEEK
#define DEC_OP_LOAD_VARQTY
#define DEC_OP_LOAD_8
#define DEC_OP_LOAD_16
#define DEC_OP_LOAD_16_BE
#define DEC_OP_LOAD_32
#define DEC_OP_LOAD_32_BE
#define DEC_OP_LOAD_64
#define DEC_OP_LOAD_64_BE
#define DEC_OP_LOAD_BITS
#define DEC_OP_TRANSFORM
#define DEC_OP_STOR_8
#define DEC_OP_STOR_16
#define DEC_OP_STOR_16_BE
#define DEC_OP_STOR_32
#define DEC_OP_STOR_32_BE
#define DEC_OP_STOR_64
#define DEC_OP_STOR_64_BE



#define DEC_OP_
#define DEC_OP_DST_ENDIAN
#define DEC_OP_CP_VARQTY
#define DEC_OP_CP_BITS_LE_LE
#define DEC_OP_CP_BITS_BE_LE
#define DEC_OP_CP_BITS_LE_BE
#define DEC_OP_CP_BITS_BE_BE
#define DEC_OP_CP_BYTES_LE_LE
#define DEC_OP_CP_BYTES_BE_LE
#define DEC_OP_CP_BYTES_LE_BE
#define DEC_OP_CP_BYTES_BE_BE


struct decode_dest {
	size_t dest_ofs;
	size_t dest_size;
	size_t in_bits;
	size_t in_endian: 1,
		dest_endian: 1,
		is_signed: 1,
		align: 16;
};

#define DEC_INT_ST_VARQTY       0
#define DEC_INT_ST_BITCOPY      4
#define DEC_INT_ST_BITCOPY_SWAP 6
#define DEC_INT_ST_COPY         8
#define DEC_INT_ST_COPY_SWAP   10
size_t userp_decode_ints(void *out, struct userp_bit_io *in, uint8_t *script, uint64_t *scriptvalues) {
	unsigned
		state= 0,                   // used for lightweight "calls" to the next-buffer code, and then return to the algorithm
		accum_bits= in->accum_bits, // If bit-packing in effect, how many bits remain from the previous byte
		i;
	uint8_t
		*in_pos= in->pos,
		*in_lim= in->lim;
		*dest_pos,
		*dest_lim;
	size_t
		in_part_pos= in->part_pos,
		out_pos= 0,
		seek, align;
	uint_least32_t
        accum= in->accum;
	uint64_t
		in_bits,
		in_bytes,
		out_size, // handle up to 64-bit counts of bits
		ofs, mask;

	while (out_pos < n_out) {
		in_bits= out[out_pos].in_bits;
		// 0 means variable-length quantity
		if (in_bits == 0) { // variable-length quantity
			state= DEC_INT_ST_VARQTY;
			accum_bits= 0; // varqty is alwas byte-aligned
			seek= 0;
		}
		// else it's a whole number of bits, and might be aligned
		else {
			align= out[out_pos].align;
			if (accum_bits && (align < 3)) {
				if (align == 1) {
					accum >>= (accum_bits & 1);
					accum_bits -= (accum_bits & 1);
				}
				else if (align == 2) {
					accum >>= (accum_bits & 3);
					accum_bits -= (accum_bits & 3);
				}
				seek= 0;
				state= DEC_INT_ST_BITCOPY;
			}
			else {
				if (align > 3) {
					ofs= in->str->parts[in_part_pos].ofs
						+ (in_pos - in->str->parts[in_part_pos].data);
					mask= 1 << (align-3);
					seek= mask - (ofs & (mask-1));
				} else
					seek= 0;
				accum_bits= 0;
				state= (in_bits & 7)? DEC_INT_ST_BITCOPY : DEC_INT_ST_COPY;
			}
		}
		// Loop repeats each time a buffer part is consumed.
		while (1) {
			// Start of loop always guarantees that *pos is readable
			while (in_pos + seek >= in_lim) {
				if (++in_part_pos >= in->str->part_count)
					goto fail_overrun;
				seek -= (in_lim - in_pos);
				in_pos= in->str->parts[in_part_pos].data;
				in_lim= in->str->parts[in_part_pos].len + in_pos;
			}
			in_pos += seek;
			// Next, go back to whatever was interrupted by running out of buffer
			switch (state) {
			case DEC_INT_ST_VARQTY:
				// State 0 is a variable quantity, need to read one byte to know what to do next.
				accum= *in_pos++;
				if (!(accum & 1)) {
					accum >>= 1;
					accum_bits= 7;
					in_bits= 7;
				} else if (!(accum & 2)) {
					accum >>= 2;
					accum_bits= 6;
					in_bits= 14;
				} else if (!(accum & 4)) {
					accum >>= 3;
					accum_bits= 5;
					in_bits= 29;
				} else if (!(accum & 8)) {
					accum >>= 4;
					accum_bits= 4;
					in_bits= 60;
				} else {
					// read one additional byte to find the length of the int
					if (in_pos >= in_lim) { state= 1+DEC_INT_ST_VARQTY; continue; }
					case 1+DEC_INT_ST_VARQTY:
					in_bytes= (accum >> 4) | (((uint64_t) *in_pos++) << 4);
					if (in_bytes == 0xFFF) { // max val means try again with twice as many bytes
						in_bytes= 0;
						for (i= 0; i < 4; i++) {
							if (in_pos >= in_lim) { state= 2+DEC_INT_ST_VARQTY; continue; }
							case 2+DEC_INT_ST_VARQTY:
							in_bytes |= ((uint64_t) *in_pos++) << (i * 8);
						}
						if (in_bytes == 0xFFFFFFFF) { // and again
							in_bytes= 0;
							for (i= 0; i < 8; i++) {
								if (in_pos >= in_lim) { state= 3+DEC_INT_ST_VARQTY; continue; }
								case 3+DEC_INT_ST_VARQTY:
								in_bytes |= ((uint64_t) *in_pos++) << (i * 8);
							}
							// Protocol theoretically continues, but no practical reason to implement beyond here
							if (!(in_bits+1))
								goto fail_bigint_sizesize;
							// make sure limbs-to-bits conversion doesn't overflow
							if (in_bytes >> (64-7))
								goto fail_overflow;
						}
					}
					in_bytes <<= 2; // the number read was 4-byte quantities
					state= DEC_INT_ST_COPY;
					continue;
				}
			case DEC_INT_ST_BITCOPY:
				if (out[out_pos].endian == LSB_FIRST && out[out_pos].
				in_bytes= ((in_bits - accum_bits)+7) >> 3;
				while (in_bytes
				if (out[ == LSB_FIRST) {
					
				// of the native endianness
				if (out[out_pos].dest_size < 8) {
				}
				unimplemented("bit copy");
			case DEC_INT_ST_COPY:
				unimplemented("byte copy");
			}
			break;
		}
		out_pos++;
	}

	CATCH(fail_overflow) {
		userp_diag_set(&in->str->env->err, USERP_ELIMIT,
			"Decoded value would exceed implementation limits");
	}
	CATCH(fail_bigint_sizesize) {
		userp_diag_set(&in->str->env->err, USERP_ELIMIT,
			"Refusing to decode bigint length stored in >64-bits");
	}
	CATCH(fail_overrun) {
		if (out[out_pos].in_bits)
			userp_diag_setf(&in->str->env->err, USERP_EOVERRUN,
				"Ran out of buffer while decoding " USERP_DIAG_SIZE "-bit integer",
				(size_t) out[out_pos].in_bits);
		else
			userp_diag_set(&in->str->env->err, USERP_EOVERRUN,
				"Ran out of buffer while decoding variable-length integer");
	}

	return out_pos;
}
*/

bool userp_decode_bits(void *out, size_t bits, struct userp_bit_io *in) {
	unimplemented("decode bits");
	return false;
}

size_t userp_decode_vqty(void* out, size_t sizeof_out, struct userp_bit_io *in) {
	size_t
		i, n, limbs, out_used;
	struct userp_bstr_part
		*in_part= in->part,
		*bigint_part;
	uint8_t
		*in_pos= in->pos,
		*in_lim= in->lim,
		*bigint_pos;
	uint64_t
		accum;
	assert(out != NULL);
	assert(sizeof_out >= 4 && (sizeof_out & 3) == 0);
	#define MAYBE_ADVANCE_BUFFER \
		while (in_pos >= in_lim) { \
			if (++in_part - in->str->parts >= in->str->part_count) \
				goto fail_overrun; \
			in_pos= in_part->data; \
			in_lim= in_part->data + in_part->len; \
		}
	MAYBE_ADVANCE_BUFFER
	accum= *in_pos++;
	if (!(accum & 1)) {
		accum >>= 1;
	} else if (!(accum & 2)) {
		accum >>= 2;
		MAYBE_ADVANCE_BUFFER
		accum |= ((uint64_t) *in_pos++) << 6;
	} else if (!(accum & 4)) {
		accum >>= 3;
		for (i= 5; i < (8*3); i+= 8) {
			MAYBE_ADVANCE_BUFFER
			accum |= ((uint64_t) *in_pos++) << i;
		}
	} else {
		// Entering "bigint" territory
		limbs= 1+(accum >> 3);
		if (limbs == 0x1F) {
			// BigInt: read the length of the int
			int bits;
			for (bits= 16; bits <= 64; bits <<= 1) {
				accum= 0;
				for (i= 0; i < bits; i+= 8) {
					MAYBE_ADVANCE_BUFFER
					accum |= ((uint64_t) *in_pos++) << bits;
				}
				// If it is under the max value for this many bits, we're done
				if (( ((accum >> 1)+1) >> (bits-1) ) == 0)
					break;
			}
			if (bits > 64 || (accum > SIZE_MAX))
				goto fail_bigint_sizesize;
			limbs= accum;
		}
		// If the bytes can fit into the accumulator, read them now.
		if (limbs <= 2) {
			accum= 0;
			for (i= 0; i < (limbs<<5); i+= 8) {
				MAYBE_ADVANCE_BUFFER
				accum |= ((uint64_t) *in_pos++) << i;
			}
			if ((accum >> 32) && sizeof_out >= 8)
				goto accum64;
			// else fall through to same path as 32-bit accumulator
		}
		else goto process_bigint;
	}
	// 32-bit in accumulator, output_size guaranted to be >= 4
	if (!(sizeof_out&7)) { // multiple of 8 means 64-bit handling
		accum64: {
			*((uint64_t*) out)= accum;
			out_used= 8;
		}
	}
	else {
		*((uint32_t*) out)= accum;
		out_used= 4;
	}
	CATCH(process_bigint) {
		// else treat it as a bigint buffer.
		// Start copying into the buffer and if we run out of space, see if maybe the high bytes
		// of the bigint are all zero, in which case it fit afterall.
		bigint_part= in_part;
		bigint_pos= in_pos;
		size_t out_used= MIN(sizeof_out, limbs << 2);
		// If sizeof_out is a multiple of 8, make sure output is also
		if (!(sizeof_out & 7) && (out_used & 7))
			out_used += 4;
		if (ENDIAN == LSB_FIRST) {
			uint8_t *dest= (uint8_t*) out;
			for (i= out_used; i > 0;) {
				MAYBE_ADVANCE_BUFFER
				n= MIN(in_lim - in_pos, i);
				memcpy(dest, in_pos, n);
				in_pos += n;
				dest += n;
				i -= n;
			}
		}
		else {
			uint8_t *dest= ((uint8_t*) out) + out_used;
			for (i= out_used; i > 0;) {
				MAYBE_ADVANCE_BUFFER
				n= MIN(in_lim - in_pos, i);
				for (uint8_t *tmp_lim= in_pos + n; in_pos < tmp_lim;)
					*--dest = *in_pos++;
				i -= n;
			}
			assert(dest == out);
		}
		if ((limbs << 2) > out_used) {
			// check the rest of the bytes for nonzero values.
			for (i= (limbs<<2) - out_used; i > 0;) {
				MAYBE_ADVANCE_BUFFER
				n= MIN(in_lim - in_pos, i);
				for (uint8_t *tmp_lim= in_pos + n; in_pos < tmp_lim; in_pos++) {
					if (*in_pos) {
						// found a nonzero byte, so the bigint didn't fit in *out,
						// so roll back and return the length of the bigint.
						in->pos= bigint_pos;
						in->lim= bigint_part->data + bigint_part->len;
						in->part= bigint_part;
						goto fail_overflow;
					}
				}
				i -= n;
			}
		}
		// This concludes the BigInt logic.
	}
	in->pos= in_pos;
	in->lim= in_lim;
	in->part= in_part;
	return out_used;
	
	CATCH(fail_bigint_sizesize) {
		userp_diag_set(&in->str->env->err, USERP_ELIMIT,
			"Refusing to decode length-of-int stored in >64-bits");
	}
	CATCH(fail_overflow) {
		// set this error code for the convenience of the caller,
		// most of which will treat this as a failure.
		userp_diag_set(&in->str->env->err, USERP_ELIMIT,
			"Decoded value would exceed implementation limits");
		// instead of returning 0, return the number of bytes the call
		// would need to have supplied.  This is guaranteed to be larger
		// than what they gave us.
		assert((limbs << 2) > sizeof_out);
		return (limbs << 2);
	}
	CATCH(fail_overrun) {
		userp_diag_set(&in->str->env->err, USERP_EOVERRUN,
			"Ran out of buffer while decoding variable-length integer");
	}
	return 0;
}

size_t userp_decode_vqty_u64vec(uint64_t *out, size_t count, struct userp_bit_io *in) {
	assert(out != NULL);
	uint8_t
		*in_pos= in->pos,
		*in_lim= in->lim;
	size_t i;
	for (i= 0; i < count; i++) {
		// Optimize for common case, of entire value in single buffer and 9 bytes or less
		// Switch to more iterative implementation if not the common case.
		if (in_lim - in_pos >= 9) {
			uint64_t val= *in_pos;
			if (!(val & 1)) {       // one byte holding 7 bits
				out[i]= val >> 1;
				in_pos++;
				continue;
			}
			else if (!(val & 2)) {  // two bytes holding 14 bits
				out[i]= userp_load_le16(in_pos) >> 2;
				in_pos += 2;
				continue;
			}
			else if (!(val & 4)) {  // 4 bytes holding 29 bits
				out[i]= userp_load_le32(in_pos) >> 3;
				in_pos += 4;
				continue;
			}
			else if ((val >> 3) <= 1) {
				if (val >> 3) {     // BigInt of two 32-bit limbs
					out[i]= userp_load_le64(++in_pos);
					in_pos+= 8;
				} else {            // BigInt of one 32-bit limb
					out[i]= userp_load_le32(++in_pos);
					in_pos+= 4;
				}
				continue;
			}
		}
		// for anything complicated, go to the full implementation, which takes a buffer size
		// and either fills it or replaces the argument with a pointer to the input.
		if (userp_decode_vqty(out+i, sizeof(*out), in) != sizeof(*out))
			break;
		in_pos= in->pos;
		in_lim= in->lim;
	}
	in->pos= in_pos;
	in->lim= in_lim;
	return i;
}

size_t userp_decode_vqty_u32vec(uint32_t *out, size_t count, struct userp_bit_io *in) {
	assert(out != NULL);
	uint8_t
		*in_pos= in->pos,
		*in_lim= in->lim;
	size_t i;
	for (i= 0; i < count; i++) {
		// Optimize for common case, of entire value in single buffer and 4 bytes or less
		// Switch to more iterative implementation if not the common case.
		if (in_lim - in_pos >= 5) {
			uint32_t val= *in_pos;
			if (!(val & 1)) {       // one byte holding 7 bits
				out[i]= val >> 1;
				in_pos++;
				continue;
			}
			else if (!(val & 2)) {  // 2 bytes holding 14 bits
				out[i]= userp_load_le16(in_pos) >> 2;
				in_pos += 2;
				continue;
			}
			else if (!(val & 4)) {  // 3 bytes holding 29 bits
				out[i]= userp_load_le32(in_pos) >> 3;
				in_pos += 4;
				continue;
			}
			else if ((val >> 3) == 0) { // bigint of one 32-bit limb
				out[i]= userp_load_le32(in_pos);
				in_pos+= 4;
				continue;
			}
		}
		// for anything complicated, go to the full implementation, which takes a buffer size
		// and either fills it or replaces the argument with a pointer to the input.
		if (userp_decode_vqty(out+i, sizeof(*out), in) != sizeof(*out))
			break;
		in_pos= in->pos;
		in_lim= in->lim;
	}
	in->pos= in_pos;
	return i;
}

static inline bool userp_decode_bits_u8(unsigned *out, struct userp_bit_io *in) {
	if (in->accum_bits == 0) {
		if (in->pos < in->lim) {
			*out= *in->pos++;
			return true;
		}
	} else if (in->accum_bits < 8) {
		if (in->pos < in->lim) {
			in->accum |= ((unsigned) *in->pos++) << in->accum_bits;
			*out= in->accum & 0xFF;
			in->accum >>= 8;
			return true;
		}
	} else {
		*out= in->accum & 0xFF;
		in->accum >>= 8;
		in->accum_bits -= 8;
		return true;
	}
	return userp_decode_bits(out, 8, in);
}

struct record_context {
	userp_type type;
	struct userp_type_record *rec;
	struct field_detail *field_detail;
	size_t field_detail_alloc;
};
	
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
