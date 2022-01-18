#include "config.h"
#include "userptiny.h"

static inline bool userptiny_is_bzero(uint8_t *buf, size_t n) {
	uint8_t *pos= buf + n;
	while (pos != buf)
		if (*--pos)
			return false;
	return true;
}

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

userptiny_error_t
userptiny_scope_init(
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

/* Protocol Version 1 Default Scope */

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

/* Decoder API */

userptiny_error_t
userptiny_dec_init(
	struct userptiny_dec *dec,
	const struct userptiny_scope *scope,
	uint8_t *state_storage,
	size_t state_storage_size
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
userptiny_dec_reset(
	struct userptiny_dec *dec,
	uint16_t root_type,
	uint8_t *in,
	uint16_t in_len
) {
	// iterating bits, so need 3 spare bits in a uint16_t
	if (in_len & 0xE000)
		return USERPTINY_ELIMIT;
	dec->input_end= in + in_len;
	dec->bits_left= in_len << 3;
	dec->state_pos= 0;
	dec->node.type= root_type;
	return userptiny_decode_node(dec, type);
}

userptiny_error_t
userptiny_decode_node(struct userptiny_dec *dec) {
	userptiny_error_t err;
	uint32_t u32;
	int32 s32;
	struct userptiny_type *type= userptiny_scope_get_type(dec->scope, dec->node.type);
	if (!type) return USERPTINY_ETYPEREF;
	dec->node.flags= 0;

	switch(type->typeclass) {
	/*
	This implementation decodes all non-bigint integers as int32_t for the
	user's convenience and code simplicity.  As such, these are the possible
	scenarios:
	1) A variable unsigned integer (which fits in 29 bits else is written as a BigInt)
	2) A variable signed integer (in 28 bits, else is sign+magnitude BigInt)
	3) A constant bit-size unsigned integer which must be <= 31 bits
	4) A constant bit-size signed integer (in 2's complement) up to 32 bits,
		and must be sign-extended if smaller than 32 bits.
	
	Any of these may be combined with a
	1) scale (multiplier)
	2) bias (offset)
	*/
	case USERPTINY_TYPECLASS_VINT_UNSIGNED:
	case USERPTINY_TYPECLASS_VINT_SIGNED:
		{ /* decode variable length int */
			uint16_t bytes_left= dec->bits_left >> 3;
			// need at least one byte
			if (!bytes_left)
				return USERPTINY_EOVERRUN;
			// This byte is either a small positive int, or determines how mmuch larger the int is
			uint8_t sel= dec->input_end[-bytes_left];
			// Bit 0 indicates something longer than 1 byte
			if (!(sel & 1)) {
				dec->node.as_int32= sel >> 1;
				dec->node.flags= USERPTINY_NODEFLAG_INT32;
				bytes_left -= 1;
			}
			else if (!(sel & 2)) {
				int sz= 2 + ((sel >> 2) & 1);
				// read a uint16, or uint32?
				if (bytes_left < sz)
					return USERPTINY_EOVERRUN;
				if (type->typeclass == USERPTINY_TYPECLASS_VINT_SIGNED) {
					dec->node.as_int32= sz == 2
						? ((int16_t)userptiny_load_le16(dec->input_end - bytes_left) >> 3)
						: ((int32_t)userptiny_load_le32(dec->input_end - bytes_left) >> 3);
				} else {
					dec->node.as_int32= sz == 2
						? (int32_t)(userptiny_load_le16(dec->input_end - bytes_left) >> 3)
						: (int32_t)(userptiny_load_le32(dec->input_end - bytes_left) >> 3);
				}
				dec->node.flags= USERPTINY_NODEFLAG_INT32;
				bytes_left -= sz;
			}
			// Else enter BigInt territory.  It's some number of uint32 limbs with a separate sign bit.
			else {
				bool negative= (type->typeclass == USERPTINY_TYPECLASS_VINT_UNSIGNED)? 0
					: (sel >> 2) & 1;
				uint16_t limbs= sel >> 3;
				// 0 means read the size from a word double the size
				if (!limbs) {
					if (bytes_left < 2)
						return USERPTINY_EOVERRUN;
					limbs= userptiny_load_le16(dec->input_end - bytes_left);
					bytes_left -= 2;
					// don't bother reading 32-bit word for the size, just give up
					if (!limbs)
						return USERPTINY_EUNSUPPORTED;
					if (limbs > 0x1FFF)
						return USERPTINY_EOVERFLOW;
				}
				if (bytes_left < (limbs << 2))
					return USERPTINY_EOVERFLOW;
				// The BigInt could possibly fit in 31bit, making the caller's life easier.
				// The sign bit still comes from the selector though, not twos complement.
				if ((limbs == 1 || is_bzero(dec->input_end - bytes_left + 4, (limbs-1)<<2))
					&& (s32= (int32_t)userptiny_load_le32(dec->input_end - bytes_left)) >= 0
				) {
					dec->node.as_int32= negative? -s32 : s32;
					dec->node.flags= USERPTINY_NODEFLAG_INT32;
				}
				else {
					dec->node.as_bigint.is_negative= negative;
					dec->node.as_bigint.limb_count= limbs;
					dec->node.as_bigint.limbs= dec->input_end - bytes_left;
					dec->node.flags= USERPTINY_NODEFLAG_BIGINT;
				}
				bytes_left -= limbs << 2;
			}
			*bits_left= bytes_left << 3;
		}
		if (0) {
	case USERPTINY_TYPECLASS_INT_BITS:
			err= userptiny_decode_bits_u32(&u32, dec->input_end, &dec->bits_left, type->integer.bits);
			if (err) return err;
			dec->node.as_int32= u32;
			dec->node.flags= USERPTINY_NODEFLAG_INT32;
		}
		if (0) {
	case USERPTINY_TYPECLASS_INT_TWOS:
			err= userptiny_decode_bits_s32(&s32, dec->input_end, &dec->bits_left, type->integer.bits);
			if (err) return err;
			dec->node.as_int32= u32;
			dec->node.flags= USERPTINY_NODEFLAG_INT32;
		}
		// TODO: scale and offset
		if (type->flags & (USERPTINY_TYPEFLAG_INT_BASE|USERPTINY_TYPEFLAG_INT_SCALE)) {
			return USERPTINY_EUNSUPPORTED;
		}
		break;
	}
	default:
		return USERPTINY_EUNSUPPORTED;
	}
}

userptiny_error_t
userptiny_decode_bits_u16(uint16_t *out, uint8_t *buf_lim, uint16_t *bits_left, uint8_t bits) {
	// sanity check
	assert(bits > 0 && bits <= 16);
	if (bits > *bits_left)
		return USERPTINY_EOVERRUN;
	// ofs is the first byte following the partial byte (if any)
	uint16_t ofs= *bits_left >> 3;
	// number of upper bits remaining in previous byte
	uint8_t prev_unused= *bits_left & 7;
	uint16_t val= !prev_unused? (
			(bits <= 8)? buf_lim[-ofs]
			: userptiny_load_le16(buf_lim-ofs)
		)
		: (bits <= prev_unused)? buf_lim[-1-ofs] >> (8-prev_unused)
		: (bits - prev_unused <= 8)? userptiny_load_le16(buf_lim-ofs-1) >> (8-prev_unused)
		: (userptiny_load_le16(buf_lim-ofs-1) >> (8-prev_unused)) | ((uint16_t)buf_lim[-ofs+1] << (8 + prev_unused));
	if (bits < 16)
		val &= (1<<bits)-1;
	*out= val;
	*bits_left -= bits;
	return 0;
}

userptiny_error_t
userptiny_decode_bits_s16(int16_t *out, uint8_t *buf_lim, uint16_t *bits_left, uint8_t bits) {
	userptiny_error_t err;
	uint16_t tmp;
	if ((err= userptiny_decode_bits_u16(&tmp, buf_lim, bits_left, bits)))
		return err;
	*out= ((int16_t)(tmp << (16 - bits))) >> (16-bits);
	return 0;
}

userptiny_error_t
userptiny_skip_vint(size_t how_many, uint8_t *buf_lim, uint16_t *bits_left) {
	// switch to bytes for the moment
	uint16_t bytes_left= *bits_left >> 3;
	while (how_many--) {
		// need at least one byte
		if (!bytes_left)
			return USERPTINY_EOVERRUN;
		// This byte determines how many more will be read
		uint8_t sel= buf_lim[-bytes_left--];
		if (sel & 1) {
			int16_t need_bytes= !(sel & 2)? 1
				: !(sel & 4)? 3
				: (int16_t)(int8_t)(sel & 0xF8);
			if (!need_bytes) {
				if (bytes_left < 2)
					return USERPTINY_EOVERRUN;
				need_bytes= (int16_t) userptiny_load_le16(buf_lim-bytes_left);
				bytes_left -= 2;
				if (!need_bytes)
					return USERPTINY_EOVERRUN;
				need_bytes &= 0xFFF8;
			}
			if (bytes_left < need_bytes)
				return USERPTINY_EOVERRUN;
			bytes_left -= need_bytes;
		}
	}
	*bits_left= bytes_left << 3;
	return 0;
}

userptiny_error_t
userptiny_decode_vint_u16(uint16_t *out, uint8_t *buf_lim, uint16_t *bits_left) {
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
	else if (!(sel & 2)) {
		if (!bytes_left)
			return USERPTINY_EOVERRUN;
		*out= (sel >> 2) | ((uint16_t)buf_lim[-bytes_left--] << 6);
	}
	else if (!(sel & 4)) {
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

/*  Alternate option for variable-length integers using a simple loop

userptiny_error_t
userptiny_decode_vint {
	// switch to bytes for the moment
	uint16_t bytes_left= *bits_left >> 3;
	uint32_t accum= 0;
	unsigned shift= 0;
	unsigned byte;
	while (1) {
		// need at least one byte
		if (!bytes_left)
			return USERPTINY_EOVERRUN;
		byte= buf_lim[-bytes_left--];
		// last byte?
		if (byte & 1)
			break;
		accum |= (uint32_t)(byte >> 1) << shift;
		shift += 7;
		if (shift > 32)
			return USERP_EOVERFLOW;
	}
	// next bit is 'bigint' flag
	is_bigint= (byte & 2);
	// If signed, next bit is sign
	if (is_signed) {
		is_negative= (byte & 4);
		byte >>= 3;
	} else {
		byte >>= 2;
	}
	if (shift > 24 && (byte >> (32 - shift)))
		return USERPTINY_EOVERFLOW;
	accum |= (uint32_t)byte << shift;
	if (is_bigint) {
		if (bytes_left < accum)
			return USERPTINY_EOVERFLOW;
	}
}
*/
