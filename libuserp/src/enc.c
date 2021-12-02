#include "local.h"
#include "userp_private.h"

userp_enc userp_new_enc(userp_env env, userp_scope scope, userp_type root_type) {
	userp_enc enc= NULL;
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

	if (!USERP_ALLOC_OBJPLUS(env, &enc, env->enc_output_parts * sizeof(struct userp_bstr_part)))
		return NULL;
	if (!userp_grab_scope(scope)) {
		USERP_FREE(env, &enc);
		return NULL;
	}

	bzero(enc, sizeof(struct userp_enc));
	enc->env= env;
	enc->scope= scope;
	enc->output.parts= enc->output_initial_parts;
	enc->output.part_alloc= env->enc_output_parts;
	return enc;
}

void userp_free_enc(userp_enc enc) {
	struct userp_bstr_part *p, *p2;
	// TODO: free each of the encoder frames
	// Release each buffer of the output
	for (p= enc->output.parts, p2= p + enc->output.part_count; p < p2; p++)
		userp_drop_buffer(p->buf);
	// Free the bstr unless it was allocated as part of this object
	if (enc->output.parts != enc->output_initial_parts)
		USERP_FREE(enc->env, enc->output.parts);
	userp_drop_scope(enc->scope);
	USERP_FREE(enc->env, enc);
}

static struct userp_bstr_part * userp_enc_make_room(userp_enc enc, size_t n, int align) {
	struct userp_bstr_part *part;
	size_t ofs= 0;
	userp_buffer buf= NULL;
	size_t alloc_n;

	// "commit" the progress from out_pos back to the bstr
	if (enc->out_pos) {
		part= &enc->output.parts[enc->output.part_count-1];
		part->len= enc->out_pos - part->data;
		ofs= part->ofs + part->len;
	}

	// Is there room in the bstr?
	if (enc->output.part_count >= enc->output.part_alloc) {
		alloc_n= enc->output.part_alloc * 2;
		// If output is the one built into the record, can't resize it.
		if (enc->output.parts == enc->output_initial_parts) {
			part= NULL;
			if (!USERP_ALLOC_ARRAY(enc->env, &part, alloc_n))
				return NULL;
			memcpy(part, enc->output.parts, sizeof(struct userp_bstr_part) * enc->output.part_count);
			enc->output.parts= part;
			enc->output.part_alloc= alloc_n;
			enc->output.env= enc->env;
		}
		else {
			if (!USERP_ALLOC_ARRAY(enc->env, &enc->output.parts, alloc_n))
				return NULL;
		}
	}

	// Allocate a new buffer for the bstr
	// TODO: grow the allocation size each time
	alloc_n= enc->env->enc_output_bufsize;
	if (!(buf= userp_new_buffer(enc->env, NULL, alloc_n, 0)))
		return NULL;

	// Initialize the new part with the new buffer
	part= &enc->output.parts[enc->output.part_count++];
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

struct userp_bstr* userp_enc_finish(userp_enc enc) {
	struct userp_bstr_part *part;
	// TODO: finish any current frames
	// "commit" the progress from out_pos back to the bstr
	if (enc->out_pos) {
		part= &enc->output.parts[enc->output.part_count-1];
		part->len= enc->out_pos - part->data;
	}
	return &enc->output;
}
