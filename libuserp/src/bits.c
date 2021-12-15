
struct userp_bit_stream {
	unsigned *pos, *lim;
	unsigned accum, bits;
	uint_least32 *selector, static_sel;
};

unsigned userp_input_read_u(userp_input *in) {
	if (!bits) {
		if (pos < lim) 
}

#define USERP_INPUT_READu8(in) \
	if (have_bits >= 8) { \
		bits -= 8;
	} else if (pos < lim) {
		accum= *pos++;
		bits += sizeof(unsigned)*8 - 8;
	} else if (!advance_buffer)
		goto eof;
	dest= (uint8_t) accum;
	accum >>= 8;

#define USERP_INPUT_READu16(in) \
	if (sizeof(unsigned) > 16 && have_bits >= 16) { \
		bits -= 16; \
	} else if (pos < lim) {
	    accum= *pos++
	    bits += sizeof(unsigned)*8 - 16;
	} else if (!advance_buffer)
	    goto eof;
	dest= (uint16_t) accum;
	accum >>= 16;

USERP_INPUT_READ_WORD(dest, destype, srctype, in)
	if (sizeof(desttype) > sizeof(srctype)) {
		bits= 0;
		if (pos + (sizeof(desttype)/sizeof(srctype)) <= lim) {
			dest= ((desttype) pos[0])
				| (((desttype) pos[1]) << 8*sizeof(srctype));
			if (sizeof(desttype) >= 4 *sizeof(srctype))
				dest |= (((desttype) pos[2]) << 8*2*sizeof(srctype))
					| (((desttype) pos[3]) << 8*3*sizeof(srctype));
			if (sizeof(desttype) >= 8 * sizeof(srctype))
				dest |= (((desttype) pos[4]) << 8*4*sizeof(srctype))
					| (((desttype) pos[5]) << 8*5*sizeof(srctype))
					| (((desttype) pos[6]) << 8*6*sizeof(srctype))
					| (((desttype) pos[7]) << 8*7*sizeof(srctype));
			pos += sizeof(esttype)/sizeof(srctype);
		} else {
			in->pos= pos;
			dest= read_bits_over_boundary(in, sizeof(desttype)*8);
			pos= in->pos;
			lim= in->lim;
		}
	}
	else if (sizeof(desttype) == sizeof(srctype)) {
		bits= 0;
		if (pos < lim)
			dest= *pos++;
		else {
			in->pos= pos;
			dest= read_bits_over_boundary(in, sizeof(desttype)*8);
			pos= in->pos;
			lim= in->lim;
		}
	}
	else { // if (sizeof(desttype) < sizeof(srctype)) {
		bits >>= 3+sizeof(desttype);
		if (bits) {
			dest= (desttype) accum;
			bits--;
			bits <<= 3+sizeof(desttype);
			accum >>= 8*sizeof(desttype);
		} else if (pos < lim) {
			dest= (accum= *pos++);
			bits= (sizeof(srctype) - sizeof(desttype)) * 8;
			accum >>= sizeof(desttype) * 8;
		} else {
			in->pos= pos;
			dest= read_bits_over_boundary(in, sizeof(desttype)*8);
			pos= in->pos;
			lim= in->lim;
		}
	}
}
