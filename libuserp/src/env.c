#include "local.h"
#include "userp_private.h"

static bool userp_default_alloc_fn(void *unused, void **pointer, size_t new_size, int flags);

/*APIDOC
## userp_env

### Synopsis

    // default environment:
    userp_env env= userp_new_env(NULL, NULL, NULL);
    ...
    userp_drop_env(env); // doesn't get freed until final reference drops
    
    // custom environment;
    bool my_alloc(void *callback_data, void **pointer, size_t new_size, int flags);
    bool my_diag(void *callback_data, userp_diag diag, int diag_code);
    env= userp_new_env(my_alloc, my_diag, my_callback_data);

### Description

The userp C library is completely thread-unaware, but can be used in a threaded program because
the library does not use any global state.  All things that might otherwise have been global state
are stored in an environment object.  Only one thread may use a `userp_env` and its associated
objects at a time.

The environment object mainly deals with:
  * memory management functions
  * logging and error behavior
  * general flags that affect the behavior of the API

### Construction & Lifecycle

#### userp_new_env

    userp_env env= userp_new_env(userp_alloc_fn, userp_diag_fn, callback_data);
    ...
    userp_drop_env(env);

This creates a new userp_env object.  Any parameter may be NULL, to use the default.  If you
provide an allocator function, the memory for the userp_env comes from it, else it comes from
`malloc()`.  If allocation or initialization fails, it returns NULL, and the error will be logged
to the provided diagnostic function, else the default diagnostic function which writes to `stderr`.
The third parameter provides the user-specified callback data for the custom logging or allocation
callback.  

The allocation function cannot be changed, but the logger can be set later using
`userp_env_set_logger`.

A `userp_env` object contains an internal reference count.  It remains until the last reference
is dropped at which time it gets destroyed and freed through the same allocation function that
it came from.  The `userp_env` returned has an initial reference count of 1.  You should call
`userp_drop_env` to release it when you are done.  Other objects like `userp_scope`, `userp_enc`,
`userp_dec`, and `userp_buffer` may hold references to it.

#### userp_grab_env

    bool success= userp_grab_env(env);

This adds a reference to the specified `userp_env` object.  It can fail and emit a fatal error
if `env` is not valid or if the internal reference count is corrupt.

#### userp_drop_env

    userp_drop_env(env);

Release a reference to the `userp_env`, possibly destroying it if the last reference is removed.
This can emit a fatal error if `env` is not valid or if the internal reference count is corrupt.

*/

userp_env userp_new_env(userp_alloc_fn alloc_fn, userp_diag_fn diag_fn, void *cb_data, int flags) {
	userp_env env= NULL;
	struct userp_diag err;
	void *alloc_callback_data= alloc_fn? cb_data : NULL;
	void *diag_callback_data= diag_fn? cb_data : stderr;
	if (!alloc_fn) alloc_fn= userp_default_alloc_fn;
	if (!diag_fn) diag_fn= userp_file_logger;
	if (!alloc_fn(alloc_callback_data, (void**) &env, sizeof(struct userp_env), USERP_HINT_STATIC|USERP_HINT_PERSIST)) {
		bzero(&err, sizeof(err));
		err.code= USERP_EALLOC;
		err.tpl= "Allocation failed for " USERP_DIAG_SIZE " bytes";
		err.size= sizeof(struct userp_env);
		diag_fn(diag_callback_data, &err, err.code);
		return NULL;
	}
	bzero(env, sizeof(struct userp_env));
	env->alloc= alloc_fn;
	env->alloc_cb_data= alloc_callback_data;
	env->diag= diag_fn;
	env->diag_cb_data= diag_callback_data;
	env->refcnt= 1;
	env->log_warn= 1;
	return env;
}

static void userp_free_env(userp_env env) {
	userp_diag_fn *diag= env->diag;
	void *diag_cb_data= env->diag_cb_data;
	userp_alloc_fn *alloc= env->alloc;
	void *alloc_cb_data= env->alloc_cb_data;
	struct userp_diag err;

	if (env->err.buf) {
		//TODO:userp_drop_buffer(env->err.buf);
		env->err.buf= NULL;
	}
	if (env->measure_twice) {
		bzero(env, sizeof(*env)); // help identify freed env
		env->measure_twice= 1;
	}
	// release memory
	if (!alloc(alloc_cb_data, (void**)&env, 0, 0)) {
		// report the error from a temporary buffer first, just in case writes
		// to the env->err fail.
		bzero(&err, sizeof(err));
		err.code= USERP_EASSERT;
		err.tpl= "Failed to free userp_env";
		diag(diag_cb_data, &err, err.code);
		// Then copy it for future reference
		env->err= err;
	}
}

