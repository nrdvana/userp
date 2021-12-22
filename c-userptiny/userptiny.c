#include "config.h"
#include "userptiny.h"

const char *userptiny_error_text(userptiny_error_t code) {
	switch (code) {
	case USERPTINY_EOVERFLOW:     return "integer overflow";
	case USERPTINY_ELIMIT:        return "size limit exceeded";
	case USERPTINY_EUNSUPPORTED:  return "unsupported feature";
	case USERPTINY_EOVERRUN:      return "buffer overrun";
	case USERPTINY_ETYPEREF:      return "invalid typeref";
	case USERPTINY_ESYMREF:       return "invalid symref";
	case USERPTINY_EALLOC:        return "insufficient memory";
	case USERPTINY_EDOINGITWRONG: return "invalid api call";
	default: return "unknown code";
	}
}

userptiny_error_t userptiny_scope_init(
	struct userptiny_scope *scope,
	const struct userptiny_scope *parent,
	uint8_t *state, size_t state_size
) {
	scope->parent= parent;

	scope->state= state;
	scope->state_alloc= state_size;

	scope->symbols= NULL;
	scope->sym_count= parent? parent->sym_count : 0;
	scope->sym_lookup_count= 0;

	scope->types= NULL;
	scope->type_count= parent? parent->type_count : 0;
	scope->type_cache= NULL;
	scope->type_cache_count= 0;
	return 0;
}

struct userptiny_type *
userptiny_scope_get_type(const struct userptiny_scope *scope, uint16_t type_id) {
	// Type ID is 1-based, with 0 meaning NULL
	if (type_id > scope->type_count)
		return NULL;
	// If type id is within the parent scope, switch to that one
	while (scope->parent && type_id <= scope->parent->type_count)
		scope= scope->parent;
	uint16_t parent_count= scope->parent? scope->parent->type_count : 0;
	// are all types decoded?
	if (scope->type_cache_count == scope->type_count - parent_count) {
		return &scope->types[ type_id - parent_count - 1 ];
	}
	else {
		// not enough state space to decode all types, so pick a cache slot and decode the type
		// TODO
		return NULL;
	}
}

const char* const userptiny_v1_scope_symtable[]= {
	"any",
	"symref",
	"typeref",
	"integer",
};

const struct userptiny_type userptiny_v1_scope_types[]= {
	{ .typeclass= USERPTINY_TYPECLASS_BUILTIN, .name= 1, .flags= 0, .builtin= { .subtype= USERPTINY_V1_TYPE_ANY } },
	{ .typeclass= USERPTINY_TYPECLASS_BUILTIN, .name= 2, .flags= 0, .builtin= { .subtype= USERPTINY_V1_TYPE_SYMREF } },
	{ .typeclass= USERPTINY_TYPECLASS_BUILTIN, .name= 3, .flags= 0, .builtin= { .subtype= USERPTINY_V1_TYPE_TYPEREF } },
	{ .typeclass= USERPTINY_TYPECLASS_INTEGER, .name= 4, .flags= 0 },
};

const struct userptiny_scope userptiny_v1_scope= {
	.parent= NULL,
	.state= NULL,
	.state_alloc= 0,
	.symbols= userptiny_v1_scope_symtable,
	.sym_count= 0,
	.sym_lookup_count= 0,
	.types= (struct userptiny_type*) userptiny_v1_scope_types,
	.type_count= sizeof(userptiny_v1_scope_types)/sizeof(*userptiny_v1_scope_types),
	.type_cache= NULL,
	.type_cache_count= sizeof(userptiny_v1_scope_types)/sizeof(*userptiny_v1_scope_types),
};

userptiny_error_t
userptiny_dec_init(struct userptiny_dec *dec,
                   const struct userptiny_scope *scope,
                   uint8_t *state_storage, size_t state_storage_size
) {
	// allocate as many state structs as possible from the givenn bytes.  Needs to be
	// correctly aligned, though.
	size_t ofs= (intptr_t)state_storage & (alignof(struct userptiny_dec_state)-1);
	// and need at least one item
	if (state_storage_size < ofs + sizeof(struct userptiny_dec_state))
		return USERPTINY_EALLOC;
	dec->scope= scope;
	dec->input_end= NULL;
	dec->bits_left= 0;
	dec->state_stack= (struct userptiny_dec_state*)(state_storage + ofs);
	dec->state_alloc= (state_storage_size-ofs) / sizeof(struct userptiny_dec_state);
	dec->state_pos= 0;
	dec->error= 0;
	return 0;
}

