#ifndef USERPTINY_H
#define USERPTINY_H

typedef uint8_t userptiny_error_t;

#define USERPTINY_EOVERFLOW 1
#define USERPTINY_EOVERRUN  2

const char *userptiny_error_name(userptiny_error_t code);

struct userptiny_scope {
	struct userptiny_scope *parent;
	uint8_t *symbols;
	uint8_t sym_base, sym_count;
	uint16_t type_base;
	uint8_t type_count, type_alloc;
	uint8_t *type_table[];
};

struct userptiny_scope* userptiny_scope_init(char *obj_buf, uint16_t size, struct userptiny_scope *parent);

struct userptiny_enc {
	struct userptiny_scope *scope;
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
	struct userptiny_scope *scope;
	uint8_t *in;
	uint16_t in_pos;
	uint16_t in_len;
	uint8_t in_bitpos,
		state_pos,
		state_alloc,
		error;
	uint8_t state_stack[];
};

struct userptiny_dec* userptiny_dec_init(char *obj, uint16_t size, struct userptiny_scope *scope);
userptiny_error_t userptiny_dec_set_input(struct userptiny_dec *dec, uint8_t *in, uint16_t in_len);
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
