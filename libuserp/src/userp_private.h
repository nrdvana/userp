#ifndef USERP_PRIVATE_H
#define USERP_PRIVATE_H
#include "userp.h"

// ----------------------------- diag.c --------------------------------------

struct userp_diag {
	int code;
	const char *tpl, *cstr1, *cstr2;
	void *ptr;
	char buffer[64];
	int align, index;
	size_t pos, pos2,
		len, len2,
		size, size2,
		count, count2;
};
#define USERP_DIAG_ALIGN_ID         0x01
#define USERP_DIAG_ALIGN       "\x01\x01"
#define USERP_DIAG_POS_ID           0x02
#define USERP_DIAG_POS         "\x01\x02"
#define USERP_DIAG_POS2_ID          0x03
#define USERP_DIAG_POS2        "\x01\x03"
#define USERP_DIAG_LEN_ID           0x04
#define USERP_DIAG_LEN         "\x01\x04"
#define USERP_DIAG_LEN2_ID          0x05
#define USERP_DIAG_LEN2        "\x01\x05"
#define USERP_DIAG_SIZE_ID          0x06
#define USERP_DIAG_SIZE        "\x01\x06"
#define USERP_DIAG_SIZE2_ID         0x07
#define USERP_DIAG_SIZE2       "\x01\x07"
#define USERP_DIAG_INDEX_ID         0x08
#define USERP_DIAG_INDEX       "\x01\x08"
#define USERP_DIAG_COUNT_ID         0x09
#define USERP_DIAG_COUNT       "\x01\x09"
#define USERP_DIAG_COUNT2_ID        0x0A
#define USERP_DIAG_COUNT2      "\x01\x0A"
#define USERP_DIAG_CSTR1_ID         0x0B
#define USERP_DIAG_CSTR1       "\x01\x0B"
#define USERP_DIAG_CSTR2_ID         0x0C
#define USERP_DIAG_CSTR2       "\x01\x0C"
#define USERP_DIAG_BUFSTR_ID        0x0D
#define USERP_DIAG_BUFSTR      "\x01\x0D"
#define USERP_DIAG_BUFHEX_ID        0x0E
#define USERP_DIAG_BUFHEX      "\x01\x0E"
#define USERP_DIAG_PTR_ID           0x0F
#define USERP_DIAG_PTR         "\x01\x0F"

void userp_diag_set(userp_diag diag, int code, const char *tpl);
void userp_diag_setf(userp_diag diag, int code, const char *tpl, ...);

// ------------------------------ env.c --------------------------------------

#ifndef USERP_DEFAULT_SCOPE_STACK_MAX
#define USERP_DEFAULT_SCOPE_STACK_MAX 256
#endif
#ifndef USERP_DEFAULT_ENC_OUTPUT_PARTS
#define USERP_DEFAULT_ENC_OUTPUT_PARTS 8
#endif
#ifndef USERP_DEFAULT_ENC_OUTPUT_BUFSIZE
#define USERP_DEFAULT_ENC_OUTPUT_BUFSIZE 4096
#endif

struct userp_env {
	userp_alloc_fn *alloc;
	void *alloc_cb_data;
	userp_diag_fn *diag;
	void *diag_cb_data;
	unsigned refcnt;
	int run_with_scissors: 1,
		measure_twice: 1,
		log_warn: 1,
		log_info: 1,
		log_debug: 1,
		log_trace: 1;
	
	/* Storage for error conditions */
	struct userp_diag
		err, // Most recent error
		msg; // Most recent non-error message
	
	// Defaults values
	int scope_stack_max;
	int enc_output_parts;
	int enc_output_bufsize;
	int salt;
};

#define USERP_DISPATCH_ERR(env) ((env)->diag((env)->diag_cb_data, &((env)->err),  (env)->err.code))
#define USERP_DISPATCH_MSG(env) ((env)->diag((env)->diag_cb_data, &((env)->msg),  (env)->msg.code))

#define SIZET_MUL_CAN_OVERFLOW(a, b) ( \
	( \
		(((size_t)(a) >> sizeof(size_t)*4) + 1) \
		* (((size_t)(b) >> sizeof(size_t)*4) + 1) \
	) \
	>> sizeof(size_t)*4 )

