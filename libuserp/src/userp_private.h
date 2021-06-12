#ifndef USERP_PRIVATE_H
#define USERP_PRIVATE_H
#include "userp.h"

// -------------------------- userp_env.c ---------------------------

struct userp_diag {
	int code;
	const char *tpl, *cstr1, *cstr2;
	userp_buffer buf;
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
#define USERP_DIAG_BUFADDR_ID       0x09
#define USERP_DIAG_BUFADDR     "\x01\x09"
#define USERP_DIAG_BUFHEX_ID        0x0A
#define USERP_DIAG_BUFHEX      "\x01\x0A"
#define USERP_DIAG_BUFSTR_ID        0x0B
#define USERP_DIAG_BUFSTR      "\x01\x0B"

void userp_diag_set(userp_diag diag, int code, const char *tpl, userp_buffer buf);

struct userp_env {
	userp_alloc_fn *alloc;
	void *alloc_cb_data;
	userp_diag_fn *diag;
	void *diag_cb_data;
	size_t refcnt;
	int run_with_scissors: 1,
		measure_twice: 1,
		log_warn: 1,
		log_debug: 1,
		log_trace: 1;
	
	/* Storage for error conditions */
	struct userp_diag err, // Most recent error
		warn,              // Most recent warning
		msg;               // Current message of less severity than error
};


#define ENV_SET_DIAG_EOF(env) (do { env->diag_code= USERP_EEOF; env->diag_tpl= "End of input"; }while(0))
void fatal_ref_overflow(userp_env env, const char *objtype);

#define SIZET_MUL_CAN_OVERFLOW(a, b) ( \
	( \
		(((size_t)(a) >> sizeof(size_t)*4) + 1) \
		* (((size_t)(b) >> sizeof(size_t)*4) + 1) \
	) \
	>> sizeof(size_t)*4 )

extern bool userp_alloc(userp_env env, void **pointer, size_t elem_size, size_t count, int flags, const char * elem_name);
#define USERP_ALLOC(env, ptr, count, flags) userp_alloc(env, (void**) ptr, 1, count, flags, #ptr + 1)
#define USERP_ALLOC_STRUCT(env, ptr) userp_alloc(env, (void**) ptr, sizeof(*ptr), 1, USERP_HINT_STATIC, #ptr)
#define USERP_ALLOC_ARRAY(env, ptr, elemtype, count, flags) userp_alloc(env, (void**) ptr, sizeof(elemtype), count, flags, #elemtype)
#define USERP_FREE(env, ptr) userp_alloc(env, (void**) ptr, 0, 0, 0, NULL);

#ifndef USERP_BSTR_PART_ALLOC_ROUND
#define USERP_BSTR_PART_ALLOC_ROUND(x) (((x) + 8 + 15) & 15)
#endif
#ifndef USERP_BUFFER_DATA_ALLOC_ROUND
#define USERP_BUFFER_PART_ALLOC_ROUND(x) (((x) + 4095) & 4095)
#endif

extern bool userp_alloc_default(void *callback_data, void **pointer, size_t new_size, int flags);
extern void userp_diag_default(void *callback_data, int diag_code, userp_env env);

// -------------------------- userp_buffer.c ------------------------


// -------------------------- userp_scope.c --------------------------

#define SYMBOL_SCOPE_BITS  (sizeof(userp_symbol)*8/3)
#define SYMBOL_OFS_BITS    (sizeof(userp_symbol)*8 - SYMBOL_SCOPE_BITS)
#define SYMBOL_SCOPE_LIMIT (1 << SYMBOL_SCOPE_BITS)
#define SYMBOL_OFS_LIMIT   (1 << SYMBOL_OFS_BITS)

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
	struct userp_bstring value;
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

struct symbol_table {
	int count, n_alloc;
	struct userp_bstring symbol_text;
	size_t symbol_end[];
};
typedef struct symbol_table *symbol_table;

struct type_table {
	int count, n_alloc;
	userp_type types[];
};
typedef struct type_table *type_table;

struct userp_scope {
	userp_env env;
	userp_scope parent;
	size_t level, refcnt;
	bool is_final;

	symbol_table symtable, *symtable_stack;
	size_t symtable_count;
	type_table typetable, *typetable_stack;
	size_t typetable_count;
};

userp_symbol userp_scope_add_symbol(userp_scope scope, const char *name);

// -------------------------- userp_dec.c ---------------------------

struct userp_dec {
	userp_env env;
	userp_scope scope;
	struct userp_dec_frame *stack;
	size_t stack_i, stack_lim;
	userp_bstrings input;
	userp_reader_fn *reader;
	void * reader_cb_data;
};

#define CATCH(label) if (0) label: 

#endif /* USERP_PROTECTED_H */
