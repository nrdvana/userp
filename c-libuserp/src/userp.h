#ifndef USERP_H
#define USERP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct userp_env *userp_env;
typedef struct userp_diag *userp_diag;
typedef struct userp_scope *userp_scope;
typedef uint32_t userp_symbol;
typedef uint32_t userp_type;
typedef struct userp_enc *userp_enc;
typedef struct userp_dec *userp_dec;
typedef struct userp_buffer *userp_buffer;
typedef uint_least32_t userp_env_flags, userp_alloc_flags, userp_buffer_flags;
struct userp_bstr;

// ----------------------------- diag.c --------------------------------------

#define USERP_IS_FATAL(code) ((code>>13) == 3)
#define USERP_IS_ERROR(code) ((code>>13) == 2)
#define USERP_IS_WARN(code)  ((code>>13) == 1)
#define USERP_IS_DEBUG(code) ((code>>13) == 0)

// Fatal errors (unlikely to be able to recover)
#define USERP_EFATAL        0x6000 // Generic fatal error
#define USERP_EBADSTATE     0x6001 // A userp object is in an invalid state / double-free / use-after-free
// Regular errors (recoverable if caller watches return value)
#define USERP_ERROR         0x4000 // Generic recoverable error
#define USERP_EALLOC        0x4001 // failure to alloc or realloc or grab a reference
#define USERP_EDOINGITWRONG 0x4002 // bad request made to the API, or exceed library internal limitation
#define USERP_ESCOPEFINAL   0x4003 // attempt to modify scope after finalized
#define USERP_EFOREIGNSCOPE 0x4004 // scope belongs to a different env
#define USERP_EUNKNOWN      0x4005 // Unknown enum value passed to API
#define USERP_ETYPESCOPE    0x4006 // userp_type is not available in the current scope
#define USERP_ESYS          0x4007 // You asked libuserp to make a system call, and the system call failed
#define USERP_EPROTOCOL     0x4100 // Generic error while decoding protocol
#define USERP_EOVERRUN      0x4101 // The protocol describes data larger than its container
#define USERP_EFEEDME       0x4102 // More data required to continue decoding
#define USERP_ELIMIT        0x4103 // decoded data exceeds a limit
#define USERP_ESYMBOL       0x4104 // symbol table entry is not valid
#define USERP_ETYPE         0x4105 // type definition is not valid
#define USERP_ERECORD       0x4106 // record fields declaration is invalid
#define USERP_EBUFPOINTER   0x4107 // specified pointer is not within specified buffer
// Warnings
#define USERP_WARN          0x2000 // generic warning
#define USERP_WLARGEMETA    0x2001 // encoded or decoded metadata is suspiciously large
// Diagnostics
#define USERP_MSG_SYMTABLE_HASHTREE_ALLOC   0x0001
#define USERP_MSG_SYMTABLE_HASHTREE_EXTEND  0x0002
#define USERP_MSG_SYMTABLE_HASHTREE_UPDATE  0x0002
#define USERP_MSG_SYMTABLE_HASHTREE_REBUILD 0x0003
#define USERP_MSG_CREATE                    0x0004
#define USERP_MSG_DESTROY                   0x0005

extern int    userp_diag_get_code(userp_diag diag);
extern bool   userp_diag_get_buffer(userp_diag diag, userp_buffer *buf_p, size_t *pos_p, size_t *len_p);
extern int    userp_diag_get_index(userp_diag diag);
extern size_t userp_diag_get_size(userp_diag diag);
extern size_t userp_diag_get_count(userp_diag diag);
extern const char *userp_diag_code_name(int code);
extern int    userp_diag_format(userp_diag diag, char *buf, size_t buflen);
extern int    userp_diag_print(userp_diag diag, FILE *fh);

// ------------------------------ env.c --------------------------------------

