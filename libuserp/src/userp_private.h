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
	size_t pos, len, size, count;
};
#define USERP_DIAG_ALIGN_ID         0x01
#define USERP_DIAG_ALIGN       "\x01\x01"
#define USERP_DIAG_POS_ID           0x02
#define USERP_DIAG_POS         "\x01\x02"
#define USERP_DIAG_LEN_ID           0x03
#define USERP_DIAG_LEN         "\x01\x03"
#define USERP_DIAG_SIZE_ID          0x04
#define USERP_DIAG_SIZE        "\x01\x04"
#define USERP_DIAG_INDEX_ID         0x05
#define USERP_DIAG_INDEX       "\x01\x05"
#define USERP_DIAG_COUNT_ID         0x06
#define USERP_DIAG_COUNT       "\x01\x06"
#define USERP_DIAG_CSTR1_ID         0x07
#define USERP_DIAG_CSTR1       "\x01\x07"
#define USERP_DIAG_CSTR2_ID         0x08
#define USERP_DIAG_CSTR2       "\x01\x08"
#define USERP_DIAG_BUFSTR_ID        0x09
#define USERP_DIAG_BUFSTR      "\x01\x09"
#define USERP_DIAG_BUFHEX_ID        0x0A
#define USERP_DIAG_BUFHEX      "\x01\x0A"
#define USERP_DIAG_PTR_ID           0x0B
#define USERP_DIAG_PTR         "\x01\x0B"

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
		log_debug: 1,
		log_trace: 1,
		log_suppress: 1;
	
	/* Storage for error conditions */
	struct userp_diag err, // Most recent error
		warn,              // Most recent warning
		msg;               // Current message of less severity than error
	
	// Defaults values
	int scope_stack_max;
	int enc_output_parts;
	int enc_output_bufsize;
};

#define USERP_CLEAR_ERROR(env) ((env)->err.code= 0)
#define USERP_DISPATCH_ERROR(env) do { if ((env)->err.code) env->diag(env->diag_cb_data, &env->err, env->err.code); } while (0)

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
#define USERP_BSTR_PART_ALLOC_ROUND(x) (((x) + 8 + 15) & ~15)
#endif

// -------------------------- userp_scope.c --------------------------

#define SYMBOL_SCOPE_BITS  (sizeof(userp_symbol)*8/3)
#define SYMBOL_OFS_BITS    (sizeof(userp_symbol)*8 - SYMBOL_SCOPE_BITS)
#define SYMBOL_SCOPE_LIMIT (1 << SYMBOL_SCOPE_BITS)
#define SYMBOL_OFS_LIMIT   (1 << SYMBOL_OFS_BITS)

#ifndef USERP_SCOPE_TABLE_ALLOC_ROUND
#define USERP_SCOPE_TABLE_ALLOC_ROUND(x) (((x) + 255) & ~255)
#endif

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

struct choice_option {
	userp_type type;
	bool is_value: 1;
	intmax_t merge_ofs, merge_count;
	struct userp_bstr value;
};

struct record_field {
	userp_symbol name;
	userp_type type;
	int placement;
};

#define USERP_TYPE_COMMON_FIELDS \
	userp_scope scope;           \
	userp_symbol name;           \
	int type_class;              \
	int align;                   \
	int pad;                     

struct userp_type {
	USERP_TYPE_COMMON_FIELDS
};

struct userp_type_int {
	USERP_TYPE_COMMON_FIELDS
	bool has_min:1, has_max:1, has_bits:1, has_bswap:1;
	intmax_t min, max;
	int bits, bswap;
	int name_count;
	struct named_int names[];
};

struct userp_type_choice {
	USERP_TYPE_COMMON_FIELDS
	size_t option_count;
	struct choice_option options[];
};

struct userp_type_array {
	USERP_TYPE_COMMON_FIELDS
	userp_type elem_type;
	userp_type dim_type;
	size_t dimension_count;
	size_t dimensions[];
};

struct userp_type_record {
	USERP_TYPE_COMMON_FIELDS
	size_t static_bits;
	userp_type other_field_type;
	size_t field_count;
	struct record_field fields[];
};

struct type_table {
	userp_type *types;
	userp_type id_offset;
	int used, alloc;
};

struct symbol_entry {
	const char *name;
	userp_type type_ref;
};
struct symbol_tree_node31 {
	uint32_t left, right:31, color:1;
};
struct symbol_tree_node15 {
	uint16_t left, right:15, color:1;
};

struct symbol_table {
	struct symbol_entry *symbols; // an array pointing to each symbol.  Slot 0 is always empty.
	size_t used, alloc;
	userp_symbol id_offset;
	void *tree;                   // an optional red/black tree sorting the symbols
	int tree_root;
	struct userp_bstr chardata;   // stores all buffers used by the symbols
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
	struct symbol_table symtable, 
		**symtable_stack;
	struct type_table typetable,
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

#define CATCH(label) if (0) label: 

void userp_unimplemented(const char* msg);
#define unimplemented(x) userp_unimplemented(x)

#endif /* USERP_PROTECTED_H */
