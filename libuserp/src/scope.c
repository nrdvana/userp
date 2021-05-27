#include "local.h"
#include "userp.h"
#include "userp_private.h"

static bool realloc_symtable(userp_env env, symbol_table *symtable, size_t count);
static bool realloc_typetable(userp_env env, type_table *typetable, size_t count);

userp_scope userp_new_scope(userp_env env, userp_scope parent) {
	size_t num_sym_tables, num_type_tables, size;
	userp_scope scope;

	// Parent must be final
	if (parent && !parent->is_final) {
		env->diag_code= USERP_EINVAL;
		env->diag_tpl= "Cannot create a nested scope until the parent is finalized";
		return NULL;
	}

	// Allocation tricks - allocate the main struct and its arrays of table stacks in one piece
	num_sym_tables= 1 + (parent? parent->symtable_count : 0);
	num_type_tables= 1 + (parent? parent->typetable_count : 0);
	size= sizeof(struct userp_scope)
		+ num_sym_tables * sizeof(void*)
		+ num_type_tables * sizeof(void*);
	if (parent && parent->level >= SYMBOL_SCOPE_LIMIT) {
		env->diag_code= USERP_EDOINGITWRONG;
		env->diag_size= SYMBOL_SCOPE_LIMIT;
		env->diag_tpl= "Scope nesting level exceeds sensible limit of " USERP_DIAG_SIZE;
		return NULL;
	}
	if (parent && !userp_grab_scope(parent))
		return NULL;
	if (!USERP_ALLOC(env, &scope, size, USERP_FLAG_STATIC)) {
		if (parent) userp_drop_scope(parent);
		return NULL;
	}
	bzero(scope, size);
	scope->env= env;
	scope->parent= parent;
	scope->level= parent? parent->level + 1 : 0;
	scope->refcnt= 1;
	scope->symtable_count= num_sym_tables-1; // final element is left off until first use
	scope->symtable_stack= (symbol_table*) (((char*)scope) + sizeof(struct userp_scope));
	scope->typetable_count= num_type_tables-1; // final element is left off until first use
	scope->typetable_stack= (type_table*) (((char*)scope) + sizeof(struct userp_scope) + num_sym_tables * sizeof(void*));
	return scope;
}

static void free_scope(userp_scope scope) {
	userp_env env= scope->env;
	if (scope->symtable) realloc_symtable(env, &scope->symtable, 0);
	if (scope->typetable) free_typetable(env, &scope->typetable, 0);
	USERP_FREE(env, &scope);
}

bool userp_grab_scope(userp_scope scope) {
	if (!scope) return false;
	if (!scope->refcnt) return true;
	if (!(scope->refcnt+1)) {
		scope->env->diag_code= USERP_EREFCNT;
		scope->env->diag_tpl= "Max number of references reached for scope";
		return false;
	}
	++scope->refcnt;
	return true;
}

void userp_drop_scope(userp_scope scope) {
	if (scope && scope->refcnt) {
		if (!--scope->refcnt && scope->env)
			free_scope(scope);
	}
}

userp_symbol userp_scope_get_symbol(userp_scope scope, const char * name, bool create) {
	unsigned table_i, i, lim;
	size_t len, pos;
	symbol_table t;
	// TODO: use hash table
	for (table_i= scope->symtable_count; table_i > 0;) {
		--table_i;
		t= scope->symtable_stack[table_i];
		for (i= 0, lim= t->count; i < lim; i++) {
			if (0 == strcmp(t->symbol_text.start + t->symbol_ofs[i], name)) {
				assert(i < SYMBOL_OFS_LIMIT);
				assert(table_i < SYMBOL_SCOPE_LIMIT);
				return (i << SYMBOL_SCOPE_BITS) | table_i;
			}
		}
	}
	return create? userp_scope_add_symbol(scope, name) : 0;
}

userp_symbol userp_scope_add_symbol(userp_scope scope, const char *name) {
	size_t pos, len, sym_i, tbl_i;

	if (scope->is_final) {
		scope->env->diag_code= USERP_EINVAL;
		scope->env->diag_tpl= "Cannot modify a finalized scope";
		return 0;
	}
	len= strlen(name);
	// Create the symbol table for this scope if it wasn't initialized yet
	if (!scope->symtable) {
		if (!realloc_symtable(scope->env, &scope->symtable, 2))
			return 0;
		if (!(scope->symtable->symbol_text.buf= userp_new_buffer(scope->env, NULL, (len+1+1023)&1023)))
			return 0;
		// symtable_stack was originally allocated with a spare slot for this exact purpose
		tbl_i= scope->symtable_count++;
		scope->symtable_stack[tbl_i]= scope->symtable;
		// If this is the 0th symbol of the 0th scope, inject an empty string for the null symbol
		if (tbl_i == 0) {
			// Hasn't been written yet, but offset=strlen will be a NUL character
			scope->symtable->symbol_ofs[0]= len;
			++scope->symtable->count;
		}
		scope->symtable->symbol_text.len= len+1; // guaranteed to be within buf.alloc_len
		pos= 0;
	}
	else {
		// Ensure space for one more symbol entry
		if (scope->symtable->count >= scope->symtable->n_alloc)
			if (!realloc_symtable(scope->env, &scope->symtable, scope->symtable->count+1))
				return 0;
		pos= scope->symtable->symbol_text.len;
		// Ensure space to store the body of the string + NUL
		if (!userp_bstring_grow(scope->env, &scope->symtable->symbol_text, len+1))
			return 0;
		tbl_i= scope->symtable_count-1;
	}
	sym_i= scope->symtable->count++;
	memcpy(scope->symtable->symbol_text.start + pos, name, len+1);
	scope->symtable->symbol_ofs[sym_i]= pos;
	return (sym_i << SYMBOL_SCOPE_BITS) | tbl_i;
}

userp_type userp_scope_get_type(userp_scope scope, userp_symbol name) {
	return NULL;
}

userp_type userp_scope_new_type(userp_scope scope, userp_symbol name, userp_type base_type) {
	return NULL;
}

userp_enc userp_type_encode(userp_type type) {
	return NULL;
}

userp_dec userp_type_decode(userp_type type) {
	return NULL;
}