#define USERP_HINT_STATIC             0x0001
#define USERP_HINT_DYNAMIC            0x0002
#define USERP_HINT_BRIEF              0x0004
#define USERP_HINT_PERSIST            0x0008
#define USERP_ALLOC_ALIGN_SIZET       0x0010
#define USERP_ALLOC_ALIGN_INTMAX      0x0020
#define USERP_ALLOC_ALIGN_PAGE        0x0030
#define USERP_POINTER_IS_BUFFER_DATA  0x0100
#define USERP_ALLOC_FLAG_MASK         0x011F

typedef bool userp_alloc_fn(void *callback_data, void **pointer, size_t new_size, userp_alloc_flags flags);
typedef void userp_diag_fn(void *callback_data, userp_diag diag, int diag_code);

extern bool userp_alloc(userp_env env, void **pointer, size_t new_size, userp_alloc_flags flags);

extern userp_env userp_new_env(userp_alloc_fn alloc_callback, userp_diag_fn diag_callback, void *callback_data, userp_env_flags flags);
extern bool userp_grab_env(userp_env env);
extern bool userp_drop_env(userp_env env);


#define USERP_LOG_LEVEL               0x0001

#define USERP_DEFAULT                      0
#define USERP_LOG_ERROR                    1
#define USERP_LOG_WARN                     2
#define USERP_LOG_DEBUG                    3
#define USERP_LOG_TRACE                    4

#define USERP_SAFETY                  0x0002

#define USERP_MEASURE_TWICE                1
#define USERP_RUN_WITH_SCISSORS            2
#define USERP_TRUNCATE_INTO_INT            3
#define USERP_TRUNCATE_INTO_FLOAT          4

void userp_env_set_attr(userp_env env, int attr_id, size_t value);

extern void userp_file_logger(void *callback_data, userp_diag diag, int code);
extern void userp_env_set_logger(userp_env env, userp_diag_fn diag_callback, void *callback_data);
extern userp_diag userp_env_get_last_error(userp_env env);

// ------------------------------- buf.c -------------------------------------

// buf->data was allocated with env's alloc, needs freed, and can be re-allocated.
#define USERP_BUFFER_DATA_ALLOC    0x001000

// buf->data was allocated with mmap, and needs freed with munmap
#define USERP_BUFFER_DATA_MMAP     0x002000

// buffer can have new data appended up to the alloc_len
#define USERP_BUFFER_APPENDABLE    0x004000

// Need to copy data into a single span of memory / bstr part
#define USERP_CONTIGUOUS           0x008000

typedef bool userp_reader_fn(void *callback_data, struct userp_bstr* buffers, size_t bytes_needed, userp_env env);
typedef void userp_buffer_destructor(void *callback_data, userp_buffer *buf);

/** Reference-counted buffer wrapper
 *
 * userp_buffer is a thin wrapper around a pointer to a region of memory.
 * It has an optional reference count, and tracks whether the buffer was dynamically
 * allocated and at what size.  Several top-level userp objects may share pointers
 * to the userp_buffer, with the last one to use it calling free() as needed.
 *
 * Semantics:
 *  - If alloc_len is zero, then the data pointer is an unknown origin and should
 *    not be resized or freed.
 *  - If refcnt is zero, then the lifespan of the userp_buffer instance is not being
 *    tracked, and nothing should increment or decrement the count, and also not
 *    attempt to free it.
 *  - Else if lifespan is tracked: if `env` is NULL the userp_buffer has been
 *    allocated with malloc and needs freed with free().  If `env` is set, it means
 *    the struct was allocated from the allocator function of the userp env and
 *    needs resized or freed using that.  It also means the userp_buffer is holding
 *    a reference count on the env object.
 *
 * Examples:
 *
 *   // Dynamically allocate a new buffer associated with a userp environment
 *   userp_buffer buf= userp_new_buffer(env, NULL, 4096);
 *   userp_drop_buffer(buf); // calls env->alloc_fn to free buf->data and buf
 *   
 *   // Wrap a static buffer having a longer lifespan than the userp instance.
 *   // You must MAKE SURE that both static_buf and buf are still valid for as
 *   // long as any userp_env or related object might reference them.
 *   char static_buf[4096];
 *   struct userp_buffer buf;
 *   bzero(&buf, sizeof(buf));
 *   buf.data= static_buf;
 *   userp_drop_buffer(&buf); // no-op
 *   
 *   // Wrap a static buffer with a dynamic userp_buffer instance
 *   char static_buf[4096];
 *   userp_buffer buf= userp_new_buffer(env, static_buf, 0); // 0 means static
 *   userp_drop_buffer(&buf); // calls env->alloc_fn to free buf, does not free buf->data
 *
 *   // Wrap a buffer that you allocated with malloc, and rely on userp to free it
 *   char * buf= (char*) malloc(4096);
 *   userp_buffer buf= userp_new_buffer(NULL, buf, 4096);
 *   userp_drop_buffer(buf); // calls free(buf->data); free(buf);
 */
