#include "local.h"
#include "userp_private.h"

bool userp_bstr_alloc(userp_env env, userp_bstr *ptr, size_t part_count) {
	size_t n_alloc= USERP_BSTR_PART_ALLOC_ROUND(part_count);
	int i;
	// If the user is requesting to shrink the bstr, free the parts that will get trimmed
	if (*ptr && (*ptr)->part_count > part_count) {
		for (i= (*ptr)->part_count-1; i >= part_count; --i) {
			userp_drop_buffer((*ptr)->parts[i].buf);
		}
		(*ptr)->part_count= part_count;
		// Ignore requests to reallocate smaller unless it would save a lot of memory,
		// or unless it would free the bstr.
		if (part_count && (*ptr)->part_count < (n_alloc<<4))
			return true;
	}
	if (!part_count)
		return USERP_FREE(env, ptr);
	if (!USERP_ALLOC_OBJPLUS(env, ptr, n_alloc * sizeof(struct userp_bstr_part)))
		return false;
	(*ptr)->part_alloc= n_alloc;
	return true;
}



//bool userp_grow_buffer(userp_buffer buf, size_t alloc_len) {
//	if (!buf->env) return false;
//	if (!(buf->flags & USERP_BUFFER_ENV_ALLOC)) {
//		userp_env_set_error(buf->env, USERP_EINVAL, "buffer is not resizable");
//		return false;
//	}
//	if (buf->alloc_len >= alloc_len) return true;
//	alloc_len= USERP_BUFFER_DATA_ALLOC_ROUND(alloc_len);
//	return USERP_ALLOC_ARRAY(buf->env, &buf->data, uint8_t, alloc_len, USERP_HINT_DYNAMIC);
//}
//
//userp_bstr userp_new_bstr(userp_env env, int part_alloc_count) {
//	userp_bstr str= NULL;
//	if (!USERP_ALLOC(env, &str, SIZEOF_USERP_BSTR(part_alloc_count)))
//		return NULL;
//	str->env= env;
//	str->part_count= 0;
//	str->part_alloc= part_alloc_count;
//	return str;
//}
//
//bool userp_free_bstr(userp_bstr *str) {
//	size_t i;
//	for (i= 0; i < (*str)->part_count; i++) {
//		if ((*str)->parts[i].buf && (*str)->parts[i].buf->refcnt)
//			userp_drop_buffer((*str)->parts[i].buf);
//	}
//	USERP_FREE((*str)->env, str);
//}
//

uint8_t* userp_bstr_append_bytes(userp_env env, userp_bstr *str, const uint8_t* bytes, size_t n) {
	userp_buffer buf;
	struct userp_bstr_part *part;
	uint8_t *ret= NULL;
	// In order to append to an existing buffer, it must have the same 'env', it must be flagged
	// USERP_BUFFER_APPENDABLE, and must have a refcnt of 1.
	if (*str && (*str)->part_count) {
		part= &(*str)->parts[(*str)->part_count-1];
		buf= part->buf;
		if (buf && buf->env == env                      // from same env
			&& (buf->flags & USERP_BUFFER_APPENDABLE)   // and marked as writable
			&& buf->refcnt == 1                         // and not used by anything else
			&& part->data + part->len + n < buf->data + buf->alloc_len // and has room
		) {
			ret= part->data + part->len;
			part->len += n;
		}
	}
	if (!ret) { // need a new buffer?
		// Ensure room for a new part
		if (!*str || (*str)->part_count >= (*str)->part_alloc)
			if (!userp_bstr_alloc(env, str, *str? (*str)->part_count+1 : 1))
				return NULL;
		// Then allocate another buffer for this new part
		if (!(buf= userp_new_buffer(env, NULL, n, USERP_BUFFER_APPENDABLE)))
			return NULL;
		// Add the part
		part= &(*str)->parts[(*str)->part_count++];
		part->buf= buf;
		part->data= buf->data;
		part->len= n;
		ret= part->data;
	}
	// bytes does not need to be defined.
	if (bytes)
		memcpy(ret, bytes, n);
	return ret;
}

//bool userp_bstr_append_parts(userp_bstr *str, size_t n_parts, const struct userp_bstr_part *src_parts) {
//	size_t n, i, ofs_diff;
//	struct userp_bstr_part *new_parts;
//	// is there room for this number of parts?
//	if ((*str)->part_count + n_parts > (*str)->part_alloc) {
//		// round up to multiple of 16, leaving at least 8 slots open
//		n= USERP_BSTR_PART_ALLOC_ROUND( (*str)->part_count + n_parts );
//		// realloc, possibly changing *str
//		if (!USERP_ALLOC((*str)->env, str, SIZEOF_USERP_BSTR((*str)->part_count + n_parts)))
//			return false;
//		(*str)->part_alloc= n;
//	}
//	// If src_parts is not given, then the only change is enlarging the string object, above.
//	// If it is given, then also copy the parts onto the end of the string and update the
//	// offsets and grab references to the buffers.
//	if (src_parts) {
//		new_parts= (*str)->parts + (*str)->part_count;
//		memcpy(new_parts, src_parts, sizeof(struct userp_bstr_part)*n_parts);
//		ofs_diff= ((*str)->part_count == 0? 0 : (new_parts[-1].ofs + new_parts[-1].len)) - src_parts[0].ofs;
//		for (i= 0; i < n_parts; i++) {
//			new_parts[i].ofs += ofs_diff;
//			if (i > 0 && new_parts[i].ofs < new_parts[i-1].ofs + new_parts[i-1].len) {
//				// invalid indexing on string
//				(*str)->env->diag_pos= i;
//				fatal_error((*str)->env, USERP_ESTRINDEX, "src_parts[" USERP_DIAG_POS "].ofs is invalid");
//				return false;
//			}
//		}
//		for (i= 0; i < n_parts; i++)
//			userp_grab_buffer(new_parts[i].buf);
//		(*str)->part_count += n_parts;
//	}
//	return true;
//}

#if 0 && HAVE_POSIX_FILES

bool userp_bstr_append_file(userp_bstr *str, int fd, size_t length) {
	int got;
	void *dest= userp_bstr_append_bytes(str, length, NULL);
	if (!dest)
		return false;
	got= read(fd, dest, length);
	if (got > 0) {
		((*str)->parts + (*str)->part_count - 1)->len += got;
		return true;
	}
	else if (got == 0
		|| errno == EAGAIN
		|| errno == EINTR
		|| errno == EWOULDBLOCK
	) {
		userp_env_set_error((*str)->env, USERP_EEOF, "No bytes ready to read from stream");
	}
	else {
		(*str)->env->diag_pos= errno;
		userp_env_set_error((*str)->env, USERP_ESYSERROR, "System error " USERP_DIAG_POS " while calling read()");
	}
	return false;
}

#endif

#if 0 && HAVE_POSIX_MEMMAP

bool userp_bstr_map_file(userp_bstr *str, int fd, int64_t offset, int64_t length) {
	// Does the string already have a part adjacent or overlapping with this request?
}

#endif

/*
bool userp_bstr_splice(userp_bstr *dst, size_t dst_ofs, size_t dst_len, userp_bstr *src, size_t src_ofs, size_t src_len) {
}

void userp_bstr_crop(userp_bstr *str, size_t trim_head, size_t trim_tail) {
}
*/
