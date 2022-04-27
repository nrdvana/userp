#include "local.h"
#include "userp_private.h"

userp_enc userp_new_enc(userp_env env, userp_scope scope, userp_type root_type) {
	userp_enc enc= NULL;
	if (!scope || scope->env != env) {
		userp_diag_set(&env->err, USERP_EFOREIGNSCOPE, "userp_scope does not belong to this userp_env");
		USERP_DISPATCH_ERR(env);
		return NULL;
	}
	if (!root_type /* TODO: or type does not belong to scope */) {
		userp_diag_set(&env->err, USERP_ETYPESCOPE, "userp_type does not belong to the current userp_scope");
		USERP_DISPATCH_ERR(env);
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

/* userp_enc_rec_begin
 *
 * Configure the encoder to begin writing fields of a record.
 */
bool userp_enc_rec_begin(userp_enc enc, userp_type type) {
	// If the record does not allow 'extra' or 'ad-hoc' fields, then just allocate
	// at least the static length (aligned) on the end of the curent buffer (or make a new buffer).
	...
	// Else there might be dynamic elements.  Create a new buffer to hold the static area and
	// encoded fields.  Don't finish the previous buffer until the record is finalized.
	...
	// Seek to the first field of the record.
	...
}

bool userp_enc_rec_declare_fields(userp_enc enc, size_t fields[], size_t field_count) {
	// Iterate fields building list of 'extra' and 'ad-hoc'.
	...
	// If fields are not in order, warn user
	...
	// Write out the list of 'extra' and 'ad-hoc' in same order provided
	// Allocate the static storage region.
	// Set the fields list, to be iterated as fields are written.  Mark it as final.
}

bool userp_enc_rec_seek_field(userp_enc enc, size_t field_id) {
	// If this field has static placement,
	//  If this field has a mask or shift or byteswap, start a new temporary buffer
	...
	//  Else use the static region as-is (making sure it is allocated)
	// Else, if the field is dynamically placed and the previous field has not been
	// written, start a new temporary buffer.
}

/* userp_enc_rec_add_field
 *
 * Add an 'ad-hoc' field to the record.
 */
bool userp_enc_rec_add_field(userp_enc enc, userp_symbol sym, userp_type type) {
	// Check if the symbol already exists as a field.
	...
	// Does the type match extra_field_type?
	...
	// If not all the normal fields have been written, start a new buffer
	...
	// Allocate space on the list of extra/adhoc fields, and add this one (but don't mark
	// as officially added until _commit_field)
	...
}

/* userp_enc_rec_commit_field
 *
 * Perform any post-write operations needed to encode this field.
 * This includes things like shift/mask for static placement fields, or recording
 * the field in the 'extra' set.
 */
bool userp_enc_rec_commit_field(userp_enc enc) {
	...
}

/* userp_enc_rec_end
 *
 * Finish writing the record into the buffer, and clean up data structures.
 */
bool userp_enc_rec_end(userp_enc enc) {
	// If not "run with scissors"
		// For each conditionally-present field, check that its condition matches the
		// presence of the field.  If the condition is false and field was written, set it
		// to true by setting the lowest bit in the mask of the condition.
	// else
		// for each present field with a conditional, set the bit for it
	// append all the buffers into the output
}

