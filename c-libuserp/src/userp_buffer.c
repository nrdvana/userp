#include "local.h"
#include "userp_protected.h"

static bool buf_decode_long_le(userp_buffer_t *buf, long *value_p, int bits);
static bool buf_encode_long_le(userp_buffer_t *buf, long value, int bits);
static bool buf_encode_bytes_zc_le(userp_buffer_t *buf, char **bytes_p, size_t count);
static bool buf_decode_bytes_zc_le(userp_buffer_t *buf, char **bytes_p, size_t count);
static bool buf_seek(userp_buffer_t *buf, size_t bitpos);
static bool buf_append_buf(userp_buffer_t *buf, userp_buffer_t *buf2);

const struct userp_buffer_vtable
	buf_vt_le_std= {
		.encode_long= buf_encode_long_le,
		.decode_long= buf_decode_long_le,
		.encode_bytes_zerocopy= buf_encode_bytes_zc_le,
		.decode_bytes_zerocopy= buf_decode_bytes_zc_le,
		.seek= buf_seek,
		.append_buf= buf_append_buf
	};

static bool buf_init(userp_buffer_t *buf, userp_env_t *env, char *mem, size_t size, bool bigendian, int alignment) {
	intptr_t mask= (1<<alignment)-1;
	if (!env->run_with_scissors) {
		if (aignment > USERP_BITALIGN_MAX) {
			env->diag_code= USERP_EINVAL;
			env->diag_tpl= "Unsupported alignment requested: " DIAG_VAR_ALIGN;
			env->diag_align= alignment;
			return false;
		}
		bzero(buf, sizeof(userp_buffer_t));
	}
	if (mem) {
		if (((intptr_t)mem) & mask) {
			env->diag_code= USERP_EINVAL;
			env->diag_tpl= "Memory address " DIAG_VAR_BUFADDR " is not aligned to " DIAG_VAR_ALIGN;
			env->diag_buf= mem;
			env->diag_align= alignment;
			return false;
		}
		buf->buf_ptr_ofs= 0;
		buf->buf= mem;
		buf->mem_owner= 0;
	} else {
		if (!env->alloc(env->alloc_cb_data, (void**) &mem, size + mask, 0)) {
			env->diag_code= USERP_EALLOC;
			env->diag_len= size;
			env->diag_tpl= "Can't allocate " DIAG_VAR_LEN " byte buffer";
			return false;
		}
		buf->buf_ptr_ofs= ((intptr_t)mem) & mask;
		buf->buf= mem + env->buf_ptr_ofs;
		buf->mem_owner= 1;
	}
	buf->len= size;
	buf->env= env;
	buf->bigendian= bigendian;
	buf->align_pow2= alignment;
	buf->bitpos= 0;
	buf->vtable= ...;
	return true;
}

static bool buf_destroy(userp_buffer_t *buf) {
	void *mem;
	if (!buf->vtable) { // This indicates a double-free bug
		if (buf->env) {
			buf->env->diag_code= USERP_EASSERT;
			buf->env->diag_tpl= "Attempt to destroy userp buffer object twice";
		}
		return false;
	}
	if (buf->mem_owner) {
		mem= (void*)(buf->buf - buf->buf_ptr_ofs);
		if (!buf->env->alloc(buf->env->alloc_cb_data, &mem, 0, 0)) {
			buf->env->diag_code= USERP_EALLOC;
			buf->env->diag_tpl= "Can't free buffer";
			return false;
		}
		buf->buf= NULL;
		buf->mem_owner= 0;
	}
	buf->vtable= NULL;
	return true;
}

void memcpy_to_ints_from_buf(userp_int_t *dest, uint8_t *src, size_t bits, int shift) {
	userp_int_t val;
	while (bits > sizeof(userp_int_t)*8) {
		*dest++ = !shift? *((userp_int_t*)src)
			: ( *((userp_int_t*)src+1) << (8-shift) ) | (*src >> shift)
		bits -= sizeof(userp_int_t)*8;
	}
	while (bits > 8) {
		
	}
}
void memcpy_to_buf_from_ints() {
	
}

