#include "config.h"
#include "userp.h"
#include "userp_protected.h"

/*----------------------------------------------------------------------------------------------
 * Public API
 */

/*
=head2 userp_env_new

  bool my_alloc(void *callback_data, void **pointer, size_t new_size, int align_pow2_bits);
  bool my_diag(void *callback_data, int diag_code, userp_env_t *env);
  
  // normal environment:
  userp_env_t *env= userp_env_new(NULL, NULL, NULL);
  // custom environment;
  userp_env_t *env= userp_env_new(my_alloc, my_diag, my_callback_data);

Call userp_env_new to create a new userp environment.  The environment defines how memory is allocated,
how errors and warnings are reported, controls various limitations on resource usage, and acts as temporary
storage for diagnostic information.
In the default environment, memory allocation is performed by malloc, free, and realloc.  Debug messages
are suppressed, warnings/errors are written to STDERR, and fatal errors call abort().

=head3 Allocator Callback

The allocator is a single function that performs all the duties of malloc, realloc, and free.  It is given
a pointer to the pointer to operate on.  It should change that pointer to reference C<new_size> bytes of
memory, aligned to C<< 2**align >> bits.  (Of course the pointer will always be byte-aligned, but the
alignment is specified as a power of bits for consistency with other Userp API where bit alignment matters)
A request for 0 bytes of memory should free the buffer and store NULL into C<*pointer>.

The first argument to the allocator is whatever you pass as the final argument to C<userp_env_new>.

=head3 Diagnostic Callback

The diagnostic callbck is a function that can change the handling of loggig and exceptions for the Userp
API.  Userp generates various diagnostic messages of different importance.  These messages are stored as
data fields in the userp_env_t and not converted to character strings unless they need to be printed.
By default, only important diagnostics or fatal errors are written to STDERR.  In addition, fatal errors
call C<< abort() >>.  When writing your own diagnostic handler, call the various diagnostic-formatting
functions such as L</userp_diag_to_str> and send the text wherever it needs to go.  Then, if the diagnostic
is a fatal error, decide for yourself whether to call abort() or just return.  All Userp API can deal with
internal failures gracefully, but then you need to make sure to check all the return values if you enable
that.

The first argument to the allocator is whatever you pass as the final argument to L</userp_env_new>.
The second argument to the allocator is identical to L</userp_env_get_diag_code>, provided for convenience.

Note that debugging diagnostics are suppressed until you enable them with L</userp_env_set_log_level>.

*/

bool userp_alloc_default(void *callback_data, void **pointer, size_t new_size, int align_pow2_bits);
void userp_diag_default(void *callback_data, int diag_code, userp_env_t *env);

userp_env_p userp_env_new(userp_alloc_fp alloc_callback, userp_log_fp diag_callback, void *callback_data) {
	userp_env_t *env= NULL;
	if (!alloc_callback) alloc_callback= userp_alloc_default;
	if (!diag_callback) diag_callback= userp_diag_default;
	if (alloc_callback(callback_data, &env, sizeof(userp_env_t), 0)) {
		bzero(env, sizeof(env));
		env->alloc= alloc_callback;
		env->diag= diag_callback;
		return env;
	}
	diag_callback(callback_data, USERP_ERR_ALLOC, NULL);
	return false;
}

/* Return a static copy of the error code's symbolic name */
const char* userp_get_error_name(int errcode) {
	#define RETSTR(x) case x: return #x;
	switch (errcode) {
	RETSTR(USERP_ERR_INVALID_ARG)
	RETSTR(USERP_ERR_ALLOC)
	default:
		return "Invalid Error Code";
	}
	#undef RETSTR
}

// The API user gets one and only one reference (marked by flag USERP_BLOCK_HELD_USER)
int userp_release_block(UserpBlock_t *block) {
	#ifndef USERP_RUN_WITH_SCISSORS
	if (!block || !block->refcnt || !block->state)
		return USERP_ERR_DOUBLEFREE;
	if (!(block->flags & USERP_BLOCK_HELD_USER)) {
		block->state->error= USERP_ERR_DOUBLEFREE;
		block->state->error_msg[0]= '\0';
		return USERP_ERR_DOUBLEFREE;
	}
	#endif
	userp_state_release_block(block->state, block);
	return 0;
}

/** userp_get_last_error
 *
 * Returns various information about the last error related to this UserpState or its blocks.
 * (i.e. the reason why some other function returned zero)
 *
 * If errname is given, it will receive a pointer to static constant nul-terminated string
 * for the error code.
 *
 * If diag_buf is given (and diag_buflen, specifying its length) it will receive additional
 * diagnostic text about the error.  It behaves as snprintf and will always be NUL terminated
 * even if the buffer was too small.
 *
 * If diag_buflen is given, it will be updated to hold the length (in characters not including
 * NUL terminator) of the additional diagnostic text.
 *
 * The error code is returned.
 */
int userp_get_last_error(UserpState_t *state, const char **errname, char *diag_buf, int *diag_buflen) {
	int copied;
	if (errname) *errname= userp_get_error_name(state->error);
	if (diag_buf && diag_buflen) {
		*diag_buflen= snprintf("%s", diag_buf, *diag_buflen, state->error_msg);
	} else if (diag_buflen) {
		*diag_buflen= strlen(state->error_msg);
	}
	return state->error;
}

/** userp_parse_block
 *
 * Given a block of Userp data or metadata, begin parsing it and return a reference to a
 * UserpBlock object.
 *   - state: required
 *   - flags: must include either USERP_META_BLOCK or USERP_DATA_BLOCK; other flags listed below.
 *   - blockdata: pointer to beginning of block data
 *   - blocklen: number of bytes of block data
 *
 * Returns NULL if the arguments don't make sense or if parsing immediately fails.  Parsing
 * errors can also happen during later calls.
 *
 * Every UserpBlock_t must be freed with user_free_block.  See notes on that function about
 * block lifespan.
 *
 */
UserpBlock_t *userp_parse_block(UserpState_t *state, int flags, const void *blockdata, size_t blocklen) {
	#ifndef USERP_RUN_WITH_SCISSORS
	if (!(flags & (USERP_META_BLOCK|USERP_DATA_BLOCK))) {
		ERR_ARG(state, userp_new_block, flags);
		return NULL;
	}
	if (!blockdata) {
		ERR_ARG(state, userp_new_block, blockdata);
		return NULL;
	}
	if (!blocklen) {
		ERR_ARG(state, userp_new_block, blocklen);
		return NULL;
	}
	#endif
	return userp_state_new_block(state, flags, (void*) blockdata, blocklen);
}

/** userp_new_block
 *
 * Create (for writing) a new UserpBlock
 *  - state: required
 *  - flags: must include either USERP_META_BLOCK or USERP_DATA_BLOCK; other flags listed below.
 *  - buffer: optional.  If NULL, UserpState will allocate the memory as needed.  If not null,
 *            bufferlen must be big enough to hold however much data you write into this block.
 *            For advanced use, the block may already contain data for zero-copy effect.
 *  - bufferlen: length in bytes of buffer, or zero to ask that UserpState allocate as needed.
 */
UserpBlock_t *userp_new_block(UserpState_t *state, int flags, void *buffer, size_t bufferlen) {
	#ifndef USERP_RUN_WITH_SCISSORS
	if (!(flags & (USERP_META_BLOCK|USERP_DATA_BLOCK))) {
		ERR_ARG(userp_new_block, flags);
		return NULL;
	}
	#endif
	return userp_state_new_block(state, flags, (void*) blockdata, blocklen);
}