userptiny_error_t
userptiny_dec_set_input(struct userptiny_dec *dec,
                        uint16_t root_type,
                        uint8_t *in, uint16_t in_len
) {
	// look up root type
	struct userptiny_type *type= userptiny_scope_get_type(dec->scope, root_type);
	if (!type)
		return USERPTINY_ETYPEREF;
	// iterating bits, so need 3 spare bits in a uint16_t
	if (in_len & 0xE000)
		return USERPTINY_ELIMIT;
	dec->input_end= in + in_len;
	dec->bits_left= in_len << 3;
	dec->state_pos= 0;
	dec->state_stack[0].type= type;
	dec->state_stack[0].elem_idx= 0;
	return 0;
}

userptiny_error_t
userptiny_dec_int(struct userptiny_dec *dec, uint16_t *out) {
	struct userptiny_dec_state *st= &dec->state_stack[dec->state_pos];
	switch(st->type->typeclass) {
	case USERPTINY_TYPECLASS_INTEGER:
		if (st->elem_idx)
			return USERPTINY_EDOINGITWRONG;
		st->elem_idx++;
		return userptiny_decode_vqty(out, dec->input_end, &dec->bits_left);
	default:
		return USERPTINY_EDOINGITWRONG;
	}
}

userptiny_error_t userptiny_decode_bits(uint16_t *out, uint8_t *buf_lim, uint16_t *bits_left, uint8_t bits) {
	// sanity check
	if (bits > 16)
		return USERPTINY_EOVERFLOW;
	if (bits > *bits_left)
		return USERPTINY_EOVERRUN;
	// ofs is the first byte following the partial byte (if any)
	uint16_t ofs= *bits_left >> 3;
	// number of upper bits remaining in previous byte
	uint8_t prev_unused= *bits_left & 7;
	if (bits > prev_unused) {
		uint8_t new_bits= bits - prev_unused;
		uint16_t val= (new_bits <= 8)? buf_lim[-ofs] & (0xFF >> (8-new_bits))
			: buf_lim[-ofs] | ((uint16_t)(buf_lim[1-ofs] & (0xFF >> (16-new_bits))) << 8);
		if (prev_unused)
			val= (val << prev_unused) | (buf_lim[-1-ofs] >> (8-prev_unused));
		*out= val;
	}
	else if (bits > 0) {
		*out= (buf_lim[-1-ofs] & (0xFF >> (prev_unused-bits))) >> (8-prev_unused);
	}
	else
		*out= 0;
	*bits_left -= bits;
	return 0;
}

userptiny_error_t userptiny_decode_vqty(uint16_t *out, uint8_t *buf_lim, uint16_t *bits_left) {
	// switch to bytes for the moment
	uint16_t bytes_left= *bits_left >> 3;
	// need at least one byte
	if (!bytes_left)
		return USERPTINY_EOVERRUN;
	// This byte determines how many more will be read
	uint8_t sel= buf_lim[-bytes_left--];
	if (!(sel & 1)) {
		*out= sel >> 1;
	}
	else if (bytes_left && !(sel & 2)) {
		if (!bytes_left)
			return USERPTINY_EOVERRUN;
		*out= (sel >> 2) | ((uint16_t)buf_lim[-bytes_left--] << 6);
	}
	else if (bytes_left >= 3 && !(sel & 4)) {
		if (bytes_left < 3)
			return USERPTINY_EOVERRUN;
		if ((buf_lim[-bytes_left+1] >> 3) || buf_lim[-bytes_left+2])
			return USERPTINY_EOVERFLOW;
		*out= (sel >> 3) | ((uint16_t)buf_lim[-bytes_left] << 5) | ((uint16_t)buf_lim[-bytes_left+1] << 13);
		bytes_left -= 3;
	}
	else {
		// just assume value in bigint is larger than int16
		return USERPTINY_EOVERFLOW;
	}
	*bits_left= bytes_left << 3;
	return 0;
}