#if 0
#define USERP_ALLOC_OBJ(env, ptr) fprintf(stderr, "alloc_obj %s at %s %d\n", #ptr, __FILE__, __LINE__), userp_alloc(env, (void**)ptr, sizeof(**ptr), USERP_HINT_STATIC)
#define USERP_ALLOC_OBJPLUS(env, ptr, extra_bytes) fprintf(stderr, "alloc_objplus %s + %d at %s %d\n", #ptr, (int)extra_bytes, __FILE__, __LINE__), userp_alloc(env, (void**)ptr, sizeof(**ptr) + extra_bytes, USERP_HINT_STATIC)
#define USERP_ALLOC_ARRAY(env, ptr, count) fprintf(stderr, "alloc_array %s[%d] at %s %d\n", #ptr, (int)count, __FILE__, __LINE__), userp_alloc(env, (void**)ptr, sizeof(**ptr) * count, USERP_HINT_DYNAMIC)
#define USERP_FREE(env, ptr) fprintf(stderr, "free %s at %s %d", #ptr, __FILE__, __LINE__), userp_alloc(env, (void**) ptr, 0, 0)
#else
#define USERP_ALLOC_OBJ(env, ptr) userp_alloc(env, (void**)ptr, sizeof(**ptr), USERP_HINT_STATIC)
#define USERP_ALLOC_OBJPLUS(env, ptr, extra_bytes) userp_alloc(env, (void**)ptr, sizeof(**ptr) + extra_bytes, USERP_HINT_STATIC)
#define USERP_ALLOC_ARRAY(env, ptr, count) userp_alloc(env, (void**)ptr, sizeof(**ptr) * count, USERP_HINT_DYNAMIC)
#define USERP_FREE(env, ptr) userp_alloc(env, (void**) ptr, 0, 0);
#endif

#ifndef USERP_BUFFER_DATA_ALLOC_ROUND
#define USERP_BUFFER_DATA_ALLOC_ROUND(x) (((x) + 4095) & ~4095)
#endif

// -------------------------------- bstr.c -----------------------------------

#define USERP_SIZEOF_BSTR(n_parts) (sizeof(struct userp_bstr) + sizeof(struct userp_bstr_part)*n_parts)

#ifndef USERP_BSTR_PART_ALLOC_ROUND
#define USERP_BSTR_PART_ALLOC_ROUND(x) (((x) + 8 + 15) & ~(size_t)15)
#endif

// -------------------------- userp_scope.c --------------------------

struct symbol_entry {
	const char *name;
	userp_type type_ref;
	userp_symbol canonical;
	uint32_t hash;
};

struct userp_symtable {
	struct symbol_entry *symbols; // an array pointing to each symbol.  Slot 0 is always empty.
	struct userp_bstr chardata;   // stores all buffers used by the symbols
	size_t used,                  // number of symbols[] occupied (including null symbol at [0])
		alloc,                    // number of symbols[] allocated
		processed;                // number of symbols which have been added to the hashtree
	userp_symbol id_offset;       // difference between userp_symbol value and symbols[] index
	void *buckets;                // hash table
	void *nodes;                  // tree nodes, forming R/B trees for each hash collision
	size_t
		bucket_alloc,             // number of hash buckets
		bucket_used,              // number of hash buckets occupied
		hash_salt,                // salt value to randomize hash layout
		node_alloc,               // number of allocated tree nodes (size depends on 'used')
		node_used;                // number of tree nodes holding collisions
};

struct type_entry {
	userp_symbol name;
	int32_t typeclass;
	void *typeobj;
};

struct userp_typetable {
	struct type_entry *types;     // an array of type entries
	struct userp_bstr typeobjects;// type definition structs allocated back-to-back in buffers
	struct userp_bstr typedata;   // encoded type definitions packed in buffers
	size_t used, alloc;           // number of types in .types[], and number of slots allocated.
	userp_type id_offset;         // ID of types[0]
};

#define TYPE_CLASS_ANY      1
#define TYPE_CLASS_INT      2
#define TYPE_CLASS_SYM      3
#define TYPE_CLASS_TYPE     4
#define TYPE_CLASS_CHOICE   5
#define TYPE_CLASS_ARRAY    6
#define TYPE_CLASS_RECORD   7