bool userp_grab_env(userp_env env) {
	struct userp_diag diag;
	if (!env || !env->refcnt) {
		bzero(&diag, sizeof(diag));
		diag.code= USERP_EASSERT;
		diag.tpl= "Attempt to grab a destroyed userp_env";
		userp_file_logger(stderr, &diag, diag.code);
	}
	if (++env->refcnt)
		return true;
	--env->refcnt;
	userp_diag_set(&env->err, USERP_EALLOC, "Reference count exceeds size_t", NULL);
	return false;
}

void userp_drop_env(userp_env env) {
	struct userp_diag diag;
	if (!env || !env->refcnt) {
		bzero(&diag, sizeof(diag));
		diag.code= USERP_EASSERT;
		diag.tpl= "Attempt to drop a destroyed userp_env";
		userp_file_logger(stderr, &diag, diag.code);
	}
	if (!--env->refcnt)
		userp_free_env(env);
}

/*APIDOC
### Attributes

#### Memory Allocator

    typedef bool userp_alloc_fn(void *callback_data, void **pointer, size_t new_size, int flags);
    
    // Allocator pseudocode:
    bool my_alloc(my_heap_t *my_heap, void **pointer, size_t new_size, int flags) {
      if (!new_size) ... // perform "free(*pointer)"
      else if (*pointer) ... // perform "realloc(*pointer)" and assign new value to *pointer
      else ... // perform "malloc" and assign the value to *pointer
    }

The allocator is a single function that performs all the duties of `malloc`, `realloc`, and `free`.
The first argument is user-defined; likely a reference to some heap object.  The second argument
`pointer` is a reference to the pointer to be altered.  If allocating the initial value, the
pointer should be intially NULL, and will point to the new memory on successful return.  If the
pointer is not NULL, it refers to a previous allocation from this function, and will be resized
if possible, or changed to point to a new region of memory initialized to the same value as the
old region. (with uninitialized bytes in any region beyond the size of the old allocation)
If the requested size is zero, the pointer will be freed (if it was non-NULL) and set to NULL.

The function should return true if the operation succeeded, false otherwise.  During a
rellocation, a false return value should leave the existing allocation exactly as before.
Returning false on a free operation (new_size=0) indicates a fatal failure.

The `flags` indicate how the memory will be used:

  * USERP_HINT_STATIC
    The memory should not need to grow.  The allocator can optimize by not reserving any extra
	space at the end for reallocations.
  * USERP_HINT_DYNAMIC
    The memory is likely to grow.  The allocator can optimize by reserving a larger chunk of
	memory so that future reallocation does not need to relocate as often.
  * USERP_HINT_BRIEF
    The memory allocation is short-lived
  * USERP_HINT_PERSIST
    The memory allocation is likely to be held longer than many other allocations
  * USERP_HINT_ALIGN
    The library is trying to allocate an aligned buffer.  The allocator should return something
	aligned to a larger degree than normal, like a kilobyte boundary or whole page boundary.
	(a userp stream may request ANY alignment value, subject to the limits of the userp_env,
	 and if the allocator returns a buffer insufficiently aligned, the library will reallocate
	 to a larger size until the buffer contains an address of appropriate alignment)
  * USERP_POINTER_IS_BUFFER_DATA
    The pointer referenced is `&buffer->data` of a `userp_buffer` structure.  If the allocator
	wishes, it can use the macro `USERP_GET_BUFFER_FROM_DATA_PTR` to get a pointer to the buffer
	and inspect its attributes.

The allocator must be set in the initial call to `userp_new_env`, because this is the first moment
that memory gets allocated for the library.  There is no way to change the allocator later, though
you can modify the state of your `callback_data`.

*/

