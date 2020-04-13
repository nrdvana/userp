#ifndef USERP_PROTECTED_H
#define USERP_PROTECTED_H
#include "userp.h"

struct userp_env {
	userp_alloc_fn *alloc;
	void *alloc_cb_data;
	userp_diag_fn *diag;
	void *diag_cb_data;
	int run_with_scissors: 1,
		jump_on_landmine: 1;
	
	/* Storage for error conditions */
	int diag_code;
	char *diag_tpl;
	int diag_align;
};

#define DIAG_VAR_ALIGN_ID 0x01
#define DIAG_VAR_ALIGN "\x01\x01"

extern bool userp_alloc_default(void *callback_data, void **pointer, size_t new_size, int align_pow2_bits);
extern void userp_diag_default(void *callback_data, int diag_code, userp_env_t *env);

struct userp_buffer {
	const struct userp_buffer_vtable *vtable;
	userp_env_t *env;
	char *buf;
	size_t len, bitpos;
	int bigendian:1,
		buf_ptr_ofs: USERP_BITALIGN_MAX,
		align_pow2: USERP_BITALIGN_MAX_LOG2,
		mem_owner: 1;
};
typedef struct userp_buffer userp_buffer_t;

struct userp_buffer_vtable {
	bool (*decode_long)(userp_buffer_t *buf, long *value_p, int bits);
	bool (*encode_long)(userp_buffer_t *buf, long value, int bits);
	bool (*encode_bytes_zerocopy)(userp_buffer_t *buf, char **bytes_p, size_t count);
	bool (*decode_bytes_zerocopy)(userp_buffer_t *buf, char **bytes_p, size_t count);
	bool (*seek)(userp_buffer_t *buf, size_t bitpos);
	bool (*append_buf)(userp_buffer_t *buf, userp_buffer_t *buf2);
};

static bool buf_init(void *buffer, size_t size, bool bigendian, int alignment);

static inline bool buf_encode_int(userp_buffer_t *buf, long value, int bits) {
	return buf->vtable->encode_long(buf, value, bits);
}
static inline bool buf_decode_int(userp_buffer_t *buf, long *value_p, int bits) {
	return buf->vtable->decode_long(buf, value_p, bits);
}

static inline bool buf_encode_bytes_zerocopy(userp_buffer_t *buf, char **bytes_p, size_t count) {
	return buf->vtable->encode_bytes_zerocopy(buf, bytes_p, count);
}
static inline bool buf_decode_bytes_zerocopy(userp_buffer_t *buf, char **bytes_p, size_t count) {
	return buf->vtable->decode_bytes_zerocopy(buf, bytes_p, count);
}

static inline bool buf_encode_bytes(userp_buffer_t *buf, const char *src, size_t count) {
	char *dest;
	if (!buf->vtable->encode_bytes_zerocopy(buf, &dest, count)) return false;
	memcpy(dest, src, count);
	return true;
}
static inline bool buf_decode_bytes(userp_buffer_t *buf, char *dest, size_t count) {
	char *src;
	if (!buf->vtable->decode_bytes_zerocopy(buf, &src, count)) return false;
	memcpy(dest, src, count);
	return true;
}

static inline bool buf_seek_to_align(userp_buffer_t *buf, int pow2) {
	size_t mask= (1<<pow2)-1;
	return buf->vtable->seek(buf, (buf->bitpos + mask) & ~mask);
}
static inline bool buf_seek(userp_buffer_t *buf, size_t bitpos) {
	return buf->vtable->seek(buf, bitpos);
}

static inline bool buf_append_buf(userp_buffer_t *buf, userp_buffer_t *buf2) {
	return buf->vtable->append_buf(buf, buf2);
}

#endif
