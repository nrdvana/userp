#ifndef USERPTINY_H
#define USERPTINY_H

/*
Error codes:

  EOVERFLOW     - The data contains a value that exceeds the limits of this implementation.
  EOVERRUN      - The data is invalid, describing a structure that exceeds the end of the buffer.
  EUNSUPPORTED  - The data uses a feature not implemented by this code.
  ELIMIT        - The data exceeds a constraint requested by the user of the library.
  ETYPEREF      - The data makes an invalid reference to a Userp Type
  ESYMREF       - The data makes an invalid reference to a Userp Symbol
  EALLOC        - The library does not have enough temporary storage to perform an operation
  EDOINGITWRONG - Your program made an invalid request to the library

*/

typedef uint8_t userptiny_error_t;

#define USERPTINY_EOVERFLOW     1
#define USERPTINY_EOVERRUN      2
#define USERPTINY_EUNSUPPORTED  3
#define USERPTINY_ELIMIT        4
#define USERPTINY_ETYPEREF      5
#define USERPTINY_ESYMREF       6
#define USERPTINY_EALLOC        7
#define USERPTINY_EDOINGITWRONG 8

/* Return a short constant string describing the error code */
const char *userptiny_error_text(userptiny_error_t code);

/* Userp Type Structure

This struct describes a userp type.  It contains just enough fields for the
implementation to be able to decode the supported features of each type.
All type-specific fields are stored in a union, and you must inspect the
'typeclass' and 'flags' before using them.

*/

#define USERPTINY_TYPECLASS_BUILTIN  1
#define USERPTINY_TYPECLASS_INTEGER  2
#define USERPTINY_TYPECLASS_CHOICE   3
#define USERPTINY_TYPECLASS_ARRAY    4
#define USERPTINY_TYPECLASS_RECORD   5

#define USERPTINY_TYPEFLAG_INT_SIGNED 1
#define USERPTINY_TYPEFLAG_INT_BASE   2
#define USERPTINY_TYPEFLAG_INT_SCALE  4

struct userptiny_type {
	uint16_t
		typeclass: 3,
		flags: 13;
	uint16_t name;
	union {
		struct userptiny_type_builtin {
			uint8_t subtype;
		} builtin;
		struct userptiny_type_int {
			uint16_t base;
			uint8_t scale;
			uint8_t bits;
		} integer;
		struct userptiny_type_array {
			uint8_t *dims;
			uint16_t dim_type;
			uint8_t dim_count;
		} array;
		struct userptiny_type_record {
			uint16_t other_type;
			uint16_t *fields;
			uint8_t field_count,
				always_count,
				often_count;
		} record;
	};
};

/* Userp Scope structure

A scope defines a collection of symbols and types, and allows multiple blocks
to share the same type definitions.  This "tiny" implementation requires that
the buffer the scope was decoded from continue to exist for the lifespan of
the Scope.  It also cannot necessarily inflate all types or symbols into
their own structs, and may need to seek around re-decoding the original data
depending on which types are being decoded from the current block.

*/

struct userptiny_scope {
	const struct userptiny_scope
		*parent;
	const char * const
		* symbols;
	struct userptiny_type
		*types;
	uint8_t
		*type_cache,
		*state;
	uint16_t
		state_alloc,
		sym_count,
		type_count;
	uint8_t
		sym_lookup_count,
		type_cache_count;
};

/* Initialize a scope by parsing a symbol and type table */
userptiny_error_t
userptiny_scope_init(struct userptiny_scope *scope,
                     const struct userptiny_scope *parent,
                     uint8_t *state_storage, size_t state_storage_size,
                     uint8_t *data, uint16_t data_size);

/* Find a numbered symbol and return a pointer to its string */
const char *
userptiny_scope_get_symbol(const struct userptiny_scope *scope, uint16_t symbol);

/* Find a numbered type and return a pointer to its struct.
The struct is only valid until the next call to userptiny_get_type,
unless the scope and all its parents are fully inflated.
*/
struct userptiny_type *
userptiny_scope_get_type(const struct userptiny_scope *scope, uint16_t type_id);

/* Userp Version-1 standard scope

Version 1 of the protocol contains a collection of common types that are
defined in the default scope.  Scopes do not have to inherit from the default
scope, but it is very convenient to do so, and the default scope can be fully
stored in static memory.

  - ANY
    The conceptual choice of all other defined types
  - SYMBOL
    A reference to a symbolic constant, previously declared in the symbol table
  - TYPEREF
    A reference to a type, previously declared in the type table
  - INTEGER
    The infinite collection of negative and positive integers, stored as a
    variable-length format that extends into a BigInt pattern.

*/

#define USERPTINY_V1_TYPE_ANY     1
#define USERPTINY_V1_TYPE_SYMBOL  2
#define USERPTINY_V1_TYPE_TYPEREF 3
#define USERPTINY_V1_TYPE_INTEGER 4

extern const struct userptiny_scope userptiny_v1_scope;

/* Userp encoder

*/

struct userptiny_enc {
	const struct userptiny_scope *scope;
	uint8_t *out;
	uint16_t out_pos;
	uint16_t out_len;
	uint8_t in_bitpos,
		state_pos,
		state_alloc,
		error;
	uint8_t state_stack[];
};