struct userp_buffer {
	uint8_t *                data;      // points to start of buffer
	userp_env                env;       // owner of the buffer
	size_t                   refcnt;    // if nonzero, this buffer is reference-counted
	size_t                   alloc_len; // if nonzero, how much memory is valid starting at 'data'
	userp_buffer_flags       flags;
};
#define USERP_FIELD_OFFSET(type,field) ((int)( ((int)&(((type*)64)->field)) - 64 ))
#define USERP_GET_BUFFER_FROM_DATA_PTR(data_ptr) ((userp_buffer)( ((char*)data_ptr) - USERP_FIELD_OFFSET(struct userp_buffer, data) ))

extern userp_buffer userp_new_buffer(userp_env env, void *data, size_t alloc_len, userp_buffer_flags flags);
extern bool userp_grab_buffer(userp_buffer buf);
extern bool userp_drop_buffer(userp_buffer buf);

// ------------------------------- bstr.c ------------------------------------

struct userp_bstr_part {
	uint8_t *data;
	userp_buffer buf;
	size_t ofs, len;
};
struct userp_bstr {
	size_t part_count, part_alloc;
	struct userp_bstr_part *parts;
	userp_env env;
};

extern bool userp_bstr_partalloc(struct userp_bstr *str, size_t part_count);
extern uint8_t* userp_bstr_append_bytes(struct userp_bstr *ptr, const uint8_t *bytes, size_t n, int flags);
extern struct userp_bstr_part* userp_bstr_append_parts(struct userp_bstr *ptr, const struct userp_bstr_part *parts, size_t n);
static inline void userp_bstr_init(struct userp_bstr *str, userp_env env) { str->part_count= str->part_alloc= 0; str->parts= NULL; str->env= env; }
static inline void userp_bstr_destroy(struct userp_bstr *str) { userp_bstr_partalloc(str, 0); }

//extern userp_bstr userp_new_bstr(userp_env env, int part_alloc_count);
//extern void userp_free_bstr(userp_bstr *str);
//extern bool userp_bstr_append_stream(userp_bstr *str, FILE *f, int64_t length);
//extern bool userp_bstr_map_file(userp_bstr *str, FILE *f, int64_t offset, int64_t length);
//extern bool userp_bstr_splice(userp_bstr *dst, size_t dst_ofs, size_t dst_len, userp_bstr *src, size_t src_ofs, size_t src_len);
//extern void userp_bstr_crop(userp_bstr *str, size_t trim_head, size_t trim_tail);

// ------------------------------ scope.c ------------------------------------

#define USERP_GET_LOCAL      1
#define USERP_CREATE         2
#define USERP_LAZY           4

extern userp_scope userp_new_scope(userp_env env, userp_scope parent);
extern bool userp_grab_scope(userp_scope scope);
extern bool userp_drop_scope(userp_scope scope);

extern bool userp_scope_reserve(userp_scope scope, size_t min_symbols, size_t min_types);

extern userp_symbol userp_scope_get_symbol(userp_scope scope, const char * name, int flags);
extern const char * userp_scope_get_symbol_str(userp_scope scope, userp_symbol sym);

