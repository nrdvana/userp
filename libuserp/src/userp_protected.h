#ifndef USERP_PROTECTED_H
#define USERP_PROTECTED_H
#include <stdint.h>

struct userp_buffer {
	char *buf;
	size_t bitpos, bitlen;
	int align_pow2;
	const struct userp_buffer_vtable *vtable;
};
typedef struct userp_buffer_t;

static inline bool buf_encode_int(userp_buffer_t *buf, long value, int bits) {
	return buf->vtable->encode_long(buf, value, bits);
}
static inline bool buf_decode_int(userp_buffer_t *buf, long *value_p, int bits) {
	return buf->vtable->decode_long(buf, value_p, int bits);
}

static inline bool buf_encode_bytes_zerocopy(userp_buffer_t *buf, char **bytes_p, size_t count) {
	return buf->vtable->encode_bytes_zerocopy(buf, bytes_p, count);
}
static inline bool buf_decode_bytes_zerocopy(userp_buffer_t *buf, char **bytes_p, size_t count) {
	return buf->vtable->decode_bytes_zerocopy(buf, bytes_p, count);
}

static inline bool buf_encode_bytes(userp_buffer_t *buf, char *src, size_t count) {
	const char *dest;
	if (!buf->vtable->encode_bytes_zerocopy(buf, &dest, count)) return false;
	memcpy(dest, src, count);
	return true;
}
static inline bool buf_decode_bytes(userp_buffer_t *buf, char *dest, size_t count) {
	const char *src;
	if (!buf->vtable->decode_bytes_zerocopy(buf, &src, count)) return false;
	memcpy(dest, src, count);
	return true;
}

struct userp_buffer_vtable {
	bool (*decode_long)(userp_buffer_t *buf, long *value_p, int bits);
	bool (*encode_long)(userp_buffer_t *buf, long value, int bits);
	bool (*encode_bytes_zerocopy)(userp_buffer_t *buf, char **bytes_p, size_t count);
	bool (*decode_bytes_zerocopy)(userp_buffer_t *buf, char **bytes_p, size_t count);
};


#endif