bool buf_decode_int_le(userp_buffer_t *buf, int bits, unsigned *value_p, int value_limbs) {
}
bool buf_encode_int_le(userp_buffer_t *buf, int bits, userp_int_t *value_p, int value_limbs) {
	/* TODO: on systems that require aligned word access, this whole function needs re-thought. */
	size_t pos, n, value, have;
	/* If value_limbs is -1, it means the value pointer points to a dynamic-length bigint
	 * whose length is indicated by element -1
	 */
	if (value_limbs < 0) value_limbs= value_p[-1];
	/* bits -1 means to encode as a variable-length integer */
	if (bits < 0) {
		/* Discard all bigint limbs that are not needed */
		while (value_limbs > 1 && !value_p[value_limbs-1]) --value_limbs;
		/* Variable-length ints always start on a byte boundary */
		pos= (buf->bitpos+7)>>3;
		/* Upper bound for number of bytes needed to hold the encoded value. */
		n= 6 + value_limbs * sizeof(userp_int_t) + (sizeof(userp_int_t) < 4? 3 : 0);
		/* Grow buffer to at least this length. Then no more length checks needed. */
		if (pos + n > buf->len && !buf_grow(buf, pos + n))
			return false;
		/* optimize encoding of small integers */
		if (value_limbs == 1) {
			value= *value_p;
			if (*value_p < 0x80) {
				*((uint8_t*)(buf->buf+pos))= (uint8_t) (value<<1);
				buf->bitpos= (pos+1)<<3;
				return true;
			} else if (*value_p < 0x4000) {
				*((uint16_t*)(buf->buf+pos))= (uint16_t) ((value<<2)+1);
				buf->bitpos= (pos+2)<<3;
				return true;
			} else if (*value_p < 0x20000000) {
				*((uint32_t*)(buf->buf+pos))= (uint32_t) ((value<<3)+3);
				buf->bitpos= (pos+4)<<3;
				return true;
			}
			#if sizeof(userp_int_t) >= 8
			else if (*value_p < 0x1000000000000000) {
				*((uint64_t*)(buf->buf+pos)= (uint64_t) ((value<<4)+7);
				buf->bitpos= (pos+8)<<3;
				return true;
			}
			#endif
		}
		/* length-of-length describes how many 4-byte limbs to encode in addition to the implied first two */
		n= (value_limbs * sizeof(userp_int_t) + 3) >> 2;
		n= n < 2? 0 : n - 2;
		/* any sane situation allows this number to fit in 12 bits */
		if (n < 0xFFF) {
			*((uint16_t*)(buf->buf+pos)= (uint16_t) ((n<<4) | 0xF);
			pos += 2;
		}
		/* but handle the less-sane ones up to 32 bits */
		else if (n < 0xFFFFFFFF) {
			*((uint16_t*)(buf->buf+pos)= (uint16_t) 0xFFFF;
			*((uint32_t*)(buf->buf+pos+2)= (uint32_t) n;
			pos += 6;
		}
		/* this is exceedingly unlikely in the year 2020.  But leave room for the future :-) */
		else {
			buf->env->diag_code= USERP_ELIMIT;
			buf->env->diag_tpl= "Refusing to encode ludicrously large integer";
		}
		/* Now just deliver the limbs of the integer, but rounded up to a multiple of 4 bytes */
		n= value_limbs * sizeof(userp_int_t);
		memcpy(buf->buf+pos, value_p, n);
		/* protocol assumes limbs of 4 bytes, but could theoretically compile lib with size=2 */
		#if sizeof(userp_int_t) < 4
		if (n & 3) {
			memcpy(buf->buf+pos+n, "\xFF\xFF\xFF", 4 - (n & 3));
			n= (n|3)+1;
		}
		#endif
		buf->bitpos= (pos+n)<<3;
	}
	/* When given a literal number of bits, this is basically just a memcpy with possible bit-shift */
	else {
		
		shift= buf->bitpos & 7;
		pos= buf->bitpos >> 3;
		/* ensure buffer space for the requested number of bits */
		n= (buf->bitpos + bits + 7)>>3;
		if (n > buf->len && !buf_grow(buf, n))
			return false;
		/* If no bit remainder, use the simpler byte-based code. */
		if (!shift) {
			switch ((bits+7) >> 3) {
			case 0: return true;
			case 1: *((uint8_t* )(buf->buf+pos))= (uint8_t) *value_p; buf->bitpos += 8; return true;
			case 2: *((uint16_t*)(buf->buf+pos))= (uint16_t) *value_p; buf->bitpos += 16; return true;
			case 4: *((uint32_t*)(buf->buf+pos))= (uint32_t) *value_p; buf->bitpos += 32; return true;
			#if sizeof(unsigned) >= 8
			case 8: *((uint64_t*)(buf->buf+pos))= (uint64_t) *value_p; buf->bitpos += 64; return true;
			#endif
			default:
				have= value_limbs * sizeof(unsigned);
				if ((bits >> 3) > have) {
					memcpy(buf->buf+pos, value_p, have);
					memset(buf->buf+pos+have, 0, (bits>>3) - have);
				} else {
					memcpy(buf->buf+pos, value_p, bits>>3);
				}
				but->bitpos += bits;
				return true;
			}
		} else {
			n= 0;
			prev= *((unsigned)(buf->buf+pos));
			while (bits > sizeof(unsigned)*8) {
				value= n < value_limbs? value_p[n] : 0;
				*((unsigned*)(buf->buf+pos))= (value_p[n] >> shift) | (prev & mask);
				prev= value_p[n];
				n++;
				bits -= sizeof(unsigned)*8;
			}
			value= n < value_limbs? value_p[n] : 0;
			if (bits > 32) {
				*((uint32_t*)(buf->buf+pos))= (value_p[n] >> shift)
				bits -= 32;
			}
			
		}
	}
}
