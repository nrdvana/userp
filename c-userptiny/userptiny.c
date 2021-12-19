#include "config.h"
#include "userptiny.h"

struct userptiny_scope* userptiny_scope_init(char *obj_buf, uint16_t size, struct userptiny_scope *parent) {
	struct userptiny_scope *self;
	size_t ptr_mask= alignof(struct userptiny_scope)-1;
	size_t align_ofs= (size_t)((intptr_t)obj_buf & ptr_mask);
	if (!obj_buf || align_ofs + sizeof(struct userptiny_scope) > size)
		return NULL;
	size -= align_ofs + sizeof(struct userptiny_scope);
	self= (struct userptiny_scope*) (obj_buf + align_ofs);
	self->parent= parent;
	self->symbols= NULL;
	self->sym_base= 0;
	self->sym_count= 0;
	self->type_base= 0;
	self->type_count= 0;
	self->type_alloc= size / sizeof(self->type_table[0]);
	return self;
}

struct userptiny_dec* userptiny_dec_init(char *obj_buf, uint16_t size, struct userptiny_scope *scope) {
	struct userptiny_dec *self;
	size_t ptr_mask= alignof(struct userptiny_dec)-1;
	size_t align_ofs= (size_t)((intptr_t)obj_buf & ptr_mask);
	if (!obj_buf || align_ofs + sizeof(struct userptiny_dec) > size)
		return NULL;
	size -= align_ofs + sizeof(struct userptiny_dec);
	self= (struct userptiny_dec*) (obj_buf + align_ofs);
	self->scope= scope;
	self->in= NULL;
	self->in_pos= 0;
	self->in_len= 0;
	self->in_bitpos= 0;
	self->state_pos= 0;
	self->state_alloc= size / sizeof(self->state_stack[0]);
	self->error= 0;
	return self;
}

uint16_t userptiny_dec_set_input(struct userptiny_dec *dec, uint8_t *in, uint16_t in_len) {
	dec->in= in;
	dec->in_pos= 0;
	dec->in_len= in_len;
	dec->in_bitpos= 0;
	dec->state_pos= 0;
}

userptiny_error_t userptiny_dec_raw_bits(uint16_t *out, uint8_t *buf_lim, uint16_t *bits_left, uint8_t bits) {
	// sanity check
	if (bits > 16)
		return USERPTINY_EOVERFLOW;
	if (bits > *bits_left)
		return USERPTINY_OVERRUN;
	uint16_t ofs= *bits_left >> 3;
	uint8_t partial= *bits_left & 7;
	if (bits > partial) {
		uint8_t upper= bits - partial;
		uint16_t val= (upper <= 8)? buf_lim[-ofs] & (0xFF >> (8-upper))
			: buf_lim[-ofs] | ((uint16_t)(buf_lim[1-ofs] & (0xFF >> (16-upper))) << 8);
		if (partial)
			val= (val << partial) | (buf_lim[-1-ofs] >> (8-partial));
		*out= val;
	}
	else {
		*out= (buf_lim[-1-ofs] & (0xFF >> (partial-bits))) >> (8 - bits);
	}
	*bits_left -= bits;
	return 0;
}

userptiny_error_t userptiny_dec_raw_vqty(uint16_t *out, uint8_t *buf_lim, uint16_t *bits_left) {
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
		*out= (sel >> 3) | ((uint16_t)buf_lim[-bytes_left+1] << 5) | ((uint16_t)buf_lim[-bytes_left+2] << 13);
	}
	else {
		// just assume value in bigint is larger than int16
		return USERPTINY_EOVERFLOW;
	}
	*bits_left= bytes_left << 3;
	return 0;
}