struct named_int {
	userp_symbol name;
	intmax_t value;
};
struct userp_type_int {
	int align;
	int pad;
	bool has_min:1, has_max:1, has_bits:1, has_bswap:1;
	intmax_t min, max;
	int bits, bswap;
	int name_count;
	struct named_int names[];
};

struct choice_option {
	userp_type type;
	bool is_value: 1;
	intmax_t merge_ofs, merge_count;
	struct userp_bstr value;
};
struct userp_type_choice {
	int align;
	int pad;
	size_t option_count;
	struct choice_option options[];
};

struct userp_type_array {
	int align;
	int pad;
	userp_type elem_type;
	userp_type dim_type;
	size_t dimension_count;
	size_t dimensions[];
};

struct record_field {
	userp_symbol name;
	userp_type type;
	int placement;
};
struct userp_type_record {
	int align;
	int pad;
	size_t static_bits;
	userp_type other_field_type;
	size_t field_count;
	struct record_field fields[];
};

struct userp_scope {
	userp_env env;
	userp_scope parent;
	size_t level;
	unsigned refcnt;
	bool is_final:1,
		has_symbols:1,
		has_types:1;

	size_t symtable_count;
	size_t typetable_count;
	struct userp_symtable symtable, 
		**symtable_stack;
	struct userp_typetable typetable,
		*typetable_stack[];
};

userp_symbol userp_scope_add_symbol(userp_scope scope, const char *name);

// ----------------------------- enc.c -------------------------------

struct userp_enc {
	userp_env env;
	userp_scope scope;
	struct userp_bstr output;
	uint8_t *out_pos, *out_lim;
	int out_align;
	
	struct userp_bstr_part output_initial_parts[];
};

// ----------------------------- dec.c -------------------------------

struct userp_dec {
	userp_env env;
	userp_scope scope;
	struct userp_dec_frame *stack;
	unsigned refcnt;
	size_t stack_i, stack_lim;
	struct userp_bstr input;
	userp_reader_fn *reader;
	void * reader_cb_data;
	// The struct ends with a userp_bstr instance, allocated to a default
	// length specified in the environment.  As long as the input buffers
	// can fit in this bstr, ->input doesn't need a second allocation.
	struct userp_bstr input_inst;
};

struct userp_dec_frame {
	int frame_type;
	userp_type node_type, parent_type;
	size_t elem_i, elem_lim;
	
	userp_type array_type, rec_type;
};

extern userp_dec userp_new_dec_silent(
	userp_env env, userp_scope scope, userp_type root_type,
	userp_buffer buffer_ref, uint8_t *bytes, size_t n_bytes
);
extern bool userp_grab_dec_silent(userp_env env, userp_dec dec);
extern void userp_drop_dec_silent(userp_env env, userp_dec dec);

static inline size_t roundup_pow2(size_t s) {
	--s;
	s |= s >> 1;
	s |= s >> 2;
	s |= s >> 4;
	s |= s >> 8;
	s |= s >> 16;
	#if SIZE_MAX > 0xFFFFFFFF
	s |= s >> 32;
	#endif
	return s+1;
}

#define FRAME_TYPE_RECORD  1
#define FRAME_TYPE_ARRAY   2
#define FRAME_TYPE_CHOICE  3
#define FRAME_TYPE_INT     4
#define FRAME_TYPE_SYM     5
#define FRAME_TYPE_TYPE    6

#define FRAME_BY_PARENT_LEVEL(dec, parent_level) \
	parent_level < 0? ( -1-parent_level <= dec->stack_i? dec->stack[-1-parent_level] : NULL ) \
	: ( parent_level <= dec->stack_i? dec->stack[dec->stack_i - parent_level] : NULL )

#ifndef USERP_DEC_FRAME_ALLOC_ROUND
#define USERP_DEC_FRAME_ALLOC_ROUND(x) (((x) + 31) & ~15)
#endif


#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

#define CATCH(label) if (0) label: 

static inline void unimplemented(const char* msg) {
	printf("unimplemented: %s", msg);
	abort();
}

#ifdef UNIT_TEST
static bool logging_alloc(void *callback_data, void **pointer, size_t new_size, userp_alloc_flags flags);
static void dump_scope(userp_scope scope);
#endif

#endif /* USERP_PRIVATE_H */
