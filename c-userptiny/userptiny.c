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

uint16_t userptiny_dec_bits(struct userptiny_dec *dec, uint8_t bits) {
	// sanity check
	if (bits > 16) {
		dec->error= USERPTINY_EOVERFLOW;
		return 0;
	}
	// Bitpos is the number of bits of in[pos] which are already consumed.
	// The number of bytes needed could be up to 3, for the bitpos and 16 bits
	// after that.
	uint8_t bitpos= dec->in_bitpos;
	int bytes_needed= (bitpos + bits + 7) >> 3;
	// verify the buffer has that many
	if (dec->in_len - dec->in_pos < bytes_needed) {
		dec->error= USERPTINY_EOVERRUN;
		return 0;
	}
	// save the new value of bitpos, but hold the original in the local variable
	dec->in_bitpos= (bitpos + bits) & 7;
	// first byte, possibly partial
	uint16_t out= dec->in[dec->in_pos] >> bitpos;
	// did that satisfy it?
	if (bits <= 8 - bitpos) {
		if (!dec->in_bitpos) dec->in_pos++;
		return out & (((uint16_t)1 << bits)-1);
	}
	// else add bytes 2 and maybe 3
	out |= ((uint16_t) dec->in[++dec->in_pos]) << (8 - bitpos);
	if (bytes_needed > 2)
		out |= ((uint16_t) dec->in[++dec->in_pos]) << (16 - bitpos);
	else if (!dec->in_bitpos)
		++dec->in_pos; // if consumed up to an even multiple, advance the position
	// need a bitmask, unless it was a full 16 bits
	return bits == 16? out : (out & (((uint16_t)1 << bits)-1));
}

uint16_t userptiny_dec_vqty(struct userptiny_dec *dec) {
	// align up to the next whole byte
	if (dec->in_bitpos && dec->in_pos < dec->in_len) {
		++dec->in_pos;
		dec->in_bitpos= 0;
	}
	// need at least one available
	if (dec->in_pos >= dec->in_len) {
		dec->error= USERPTINY_EOVERRUN;
		return 0;
	}
	// This byte determines how many more will be read
	uint16_t out= dec->in[dec->in_pos++];
	if (!(out & 1))
		return out >> 1;
	if (dec->in_pos >= dec->in_len) {
		dec->error= USERPTINY_EOVERRUN;
		return 0;
	}
	out |= ((uint16_t) dec->in[dec->in_pos++]) << 8;
	if (!(out & 2))
		return out >> 2;
	if (!(out & 4)) {
		// two more bytes left to read, but can only hold 3 more bitss
		if (dec->in_len - dec->in_pos < 2) {
			dec->error= USERPTINY_EOVERRUN;
			return 0;
		}
		if ((dec->in[dec->in_pos] >> 3) || dec->in[dec->in_pos+1]) {
			dec->error= USERPTINY_EOVERFLOW;
			return 0;
		}
		out= (out >> 3) | ((uint16_t) dec->in[dec->in_pos]) << 13;
		dec->in_pos += 2;
		return out;
	}
	// could try checking if bigint holds less than 16 bits, but probably not worth it
	dec->error= USERPTINY_EOVERFLOW;
	return 0;
}

