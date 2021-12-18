/*


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

USERP_INPUT_READ_ALIGNED_WORD(dest, destype, srctype, in)
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

*/

#define LIBWORD userp_word_t
#define IOWORD userp_ioword_t

bool userp_bitdec_advance_pos(struct userp_bitdec *in, size_t step) {
	struct userp_bstr_part *part, *part_lim;
	if (step < in->lim - in->pos) {
		in->pos += step;
		return true;
	}
	else {
		step -= in->lim - in->pos;
		part= in->part,
		part_lim= in->str->parts + in->str->part_count;
		while (part_lim - part > 1) {
			part++;
			assert( 0 == (part->len & ((1 << LOG2_IOWORD_BITS)-1)) );
			size_t n_words= part->len >> LOG2_IOWORD_BITS;
			if (step < n_words) {
				in->part= part;
				in->pos= step + (IOWORD*) part->data;
				in->lim= in->pos + n_words - step;
				return true;
			}
			step -= n_words;
		}
		userp_diag_set(&in->str->env->err, USERP_EOVERRUN, "Unexpected end of input");
		in->fail= true;
		return false;
	}
}

bool userp_bitdec_align(struct userp_bitdec *in, unsigned align) {
	assert(in->bitpos <= sizeof(IOWORD)*8);
	assert(in->bitpos == 0 || in->pos != in->lim);
	size_t bitpos= in->bitpos;
	bitpos += (1 << align)-1;
	size_t step= bitpos >> LOG2_IOWORD_BITS;
	if (step) {
		if (in->lim - in->pos > step)
			in->pos += step;
		else if (!userp_input_advance_pos(in, step))
			return false;
		in->bitpos= 0;
	}
	else
		in->bitpos= (bitpos >> align) << align;
	return true;
}

LIBWORD userp_bitdec_bits(struct userp_bitdec *in, unsigned bits) {
	size_t bitpos= in->bitpos;
	struct userp_bstr_part *part= in->part;
	IOWORD *pos= in->pos, *lim= in->lim;
	unsigned outpos= 0;
	if (bits > sizeof(LIBWORD)*8) {
		userp_diag_setf(&in->str->env->err, USERP_ELIMIT,
			"Number exceeds library limit of " USERP_DIAG_SIZE " bits",
			(size_t) (sizeof(LIBWORD)*8)
		);
	}
	// First, take bits from any partially used input word
	// (if bitpos is nonzero, it guarantees pos is readable)
	LIBWORD out;
	if (!bitpos)
		out= 0;
	else {
		assert(lim != pos);
		assert(bitpos < sizeof(IOWORD)*8);
		out= pos[0] >> bitpos;
		outpos= sizeof(IOWORD)*8 - bitpos;
		if (bits <= outpos) { // read is satisfied
			bitpos += bits;
			if (bitpos == sizeof(IOWORD)*8) {
				in->bitpos= 0;
				in->pos++;
			} else
				in->bitpos= bitpos;
			out &= (1 << bits)-1;
			return out;
		}
		// else bitpos += outpos, then wraps back to zero, and increment pos
		bitpos= 0;
		pos++;
		bits -= outpos;
	}
	while (bits) {
		while (pos == lim) {
			if (part - in->str->parts + 1 == in->str->part_count) {
				userp_diag_set(&in->str->env->err, USERP_EOVERFLOW, "Unexpected end of input");
				in->fail= true;
				return 0;
			}
			++part;
			assert((part->data & (sizeof(IOWORD)-1) == 0);
			assert((part->len  & (sizeof(IOWORD)-1) == 0);
			pos= (IOWORD*) part->data;
			lim= pos + part->len / sizeof(IOWORD);
		}
		if (bits < sizeof(IOWORD)*8) {
			// a partial word will satisfy the read
			out |= ((LIBWORD)(in->pos[0] & (((IOWORD)1 << bits)-1))) << outpos;
			bitpos= sizeof(IOWORD)*8 - bits;
			bits= 0;
		} else {
			// use the whole word
			out |= ((LIBWORD) *pos++) << outpos;
			outpos += sizeof(IOWORD)*8;
			bits -= sizeof(IOWORD)*8;
		}
	}
	in->pos= pos;
	in->lim= lim;
	in->part= part;
	in->bitpos= bitpos;
	return out;
}

bool userp_bitdec_skip_bits(struct userp_bitdec *in, size_t bits) {
	struct userp_bstr_part *part= in->part;
	IOWORD *pos= in->pos, *lim= in->lim;
	size_t bitpos= in->bitpos + bits;
	while (1) {
		while (pos == lim) {
			if (part - in->str->parts + 1 == in->str->part_count) {
				userp_diag_set(&in->str->env->err, USERP_EOVERFLOW, "Unexpected end of input");
				in->fail= true;
				return false;
			}
			++part;
			assert((part->data & (sizeof(IOWORD)-1) == 0);
			assert((part->len  & (sizeof(IOWORD)-1) == 0);
			pos= (IOWORD*) part->data;
			lim= pos + part->len / sizeof(IOWORD);
		}
		if ((bitpos + sizeof(IOWORD)*8-1 >> LOG2_IOWORD_BITS) <= in->lim - in->pos) {
			in->pos= pos + (bitpos >> LOG2_IOWORD_BITS);
			in->lim= lim;
			in->part= part;
			in->bitpos= bitpos & sizeof(IOWORD)*8-1;
			return true;
		}
		else {
			bitpos -= (lim - pos) << LOG2_IOWORD_BITS;
			pos= lim;
		}
	}
}