#define USERP_TYPECLASS_ANY     1
#define USERP_TYPECLASS_TYPEREF 2
#define USERP_TYPECLASS_SYMREF  3
#define USERP_TYPECLASS_INTEGER 4
#define USERP_TYPECLASS_CHOICE  5
#define USERP_TYPECLASS_ARRAY   6
#define USERP_TYPECLASS_RECORD  7

extern userp_type userp_scope_get_type(userp_scope scope, userp_symbol name, int flags);
extern userp_type userp_scope_new_type(userp_scope scope, userp_symbol name, userp_type base_type);
extern bool userp_scope_contains_type(userp_scope scope, userp_type type);
extern userp_enc userp_type_encode(userp_type type);
extern userp_dec userp_type_decode(userp_type type);

extern bool userp_scope_import(userp_scope scope, userp_scope source, int flags);

extern userp_symbol userp_scope_resolve_relative_symref(userp_scope scope, size_t val);
extern userp_type   userp_scope_resolve_relative_typeref(userp_scope scope, size_t val);

extern bool userp_scope_finalize(userp_scope scope, int flags);

/* Stream API */
extern const userp_scope userp_stream1_scope;       /* The global scope object for version 1 of the stream protocol */
extern const userp_type userp_stream1_Any;          /* Any type currently in scope */
extern const userp_type userp_stream1_Symbol;       /* Reference to a symbol currently in scope */
extern const userp_type userp_stream1_Type;         /* Reference to a type currently in scope */
extern const userp_type userp_stream1_Int;          /* Any integer of any bit length */
extern const userp_type userp_stream1_IntU;         /* Zero-or-positive integer of any bit length */
extern const userp_type userp_stream1_Matrix;       /* Multi-dimensional array, indeterminate dimensions */
extern const userp_type userp_stream1_Array;        /* Single-dimension array */
extern const userp_type userp_stream1_Record;       /* Record of arbitrary fields */
extern const userp_type userp_stream1_Bit;          /* Single bit */
extern const userp_type userp_stream1_Byte;         /* Single octet */
extern const userp_type userp_stream1_ByteArray;    /* Array of octets */
extern const userp_type userp_stream1_String;       /* Array of octets which will also be printable UTF-8 */
extern const userp_type userp_stream1_Int16;        /* 16-bit little-endian integer */
extern const userp_type userp_stream1_Int16U;       /* 16-bit little-endian unsigned integer */
extern const userp_type userp_stream1_Int32;        /* 32-bit little-endian integer */
extern const userp_type userp_stream1_Int32U;       /* 32-bit little-endian unsigned integer */
extern const userp_type userp_stream1_Int64;        /* 64-bit little-endian integer */
extern const userp_type userp_stream1_Int64U;       /* 64-bit little-endian unsigned integer */

//bool          userp_create_stream(userp_env env, int version);
//bool          userp_parse_stream(userp_env env, userp_bstrings);
//userp_bstrings userp_get_buffers(userp_env env);
//bool          userp_stream_set_header(userp_context stream, const char *name, const char *value);
//const char *  userp_stream_get_header(userp_context stream, const char *name);

void userp_enc_clear_error(userp_enc enc);
bool userp_enc_int(userp_enc enc, int value);
bool userp_enc_symbol(userp_enc enc, userp_symbol sym);
bool userp_enc_typeref(userp_enc enc, userp_type type);
bool userp_enc_select(userp_enc enc, userp_type type);
bool userp_enc_begin_array(userp_enc enc, const size_t *dims, size_t n_dim);
bool userp_enc_end_array(userp_enc enc);
bool userp_enc_begin_record(userp_enc enc, size_t n_field, userp_symbol *fields);
bool userp_enc_field(userp_enc enc, userp_symbol field);
bool userp_enc_end_record(userp_enc enc);
bool userp_enc_float(userp_enc enc, float value);
bool userp_enc_double(userp_enc enc, double value);
bool userp_enc_bytes(userp_enc enc, const void* buf, size_t length);
bool userp_enc_bytes_zerocopy(userp_enc enc, const void* buf, size_t length);
bool userp_enc_string(userp_enc enc, const char* str);

