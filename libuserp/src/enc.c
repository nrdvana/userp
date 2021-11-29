#include "local.h"
#include "userp_private.h"

userp_enc userp_new_enc(userp_env env, userp_scope scope, userp_type root_type) {
	userp_enc enc= NULL;
	int default_output_parts= env->default_enc_output_parts? env->default_enc_output_parts
		: USERP_ENC_DEFAULT_OUTPUT_PARTS;

	if (!scope || scope->env != env) {
		userp_diag_set(&env->err, USERP_EFOREIGNSCOPE, "userp_scope does not belong to this userp_env");
		USERP_DISPATCH_ERROR(env);
		return NULL;
	}
	if (!root_type /* TODO: or type does not belong to scope */) {
		userp_diag_set(&env->err, USERP_ETYPESCOPE, "userp_type does not belong to the current userp_scope");
		USERP_DISPATCH_ERROR(env);
		return NULL;
	}

	if (!USERP_ALLOC_OBJPLUS(env, &enc, default_output_parts * sizeof(struct userp_bstr_part)))
		return NULL;
	if (!userp_grab_scope(scope)) {
		USERP_FREE(env, &enc);
		return NULL;
	}

	bzero(enc, sizeof(struct userp_enc));
	enc->env= env;
	enc->scope= scope;
	enc->output= &enc->output_inst;
	enc->output_inst.part_count= 0;
	enc->output_inst.part_alloc= default_output_parts;
	return enc;
}

void userp_free_enc(userp_enc enc) {
	struct userp_bstr_part *p, *p2;
	// TODO: free each of the encoder frames
	// Release each buffer of the output
	for (p= enc->output->parts, p2= p + enc->output->part_count; p < p2; p++)
		userp_drop_buffer(p->buf);
	// Free the bstr unless it was allocated as part of this object
	if (enc->output != &enc->output_inst)
		USERP_FREE(enc->env, enc->output);
	userp_drop_scope(enc->scope);
	USERP_FREE(enc->env, enc);
}

static struct userp_bstr_part * userp_enc_make_room(userp_enc enc, size_t n, int align) {
	userp_bstr bstr= enc->output;
	struct userp_bstr_part *part;
	size_t ofs= 0;
	userp_buffer buf= NULL;
	// TODO: grow the allocation size each time
	size_t alloc_n= USERP_ENC_DEFAULT_OUTPUT_BUFSIZE;

	// "commit" the progress from out_pos back to the bstr
	if (enc->out_pos) {
		part= &bstr->parts[bstr->part_count-1];
		part->len= enc->out_pos - part->data;
		ofs= part->ofs + part->len;
	}

	// Is there room in the bstr?
	if (bstr->part_count >= bstr->part_alloc) {
		// If output is the one built into the record, can't resize it.
		if (bstr == &enc->output_inst) {
			bstr= NULL;
			if (!USERP_ALLOC_ARRAY(enc->env, &bstr, enc->output_inst.part_alloc * 2))
				return NULL;
			memcpy(bstr, &enc->output_inst, USERP_SIZEOF_BSTR(enc->output_inst.part_alloc));
			enc->output_inst.part_count= 0;
			enc->output= bstr;
		}
		else {
			if (!USERP_ALLOC_ARRAY(enc->env, &enc->output, enc->output->part_alloc * 2))
				return NULL;
			bstr= enc->output;
		}
	}

	// Allocate a new buffer for the bstr
	if (!(buf= userp_new_buffer(enc->env, NULL, alloc_n, 0)))
		return NULL;

	// Initialize the new part with the new buffer
	part= &bstr->parts[bstr->part_count++];
	part->buf= buf;
	part->data= buf->data;
	part->len= 0;
	part->ofs= ofs;

	// Update the pointers for the current span of new buffer
	enc->out_pos= part->data;
	enc->out_lim= part->data + part->buf->alloc_len;

	return part;
}

bool userp_enc_int(userp_enc enc, int value) {
	// TODO: switch based on current frame's type and alignment requirements
	if (enc->out_lim - enc->out_pos < 4) {
		if (!userp_enc_make_room(enc, 4, 0))
			return false;
	}
	memcpy(enc->out_pos, &value, 4);
	enc->out_pos += 4;
	return true;
}

userp_bstr userp_enc_finish(userp_enc enc) {
	struct userp_bstr_part *part;
	// TODO: finish any current frames
	// "commit" the progress from out_pos back to the bstr
	if (enc->out_pos) {
		part= &enc->output->parts[enc->output->part_count-1];
		part->len= enc->out_pos - part->data;
	}
	return enc->output;
}
