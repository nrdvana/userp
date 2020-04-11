#ifndef USERP_H
#define USERP_H

typedef struct userp_env userp_env_t, *userp_env_p;
typedef struct userp_context userp_t, *userp_p;
typedef struct userp_scope userp_scope_t, *userp_scope_p;
typedef struct userp_type userp_type_t, *userp_type_p;
typedef struct userp_enc userp_enc_t, *userp_enc_p;
typedef struct userp_dec userp_dec_t, *userp_dec_p;

typedef bool (*userp_alloc_fp)(void *callback_data, void **pointer, size_t new_size, int align_pow2_bits);
typedef bool (*userp_diag_fp)(void *callback_data, int diag_code, userp_env_t *env);
extern userp_env_p userp_env_new(userp_alloc_fp alloc_callback, userp_log_fp diag_callback, void *callback_data);

#define USERP_DIAG_IS_ERR(code)  (!(code>>14))
#define USERP_DIAG_IS_WARN(code) ((code>>13) == 2)
#define USERP_DIAG_IS_DEBUG(code) ((code>>13) == 3)

#define USERP_DIAG_MAJOR(code) (code & 0x3F00)
#define USERP_DIAG_OS          0x0100
#define USERP_DIAG_OS_MALLOC   0x0101
#define USERP_DIAG_EOF         0x0200
#define USERP_DIAG_ARG         0x0300
/* TODO */

extern userp_env_last_error(userp_env_p ctx, int **code, const char **static_msg, char *diag_buf, int *diag_buflen);
extern userp_last_error(userp_p ctx, int **code, const char **static_msg, char *diag_buf, int *diag_buflen);

/* userp_context.c */

extern userp_p userp_new_stream_writer(userp_env_p env, int ver_maj, int ver_min, int options);
extern userp_p userp_new_stream_reader(userp_env_p env);
#define USERP_STREAM_LE  0x0000
#define USERP_STREAM_BE  0x0001

// Implied-read decoding API
extern bool userp_stream_set_fd(userp_p stream, int fd, int options);
#define USERP_STREAM_NONBLOCK
#define USERP_STREAM_SEEKABLE
extern userp_p userp_stream_next_bock(userp_p context);

// Self-managed buffers decoding API
extern bool userp_stream_set_buffer(userp_p stream, invoid *buffer, size_t len);

extern userp_p userp_new_block(userp_p stream, int block_type);

/* userp_block.c */

extern userp_scope_p userp_block_scope(userp_block_p block);

/* userp_scope.c */

userp_type_p userp_scope_builtin_type(userp_scope_p scope, int type_id);

#define USERP_TYPE_INT
#define USERP_TYPE_UINT
#define USERP_TYPE_INT8
#define USERP_TYPE_UINT8
#define USERP_TYPE_INT16
#define USERP_TYPE_UINT16
#define USERP_TYPE_INT32
#define USERP_TYPE_UINT32
#define USERP_TYPE_INT64
#define USERP_TYPE_UINT64

/* userp_type.c */



/* userp_enc.c */

/* userp_dec.c */

/* Create a new UserpBlock_t to parse the given data buffer */
extern UserpBlock_t *userp_parse_block(UserpState_t *state, int flags, const void *blockdata, size_t blocklen);
/* Create a new UserpBlock_t to begin writing a new block, possibly using a user-supplied buffer */
extern UserpBlock_t *userp_begin_block(UserpState_t *state, int flags, void *buffer, size_t bufferlen);
/* Destroy a block once it is no longer needed.  This can fail if the block is in use. */
extern bool userp_destroy_block(UserpBlock_t *block);

/* Declare an identifier in the given metadata block.  Useful for pre-populating the ident cache */
extern int userp_meta_declare_ident(UserpBlock_t *block, const char *name);
/* Declare a new type in the given metadata block. */
extern int userp_meta_new_type(UserpBlock_t *block, const char *name, const char *typespec, int tag_count, const char **tags);

/* Look up a type by name, optionally verifying it against a type of the same name in another meta block. */
#define userp_type_by_name(block,name) userp_type_by_name_matching(block,name,NULL)
extern int userp_type_by_name_matching(UserpBlock_t *block, const char *name, UserpBlock_t *assert_match);

/* Get the name of a type ID in the scope of the given block (data or metadata) */
extern const char* userp_meta_get_typename(UserpBlock_t *block, int type_id);
/* Get the specification for a type in the scope of the given block.
 * Can also return the type_id of each referenced type.
 * Can also return the application-specific tags.
 */
extern int userp_meta_get_typespec(UserpBlock_t *block, int type_id,
	char *spec_buf, size_t buf_len, /* buffer to receive specification string.  size must include room for NUL */
	size_t *type_id_count, const int **type_id, /* list of type IDs. */
	size_t *tag_count, const char * const **tags_buf, /* list of tags */
);

/* Block-Read API */

extern int userp_node_info(UserpBlock_t *block, int *node_type, int *seq_elems, int *seq_elem_type);
extern int userp_enter_node(UserpBlock_t *block);
extern int userp_elem_seek(UserpBlock_t *block, size_t elem_idx);
extern int userp_elem_seek_ident(UserpBlock_t *block, int ident);
extern int userp_elem_seek_name(UserpBlock_t *block, const char *ident_name);
extern int userp_exit_node(UserpBlock_t *block);

extern int userp_read_int(UserpBlock_t *block, int *node_type, int *node_value);
extern int userp_read_longlong(UserpBlock_t *block, int *node_type, long long *node_value);
extern int userp_read_float(UserpBlock_t *data, int *node_type, float *node_value);
extern int userp_read_double(UserpBlock_t *data, int *node_type, double *node_value);
extern int userp_read_data(UserpBlock_t *block, int *node_type, void *node_value, size_t *node_value_len);
extern int userp_read_nocopy(UserpBlock_t *block, int *node_type, const void **node_ptr, size_t *node_value_len);

/* Block-Write API */

extern int userp_begin_node(UserpBlock_t *block, int type_id, size_t elem_count);
extern int userp_end_node(UserpBlock_t *block);

extern int userp_write_int(UserpBlock_t *block, int type_id, int value);
extern int userp_write_longlong(UserpBlock_t *block, int type_id, long long value);
extern int userp_write_float(UserpBlock_t *block, int type_id, float value);
extern int userp_write_double(UserpBlock_t *block, int type_id, double value);
extern int userp_write_data(UserpBlock_t *block, int type_id, void *value, size_t value_len);
extern int userp_write_get_address(UserpBlock_t *block, int type_id, void **node_ptr, size_t *buf_len);
extern int userp_write_nocopy(UserpBlock_t *block, int type_id, size_t buf_len);

#endif
