#ifndef USERP_PROTECTED_H
#define USERP_PROTECTED_H
#include "userp.h"

// -------------------------- userp_env.c ---------------------------

struct userp_env {
	userp_alloc_fn *alloc;
	void *alloc_cb_data;
	userp_diag_fn *diag;
	void *diag_cb_data;
	int run_with_scissors: 1,
		enable_landmines: 1;
	
	/* Storage for error conditions */
	int diag_code;
	char *diag_tpl;
	void *diag_buf;
	int diag_align;
	size_t diag_pos, diag_len, diag_size, diag_count;
};

#define USERP_DIAG_ALIGN_ID         0x01
#define USERP_DIAG_ALIGN       "\x01\x01"
#define USERP_DIAG_POS_ID           0x02
#define USERP_DIAG_POS         "\x01\x02"
#define USERP_DIAG_LEN_ID           0x03
#define USERP_DIAG_LEN         "\x01\x03"
#define USERP_DIAG_SIZE_ID          0x04
#define USERP_DIAG_SIZE        "\x01\x04"
#define USERP_DIAG_COUNT_ID         0x05
#define USERP_DIAG_COUNT       "\x01\x05"
#define USERP_DIAG_BUFADDR_ID       0x06
#define USERP_DIAG_BUFADDR     "\x01\x06"
#define USERP_DIAG_BUFHEX_ID        0x07
#define USERP_DIAG_BUFHEX      "\x01\x07"
#define USERP_DIAG_BUFSTRZ_ID       0x08
#define USERP_DIAG_BUFSTRZ     "\x01\x08"

#define ENV_SET_DIAG_EOF(env) (do { env->diag_code= USERP_EEOF; env->diag_tpl= "End of input"; }while(0))

#define SIZET_MUL_CAN_OVERFLOW(a, b) ( \
	( \
		(((size_t)(a) >> sizeof(size_t)*4) + 1) \
		* (((size_t)(b) >> sizeof(size_t)*4) + 1) \
	) \
	>> sizeof(size_t)*4 )

extern bool userp_alloc(userp_env env, void **pointer, size_t elem_size, size_t count, int flags, const char * elem_name);
#define USERP_ALLOC(env, ptr, count, flags) userp_alloc(env, (void**) ptr, 1, count, flags #ptr + 1)
#define USERP_ALLOC_ARRAY(env, ptr, elemtype, count, flags) userp_alloc(env, (void**) ptr, sizeof(elemtype), count, flags, #elemtype)
#define USERP_FREE(env, ptr) userp_alloc(env, (void**) ptr, 0, 0, 0, NULL);

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
	size_t symbol_ofs[];
};
typedef struct symbol_table *symbol_table;

struct type_table {
	int count, n_alloc;
	userp_type *types[];
};
typedef struct type_table *type_table;

struct userp_scope {
	userp_env env;
	userp_scope parent;
	unsigned level, refcnt;
	bool is_final;

	symbol_table symtable, *symtable_stack;
	size_t symtable_count;
	type_table typetable, *typetable_stack;
	size_t typetable_count;
};

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


#endif
