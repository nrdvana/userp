#include "local.h"
#include "userp_protected.h"

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
