#ifndef USERPTINY_H
#define USERPTINY_H

typedef uint8_t userptiny_error_t;

#define USERPTINY_EOVERFLOW     1
#define USERPTINY_EOVERRUN      2
#define USERPTINY_EUNSUPPORTED  3
#define USERPTINY_ELIMIT        4
#define USERPTINY_ETYPEREF      5
#define USERPTINY_ESYMREF       6
#define USERPTINY_EALLOC        7
#define USERPTINY_EDOINGITWRONG 8

const char *userptiny_error_text(userptiny_error_t code);

#define USERPTINY_TYPECLASS_BUILTIN  1
#define USERPTINY_TYPECLASS_INTEGER  2
#define USERPTINY_TYPECLASS_CHOICE   3
#define USERPTINY_TYPECLASS_ARRAY    4
#define USERPTINY_TYPECLASS_RECORD   5

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
			uint16_t min, max;
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

#define USERPTINY_V1_TYPE_ANY     1
#define USERPTINY_V1_TYPE_SYMREF  2
#define USERPTINY_V1_TYPE_TYPEREF 3
#define USERPTINY_V1_TYPE_INTEGER 4

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

userptiny_error_t
userptiny_scope_init(struct userptiny_scope *scope,
                     const struct userptiny_scope *parent,
                     uint8_t *state_storage, size_t state_storage_size);

userptiny_error_t
userptiny_scope_parse(struct userptiny_scope *scope,
                      uint8_t *data, uint16_t data_size);

struct userptiny_type *
userptiny_scope_get_type(const struct userptiny_scope *scope, uint16_t type_id);

extern const struct userptiny_scope userptiny_v1_scope;

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
				uint32_t as_uint32;
				int32_t  as_int32;
				struct {
					uint64_t *limbs;
					uint16_t limb_count;
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

userptiny_error_t
userptiny_dec_init(struct userptiny_dec *dec,
                   const struct userptiny_scope *scope,
                   uint8_t *state_storage, size_t state_storage_size);

userptiny_error_t userptiny_dec_reset(struct userptiny_dec *dec, uint16_t root_type, uint8_t *in, uint16_t in_len);
userptiny_error_t userptiny_dec_begin(struct userptiny_dec *dec);
userptiny_error_t userptiny_dec_seek_elem(struct userptiny_dec *dec, uint16_t idx);
userptiny_error_t userptiny_dec_seek_field_idx(struct userptiny_dec *dec, uint16_t idx);
userptiny_error_t userptiny_dec_seek_field_sym(struct userptiny_dec *dec, uint16_t sym_id);
userptiny_error_t userptiny_dec_next(struct userptiny_dec *dec);
userptiny_error_t userptiny_dec_end(struct userptiny_dec *dec);

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