// -------------------------------- dec.c ------------------------------------

#define USERP_DEC_BUFFER_ALIGN 6  /* 2^6 = 64-bit */

/* Node Info lets a user inspect the aspects of a decoded value.
 * This struct is used directly by the library, but only about half of it
 * is made public.  It is presented to the user as a 'const' typedef.
 */
#define USERP_NODEFLAG_INT      0x0001
#define USERP_NODEFLAG_UNSIGNED 0x0002
#define USERP_NODEFLAG_BIGINT   0x0004
#define USERP_NODEFLAG_SYM      0x0008
#define USERP_NODEFLAG_TYPE     0x0010
#define USERP_NODEFLAG_FLOAT    0x0020
#define USERP_NODEFLAG_RATIONAL 0x0040
#define USERP_NODEFLAG_ARRAY    0x0080
#define USERP_NODEFLAG_RECORD   0x0100
struct userp_node_info {
	uint32_t flags;     // indicates which fields are valid.
	userp_type
		value_type,     // the data type of the value decoded for this node
		node_type;      // the data type declared for the node (such as "Any")
	size_t node_depth;  // the number of parent nodes (records or arrays)
	int64_t intval;     // for integer types, holds the value if (flags & USERP_NODEFLAG_INT)
	userp_bstr data;    // for various types, references span of buffer(s) containing data
	size_t array_dim_count;      // for array types, this lists the
	const size_t *array_dims;    //   dimensions of the array (usually just one)
	size_t elem_count;  // For arrays or records, this is the number of elements or fields present
};

typedef const struct userp_node_info userp_node_info;

struct userp_dec;
typedef struct userp_dec userp_dec;

userp_dec userp_new_dec(
	userp_env env, userp_scope scope, userp_type root_type,
	userp_buffer buffer_ref, uint8_t *bytes, size_t n_bytes
);
bool userp_grab_dec(userp_env env, userp_dec dec);
void userp_drop_dec(userp_env env, userp_dec dec);


userp_env userp_dec_env(userp_dec dec);
userp_scope userp_dec_scope(userp_dec dec);
void userp_dec_set_reader(userp_dec dec, userp_reader_fn, void *callback_data);


// Parse the current node if not parsed yet and return a struct describing it
userp_node_info userp_dec_node_info(userp_dec dec);
// Begin iterating the array or record elements of the current node
bool userp_dec_begin(userp_dec dec);
// Stop iterating an array or record and set the current node to the following element, if any
bool userp_dec_end(userp_dec dec);
// Seek to the specified array index or field number of the parent node
bool userp_dec_seek_elem(userp_dec dec, size_t elem_idx);
// Seek to the named field of the most recent parent record node
bool userp_dec_seek_field(userp_dec dec, userp_symbol fieldname);
// Skip the current node and move to the next element in the parent array or record
bool userp_dec_skip(userp_dec dec);
// Decode the current node as an integer, and move to the next node if successful
bool userp_dec_int(userp_dec dec, int *out);
bool userp_dec_int_n(userp_dec dec, void *intbuf, size_t word_size, bool is_signed);
bool userp_dec_bigint(userp_dec dec, void *intbuf, size_t *len, int *sign);
// Decode the current node as a Symbol, and move to the next node if successful
bool userp_dec_symbol(userp_dec dec, userp_symbol *out);
// Decode the current node as a Type reference, and move to the next node if successful
bool userp_dec_typeref(userp_dec dec, userp_type *out);
// Decode the current node as a `float` and move to the next node
bool userp_dec_float(userp_dec dec, float *out);
bool userp_dec_double(userp_dec dec, double *out);
// Copy out the bytes of the current node, as-is
//bool userp_dec_bytes(userp_dec dec, void *out, size_t *length_inout, size_t elem_size, userp_flags flags);
//userp_bstr userp_dec_bytes_zerocopy(userp_dec dec, size_t elem_size, userp_flags flags);

#ifdef __cplusplus
}
#endif

#endif  /* USERP_H */
