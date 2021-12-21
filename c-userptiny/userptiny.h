#ifndef USERPTINY_H
#define USERPTINY_H

typedef uint8_t userptiny_error_t;

#define USERPTINY_EOVERFLOW    1
#define USERPTINY_EOVERRUN     2
#define USERPTINY_EUNSUPPORTED 3
#define USERPTINY_ELIMIT       4

const char *userptiny_error_name(userptiny_error_t code);

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

struct userptiny_dec {
	const struct userptiny_scope *scope;
	uint8_t *input_end;
	uint16_t bits_left;
	uint8_t
		state_pos,
		state_alloc,
		error;
	uint8_t *state_stack;
};

userptiny_error_t
userptiny_dec_init(struct userptiny_dec *dec,
                   const struct userptiny_scope *scope,
                   uint8_t *state_storage, size_t state_storage_size);

userptiny_error_t userptiny_dec_set_input(struct userptiny_dec *dec, uint16_t root_type, uint8_t *in, uint16_t in_len);
userptiny_error_t userptiny_dec_int(struct userptiny_dec *dec, uint16_t *out);

userptiny_error_t userptiny_decode_bits(uint16_t *out, uint8_t *buf_lim, uint16_t *bits_left, uint8_t bits);
userptiny_error_t userptiny_decode_vqty(uint16_t *out, uint8_t *buf_lim, uint16_t *bits_left);
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

#endif