#include "local.h"
#include "userp_private.h"

/*
=head2 userp_env_new

  bool my_alloc(void *callback_data, void **pointer, size_t new_size, int flags);
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
The second argument to the allocator is identical to L</userp_env_get_diag>, provided for convenience.

Note that debugging diagnostics are suppressed until you enable them with L</userp_env_set_log_level>.

=head1 userp_env_free

  bool userp_env_free(userp_env_t *env);

This frees all resources used by the environment and invalidates every pointer that refers to resources
of this environment.  If there are reference-counted objects still in use, this will raise a fatal error.
If you trap the fatal error, it will return C<false>, and you may try freeing it again later.
Otherwise, this always returns true.

=cut
*/

userp_env userp_new_env(userp_alloc_fn alloc_callback, userp_diag_fn diag_callback, void *callback_data, int flags) {
	userp_env env= NULL;
	void *alloc_callback_data= alloc_callback? callback_data : NULL;
	void *diag_callback_data= diag_callback? callback_data : NULL;
	if (!alloc_callback) alloc_callback= userp_alloc_default;
	//if (!diag_callback) diag_callback= userp_diag_default;
	if (alloc_callback(callback_data, (void**) &env, sizeof(struct userp_env), USERP_HINT_STATIC)) {
		bzero(env, sizeof(userp_env_t));
		env->alloc= alloc_callback;
		env->alloc_cb_data= (alloc_callback == userp_alloc_default)? env : alloc_callback_data;
		env->diag= diag_callback;
		env->diag_cb_data= diag_callback_data;
		env->refcnt= 1;
		env->log_warn= 1;
		return env;
	}
	diag_callback(callback_data, USERP_EALLOC, NULL);
	return false;
}

static void userp_free_env(userp_env env) {
	// release memory
	if (!env->alloc(env->alloc_cb_data, (void**)&env, 0, 0))
		// if that failed, report the error (fatal)
		userp_env_set_error(env, USERP_EALLOC, "Failed to free userp_env");
}

void userp_grab_env(userp_env env) {
	assert(env != NULL);
	assert(env->refcnt > 0 && env->refcnt+1 > 0);
	if (!++env->refcnt) {
		userp_env_set_error(env, USERP_EDOINGITWRONG, "Reference count exceeds size_t");
		--env->refcnt;
	}
}