bool userp_default_alloc_fn(void *unused, void **pointer, size_t new_size, int flags) {
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

/*APIDOC
#### logger

    typedef void userp_diag_fn(void *callback_data, userp_diag diag, int diag_code);
    userp_env_set_logger(env, diag_fn, callback_data);
    
    // Example: log to syslog instead of stderr
    void log_to_syslog(void *unused, userp_diag diag, int code) {
      char buf[512];
      userp_diag_format(diag, buf, sizeof(buf));
      syslog(
        USERP_IS_FATAL(code)? LOG_CRIT
        : USERP_IS_ERROR(code)? LOG_ERR
        : USERP_IS_WARN(code)? LOG_WARNING
        : LOG_DEBUG,
        "%s", buf);
    }
    userp_env_set_logger(env, log_to_syslog, NULL);

The default logger writes all errors and warnings to `stderr`, and in the event of fatal errors it
also calls `abort()`.  You can supply your own logger to do deeper inspection of errors or to write
the messages differently.  You may also decide not to `abort()` on fatal errors.

The `callback_data` is a user-defined pointer.  The `userp_diag` object is a reference to a struct
holding all the details of the error.  The `diag_code` argument is the same as would be returned
by `userp_diag_get_code(diag)` but provided more conveniently since it is the first thing any
handler will need to know.

To use the default logging but change the `FILE*` the messages are written to, call

    userp_env_set_logger(env, userp_file_logger, stdout); // or any other FILE*

The logging/diagnostics object is discussed in greater detail under `userp_diag`.

Debug logging is not enabled by default; see `USERP_LOG_LEVEL` and `userp_env_set_attr`.

#### last_error

    userp_diag d= userp_env_get_last_error(env);

Get a reference to the last log message of severity "error" or higher.  The userp_diag is owned
by the `userp_env` instance, and the pointer is not valid after calling `userp_drop_env`.

*/

void userp_file_logger(void *callback_data, userp_diag diag, int code) {
	FILE *dest= (FILE*) callback_data;
	if (USERP_IS_ERROR(code) || USERP_IS_FATAL(code)) {
		fprintf(dest, "error: ");
		userp_diag_print(diag, dest);
		fputc('\n', dest);
		fflush(dest);
		if (USERP_IS_FATAL(code)) abort();
	}
	else {
		fprintf(dest, USERP_IS_WARN(code)? "warning: ":"debug: ");
		userp_diag_print(diag, dest);
		fputc('\n', dest);
	}
}

void userp_env_set_logger(userp_env env, userp_diag_fn diag_callback, void *callback_data) {
	if (!diag_callback) {
		env->diag= userp_file_logger;
		env->diag_cb_data= stderr;
	}
	else {
		env->diag= diag_callback;
		env->diag_cb_data= callback_data;
	}
}

userp_diag userp_env_get_last_error(userp_env env) {
	return &env->err;
}

void userp_env_emit_err(userp_env env) {
	env->diag(env->diag_cb_data, &env->err, env->err.code);
}

/*APIDOC
#### log_level

    userp_env_set_attr(env, USERP_LOG_LEVEL, USERP_LOG_DEBUG);

Set the log level.  It can be one of

  * USERP_LOG_FATAL
  * USERP_LOG_ERROR
  * USER_LOG_WARN, USERP_DEFAULT
  * USERP_LOG_DEBUG
  * USERP_LOG_TRACE

The library must be compiled with trace support to get any logging more verbose than
`USERP_LOG_DEBUG`.

#### safety

    userp_env_set_attr(env, USERP_SAFETY, USERP_RUN_WITH_SCISSORS);

The "safety" attribute enables or disabled checks for correctness within the library.
Removing checks can make it parse streams faster.  Adding checks can help track down bugs.

There are currently three settings:

  * USERP_MEASURE_TWICE - "measure twice, cut once".  Check all possible protocol mistakes,
    and all possible API mistakes.
  * USERP_DEFAULT       - Check all possible protocol mistakes, but lax checking on API usage.
  * USERP_RUN_WITH_SCISSORS - "Never run with scissors".  Assume protocol is encoded correctly
    and that all API calls are made correctly.  Never use this on un-trusted data.

*/

void userp_env_set_attr(userp_env env, int attr_id, size_t value) {
	int n;
	const char *attr_name= NULL;
	switch (attr_id) {
	case USERP_LOG_LEVEL:
		switch (value) {
		case USERP_DEFAULT:
		case USERP_LOG_WARN:
			env->log_warn= 1; env->log_debug= 0; env->log_trace= 0; break;
		case USERP_LOG_ERROR:
			env->log_warn= 0; env->log_debug= 0; env->log_trace= 0; break;
		case USERP_LOG_DEBUG:
			env->log_warn= 1; env->log_debug= 1; env->log_trace= 0; break;
		case USERP_LOG_TRACE:
			env->log_warn= 1; env->log_debug= 1; env->log_trace= 1; break;
		default:
			attr_name= "log level"; goto unknown_val;
		}
		return;
	case USERP_SAFETY:
		switch (value) {
		case USERP_DEFAULT:
			env->run_with_scissors= 0; env->measure_twice= 0; break;
		case USERP_RUN_WITH_SCISSORS:
			env->run_with_scissors= 1; env->measure_twice= 0; break;
		case USERP_MEASURE_TWICE:
			env->run_with_scissors= 1; env->measure_twice= 0; break;
		default:
			attr_name= "safety level"; goto unknown_val;
		}
		return;
	}
	CATCH(unknown_val) {
		if (!env->run_with_scissors) {
			userp_diag_set(&env->err, USERP_EINVAL, "Unknown " USERP_DIAG_CSTR1 ": " USERP_DIAG_INDEX, NULL);
			env->err.index= (int) value;
			env->err.cstr1= attr_name;
			userp_env_emit_err(env);
		}
	}
}

// ----------------------------- Private methods -----------------------------

bool userp_alloc_obj(userp_env env, void **pointer, size_t elem_size, int flags, const char * obj_name) {
	if (env->alloc(env->alloc_cb_data, pointer, elem_size, flags))
		return true;
	if (elem_size)
		userp_diag_set(&env->err, USERP_ELIMIT, "Unable to allocate " USERP_DIAG_CSTR1 " (" USERP_DIAG_SIZE " bytes)", NULL);
	else
		userp_diag_set(&env->err, USERP_EASSERT, "Unable to free " USERP_DIAG_CSTR1, NULL);
	env->err.cstr1= obj_name;
	env->err.size= elem_size;
	// de-allocation errors need reported immediately, because they can be fatal
	if (!elem_size) userp_env_emit_err(env);
	return false;
}

bool userp_alloc_array(userp_env env, void **pointer, size_t elem_size, size_t count, int flags, const char * elem_name) {
	size_t n= elem_size * count;
	// check overflow
	if (!env->run_with_scissors && SIZET_MUL_CAN_OVERFLOW(elem_size, count)) {
		userp_diag_set(&env->err, USERP_ELIMIT, "Allocation of " USERP_DIAG_COUNT "x " USERP_DIAG_CSTR1 " (" USERP_DIAG_SIZE ") exceeds size_t", NULL);
		env->err.size= elem_size;
		env->err.count= count;
		env->err.cstr1= elem_name;
		return false;
	}
	if (env->alloc(env->alloc_cb_data, pointer, n, flags))
		return true;
	if (count)
		userp_diag_set(&env->err, USERP_ELIMIT, "Unable to allocate " USERP_DIAG_COUNT "x " USERP_DIAG_CSTR1 " (" USERP_DIAG_SIZE " bytes)", NULL);
	else
		userp_diag_set(&env->err, USERP_EASSERT, "Unable to free array of " USERP_DIAG_COUNT " " USERP_DIAG_CSTR1, NULL);
	env->err.count= count;
	env->err.cstr1= elem_name;
	env->err.size= n;
	// de-allocation errors need reported immediately, because they can be fatal
	if (!count) userp_env_emit_err(env);
	return false;
}

#ifdef WITH_UNIT_TESTS

static bool logging_alloc(void *callback_data, void **pointer, size_t new_size, int flags) {
	void *prev= *pointer;
	bool ret= userp_default_alloc_fn(callback_data, pointer, new_size, flags);
	printf("alloc 0x%llX to %ld = 0x%llX\n", (long long) prev, new_size, (long long) *pointer);
	return ret;
}

UNIT_TEST(env_new_free) {
	userp_env env= userp_new_env(logging_alloc, userp_file_logger, stdout, 0);
	userp_free_env(env);
}
/*OUTPUT
/alloc 0x0+ to \d+ = 0x\w+/
/alloc 0x\w+ to 0 = 0x0+/
*/

#endif