/* Userp Node Info

This struct describes the value of the "current" point within the data block.
The decoder functions all unpack to this same struct, and then the user of the
library can inspect the node without knowing in advance what type was about to
be decoded.

The struct is heavily "unioned" and fields should only be used after inspecting
the flags.

*/

#define USERPTINY_NODEFLAG_INT      0x0001
#define USERPTINY_NODEFLAG_SIGNED   0x0002
#define USERPTINY_NODEFLAG_BIGINT   0x0004
#define USERPTINY_NODEFLAG_SYM      0x0008
#define USERPTINY_NODEFLAG_TYPE     0x0010
#define USERPTINY_NODEFLAG_FLOAT    0x0020
#define USERPTINY_NODEFLAG_RATIONAL 0x0040
#define USERPTINY_NODEFLAG_ARRAY    0x0080
#define USERPTINY_NODEFLAG_RECORD   0x0100
struct userptiny_node_info {
	union {
		struct {
			union {
				int32_t  as_int32;
				struct {
					uint64_t *limbs;
					uint16_t limb_count:16;
					bool is_negative;
				} as_bigint;
			};
			uint16_t as_symbol;
		};
		float    as_float;
		double   as_double;
		uint16_t as_typeref;
		struct {
			uint8_t *elems;
			uint16_t elem_count;
			uint16_t elem_type;
			uint16_t elem_bitsize;
		} as_array;
		struct {
			uint8_t *struct_ptr;
			uint16_t struct_bitsize;
			uint16_t field_count;
		} as_record;
	};
	uint16_t flags;
	uint16_t type;
};

/* Userp Decoder

The decoder operates on one block at a time, stepping or seeking through the
elements of the data and loading each into the 'node' field.  In order to
descend into records and arrays, it needs to have room on the "state stack",
which must be provided by the caller during initialization.

*/

struct userptiny_dec_state {
	struct userptiny_type
		*type;
	uint16_t elem_idx;
};

struct userptiny_dec {
	struct userptiny_node_info node;
	const struct userptiny_scope
		*scope;
	uint8_t *input_end;
	uint16_t bits_left;
	uint8_t
		state_pos,
		state_alloc,
		error;
	struct userptiny_dec_state
		*state_stack;
};

/* Initialize a decoder struct, and provide the state storage needed by the app */
userptiny_error_t
userptiny_dec_init(struct userptiny_dec *dec,
                   const struct userptiny_scope *scope,
                   uint8_t *state_storage, size_t state_storage_size);

/* Begin decoding a block, with the root node loaded into dec->node_info */
userptiny_error_t userptiny_dec_reset(struct userptiny_dec *dec, uint16_t root_type, uint8_t *in, uint16_t in_len);

/* Descend into an array or record node, to iterate its contents. */
userptiny_error_t userptiny_dec_enter(struct userptiny_dec *dec);
/* Return to a parent array or record, pointing at the same node as before the 'enter' call. */
userptiny_error_t userptiny_dec_exit(struct userptiny_dec *dec);
/* Read the next element in the sequence, or arrive at a final state for the current iteration. */
userptiny_error_t userptiny_dec_next(struct userptiny_dec *dec);
/* Seek to an element of the currently iterating array */
userptiny_error_t userptiny_dec_seek_elem(struct userptiny_dec *dec, uint16_t idx);
/* Seek to a field of the currently iterating record, by field index according to the type definition */
userptiny_error_t userptiny_dec_seek_field_idx(struct userptiny_dec *dec, uint16_t idx);
/* Seek to a field of the currently itreating record, by field name (symbol ID) */
userptiny_error_t userptiny_dec_seek_field_sym(struct userptiny_dec *dec, uint16_t sym_id);

userptiny_error_t userptiny_decode_bits_u16(uint16_t *out, uint8_t *buf_lim, uint16_t *bits_left, uint8_t bits);
userptiny_error_t userptiny_decode_bits_s16( int16_t *out, uint8_t *buf_lim, uint16_t *bits_left, uint8_t bits);
userptiny_error_t userptiny_decode_vint_u16(uint16_t *out, uint8_t *buf_lim, uint16_t *bits_left);
userptiny_error_t userptiny_decode_vint_s16( int16_t *out, uint8_t *buf_lim, uint16_t *bits_left);
//userptiny_error_t userptiny_decode_int(uint16_t *out, struct userptiny_scope *scope, uint16_t type, uint8_t *buf_lim, uint16_t *bits_left);
//
//userptiny_error_t userptiny_decode_int(uint16_t *out, struct userptiny_scope *scope, uint16_t type, uint8_t *buf_lim, uint16_t *bits_left) {
//	uint8_t *type_spec= userptiny_scope_find_type(scope, type);
//	if ((*type_spec & USERPTINY_TYPEDEF_CLASS_MASK) == USERPTINY_TYPEDEF_CLASS_INTEGER) {
//		// Is it a fixed quantity of bits?
//		if (*type_spec & USERPTINY_TYPEDEF_INT_BITS_PRESENCE_BIT) {
//			type_spec
//		}
//	}
//}

#if ENDIAN == LSB_FIRST && READ_UNALIGNED
  #define userptiny_load_le16(p) ( *((uint16_t*) p) )
#else
static inline uint16_t userptiny_load_le16(uint8_t *p) {
	return (((uint16_t) ((uint8_t*) p)[1]) << 8) | ((uint16_t) ((uint8_t*) p)[0]);
}
#endif

#endif