void userp_drop_env(userp_env env) {
	assert(env != NULL);
	assert(env->refcnt > 0);
	// This will probably crash, since env is probably freed, but better to find those bugs sooner?
	if (!env->refcnt) {
		userp_env_set_error(env, USERP_EASSERT, "Called userp_drop_env when refcnt already 0");
		return;
	}
	if (env->log_trace)
		env->userp_env_diag(
	if (!--env->refcnt)
		userp_free_env(env);
}

bool userp_alloc(userp_env env, void **pointer, size_t elem_size, size_t count, const char * elem_name) {
	size_t n= elem_size * count;
	// check overflow
	if (!env->enable_landmines && SIZET_MUL_CAN_OVERFLOW(elem_size, count)) {
		env->diag_code= USERP_ELIMIT;
		env->diag_size= elem_size;
		env->diag_count= count;
		env->diag_tpl= "Allocation size (" USERP_DIAG_SIZE "x" USERP_DIAG_COUNT "exceeds size_t";
		return false;
	}
	if (env->alloc(env->alloc_cb_data, pointer, n, 0))
		return true;
	if (count) {
		env->diag_code= USERP_EALLOC;
		env->diag_size= n;
		env->diag_count= count;
		env->diag_buf= elem_name;
		env->diag_tpl= "Unable to allocate " USERP_DIAG_SIZE " bytes for " USERP_DIAG_BUFSTR " x" USERP_DIAG_COUNT;
	} else {
		env->diag_buf= *pointer;
		env->diag_tpl= "Unable to free pointer " USERP_DIAG_BUFADDR;
	}
	return false;
}

bool userp_alloc_default(void *callback_data, void **pointer, size_t new_size, int flags) {
	void *re;
	if (new_size) {
		if (!(re= realloc(*pointer, new_size)))
			return false;
		*pointer= re;
	}
	else if (*pointer) {
		free(*pointer);
		*pointer= NULL;
	}
	return true;
}

static void userp_diag_to_file(void *callback_data, int diag_code, userp_env_t *env) {
	FILE *dest= (FILE*) callback_data;
	if (USERP_IS_ERROR(diag_code) || USERP_IS_FATAL(diag_code)) {
		fprintf(dest, "error: ");
		userp_env_print_diag(env, dest);
		fputc('\n', dest);
		fflush(dest);
		if (USERP_IS_FATAL(diag_code)) abort();
	}
	else if (USERP_IS_WARN(diag_code)) {
		fprintf(dest, "warning: ");
		userp_env_print_diag(env, dest);
		fputc('\n', dest);
	}
}

void userp_env_set_file_logger(userp_env env, FILE *dest) {
	userp_env_set_logger(env, userp_diag_to_file, dest);
}

void userp_env_set_logger(userp_env env, userp_diag_fn diag_callback, void *callback_data) {
	
}

void userp_env_set_error(userp_env env, int code, const chart *tpl) {
	if (env) {
		env->diag_code= code;
		env->diag_tpl= tpl;
		if (env->diag)
			env->diag(env->diag_cb_data, env->diag_code, env);
	}
}

/*
=head2 userp_env_get_diag_code

  int userp_env_get_diag_code(userp_env_t *env);

Returns the integer value of the most recent diagnostic event.

=head2 userp_diag_code_name

  const char *userp_diag_code_name(int code)

Returns a static string naming the most recent diagnostic event.
(or the string "unknown")

=head2 userp_env_get_diag

  int userp_env_get_diag(userp_env_t *env, char *buf, size_t buflen);
  
  // query length of message
  str_len= userp_env_get_diag(env, NULL, 0);
  
  // build message
  buf= malloc(str_len+1);
  userp_env_get_diag(env, buf, str_len+1);

This function stringifies the last diagnostic message in the given Userp environment into the buffer you
provide, and returns the string length (in bytes) of the diagnostic message.  You may provide a NULL C<buf>
if you just want to find out the length of the string.  Like C<snprintf>, if the buffer you provide is
too small for the message and its NUL terminator, the buffer will be filled with a truncated message which
is always NUL terminated, but the return value will always be the length of the un-truncated message.

=head2 userp_env_print_diag

  int userp_env_print_diag(userp_env_t *env, FILE *fh);

Prints the last diagnostic message to a libc FILE handle, saving you the trouble of allocating a buffer
for the message.  A return value of -1 means that a libc fwrite error occurred.

=cut
*/

extern int userp_env_get_diag_code(userp_env_t *env) {
	return env->diag_code;
}

static int userp_process_diag_tpl(userp_env_t *env, char *buf, size_t buf_len, FILE *fh) {
	const char *from, *pos= env->diag_tpl, *p1, *p2;
	char tmp_buf[64], id;
	size_t n= 0, tmp_len, str_len= 0;
	int i;
	while (*pos) {
		// here, *pos is either \x01 followed by a code indicating which variable to insert,
		// or any other character means to scan forward.
		if (*pos == '\x01') {
			n= 0;
			from= tmp_buf;
			++pos;
			switch (id= *pos++) {
			// Buffer, displayed as a pointer address
			case USERP_DIAG_BUFADDR_ID:
				n= snprintf(tmp_buf, sizeof(tmp_buf), "%p", env->diag_buf);
				break;
			// Buffer, hexdump the contents
			case DIAG_VAR_BUFHEX_ID:
				p1= ((char*)env->diag_buf) + env->diag_pos;
				p2= p1 + (env->diag_len - env->diag_pos);
				for (n= 0; (n+1)*3 < sizeof(tmp_buf) && p1 < p2; p1++) {
					tmp_buf[n++]= "0123456789ABCDEF"[(*p1>>4) & 0xF];
					tmp_buf[n++]= "0123456789ABCDEF"[*p1 & 0xF];
					tmp_buf[n++]= ' ';
				}
				if (n) --n;
				tmp_buf[n]= '\0';
				break;
			// integer with "power of 2" notation
			case USERP_DIAG_ALIGN_ID:
				n= snprintf(tmp_buf, sizeof(tmp_buf), "2**%d", env->diag_align);
				break;
			// Generic integer fields
			case USERP_DIAG_POS_ID:
				n= env->diag_pos; if (0)
			case USERP_DIAG_LEN_ID:
				n= env->diag_len; if (0)
			case USERP_DIAG_SIZE_ID:
				n= env->diag_size; if (0)
			case USERP_DIAG_COUNT_ID:
				n= env->diag_count;
				n= snprintf(tmp_buf, sizeof(tmp_buf), "%ld", n);
				break;
			default:
				// If we get here, it's a bug.  If the second byte was NUL, back up one, else substitute an error message
				if (!pos[-1]) --pos;
				from= "(unknown var)";
				if (0)
			// Buffer, printed as a string
			case DIAG_VAR_BUFSTRZ_ID:
				from= env->diag_buf;
				n= strlen(from);
				break;
			}
		}
		else {
			from= pos;
			while (*pos > 1) ++pos;
			n= pos-from;
		}
		if (n) {
			if (buf && str_len+1 < buf_len) // don't call memcpy unless there is room for at least 1 char and NUL
				memcpy(buf+str_len, from, (str_len+n+1 <= buf_len)? n : buf_len-1-str_len);
			if (fh) {
				if (!fwrite(from, 1, n, fh))
					return -1;
			}
			str_len += n;
		}
	}
	// at end, NUL-terminate the buffer, if provided and has a length
	if (buf && buf_len > 0)
		buf[str_len+1 > buf_len? buf_len-1 : str_len]= '\0';
	return str_len;
}

extern int userp_env_get_diag(userp_env_t *env, char *buf, size_t buflen) {
	return userp_process_diag_tpl(env, buf, buflen, NULL);
}

extern int userp_env_print_diag(userp_env_t *env, FILE *fh) {
	return userp_process_diag_tpl(env, NULL, 0, fh);
}

#ifdef WITH_UNIT_TESTS

static bool logging_alloc(void *callback_data, void **pointer, size_t new_size, int flags) {
	userp_env env= (userp_env) callback_data;
	
}
static void stdout_logger(void *callback_data, int diag_code, userp_env_t *env) {
	if (USERP_IS_ERROR(diag_code) || USERP_IS_FATAL(diag_code)) {
		fprintf(stderr, "error: ");
		userp_env_print_diag(env, stderr);
		fputc('\n', stderr);
		fflush(stderr);
		if (USERP_IS_FATAL(diag_code)) abort();
	}
	else if (USERP_IS_WARN(diag_code)) {
		fprintf(stderr, "warning: ");
		userp_env_print_diag(env, stderr);
		fputc('\n', stderr);
	}
}

UNIT_TEST(test_userp_env_new_free) {
	userp_new_env(NULL, NULL, NULL, 0);
}

#endif
