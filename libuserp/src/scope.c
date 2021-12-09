#include "local.h"
#include "userp.h"
#include "userp_private.h"

// Scope Table and Type Table implementations are split into separate files
// for readability, but are part of the same compilation unit.
#include "scopesym.c"
#include "scopetype.c"

/*----------------------------------------------------------------------------

## Scope

The scope is just a container for a symbol table and type table, and some
efficient lookup functions that make the parent scope's symbols and types
available.

*/

static void scope_import_free(struct scope_import *imp);
static struct scope_import* scope_import_new(userp_scope dst, userp_scope src);

userp_scope userp_new_scope(userp_env env, userp_scope parent) {
	size_t num_sym_tables, num_type_tables;
	userp_scope scope= NULL;

	if (parent) {
		// Parent must belong to the same environment
		if (parent->env != env) {
			userp_diag_set(&env->err, USERP_EFOREIGNSCOPE, "Parent scope does not belong to this userp_env");
			USERP_DISPATCH_ERR(env);
			return NULL;
		}
		// Parent must be final
		if (!parent->is_final) {
			userp_diag_set(&env->err, USERP_EDOINGITWRONG, "Cannot create a nested scope until the parent is finalized");
			USERP_DISPATCH_ERR(env);
			return NULL;
		}
		if (parent->level >= env->scope_stack_max) {
			userp_diag_setf(&env->err, USERP_ELIMIT,
				"Scope nesting level exceeds limit of " USERP_DIAG_SIZE,
				(size_t) env->scope_stack_max);
			USERP_DISPATCH_ERR(env);
			return NULL;
		}
		if (!userp_grab_scope(parent))
			return NULL;
	}
	if (!userp_grab_env(env)) {
		if (parent)
			userp_drop_scope(parent);
		return NULL;
	}
	// Allocation tricks - allocate the main struct and its arrays of table stacks in one piece
	num_sym_tables= 1 + (parent? parent->symtable_count : 0);
	num_type_tables= 1 + (parent? parent->typetable_count : 0);
	if (!USERP_ALLOC_OBJPLUS(env, &scope, (num_sym_tables+num_type_tables) * sizeof(void*))) {
		if (parent) userp_drop_scope(parent);
		return NULL;
	}
	bzero(scope, sizeof(*scope));
	scope->serial_id= ++env->scope_serial;
	if (env->log_trace) {
		userp_diag_setf(&env->msg, USERP_MSG_CREATE,
			USERP_DIAG_CSTR1 ": create " USERP_DIAG_INDEX " (" USERP_DIAG_PTR ")",
			"userp_scope", scope->serial_id, scope);
		USERP_DISPATCH_MSG(env);
	}
	scope->env= env;
	scope->parent= parent;
	scope->level= parent? parent->level + 1 : 0;
	scope->refcnt= 1;
	scope->symtable_count= num_sym_tables-1; // final element is left off until first use
	scope->symtable_stack= (struct userp_symtable**) (scope->typetable_stack + num_type_tables);
	scope->typetable_count= num_type_tables-1; // final element is left off until first use
	if (parent) {
		memcpy(scope->symtable_stack, parent->symtable_stack, parent->symtable_count * sizeof(*parent->symtable_stack));
		memcpy(scope->typetable_stack, parent->typetable_stack, parent->typetable_count * sizeof(*parent->symtable_stack));
	}
	return scope;
}

static void userp_free_scope(userp_scope scope) {
	userp_env env= scope->env;
	userp_scope parent= scope->parent;
	struct scope_import *imp, *next_imp;

	if (env->log_trace) {
		userp_diag_setf(&env->msg, USERP_MSG_DESTROY,
			USERP_DIAG_CSTR1 ": destroy " USERP_DIAG_INDEX " (" USERP_DIAG_PTR ")",
			"userp_scope", scope->serial_id, scope);
		USERP_DISPATCH_MSG(env);
	}
	if (scope->has_symbols) {
		if (scope->symtable.nodes)
			USERP_FREE(env, &scope->symtable.nodes);
		if (scope->symtable.buckets)
			USERP_FREE(env, &scope->symtable.buckets);
		if (scope->symtable.symbols)
			USERP_FREE(env, &scope->symtable.symbols);
		userp_bstr_destroy(&scope->symtable.chardata);
	}
	// Free elements of linked list while walking it
	for (imp= scope->lazyimports; imp; imp= next_imp) {
		next_imp= imp->next_import;
		userp_drop_scope(imp->src);
		scope_import_free(imp);
	}
	USERP_FREE(env, &scope);
	if (parent) userp_drop_scope(parent);
	userp_drop_env(env);
}

bool userp_grab_scope(userp_scope scope) {
	if (!scope || !scope->refcnt) {
		fprintf(stderr, "fatal: attemp to grab a destroyed scope\n");
		abort();
	}
	if (!++scope->refcnt) { // check for rollover
		--scope->refcnt; // back up to UINT_MAX
		userp_diag_set(&scope->env->err, USERP_EALLOC, "Refcount limit reached for userp_scope");
		USERP_DISPATCH_ERR(scope->env);
		return false;
	}
	return true;
}

bool userp_drop_scope(userp_scope scope) {
	if (!scope || !scope->refcnt) {
		fprintf(stderr, "fatal: attempt to drop a destroyed scope\n");
		abort();
	}
	if (!--scope->refcnt) {
		userp_free_scope(scope);
		return true;
	}
	return false;
}

bool userp_scope_finalize(userp_scope scope, int flags) {
	// If user requests 'optimize' flag,
	//   if symbol table has a tree, build a new buffer large enough for all symbol data,
	//   then walk the tree copying each symbol into the new buffer,
	//   then replace the vector with a new vector of sorted elements,
	//   then free the tree and the old vector.
	// TODO: make sure there aren't incomplete type definitions
	scope->is_final= 1;
	return true;
}

bool userp_scope_reserve(userp_scope scope, size_t min_symbols, size_t min_types) {
	// if scope is finalized, emit an error
	if (scope->is_final) {
		userp_diag_set(&scope->env->err, USERP_ESCOPEFINAL, "Can't alter a finalized scope");
		USERP_DISPATCH_ERR(scope->env);
		return false;
	}
	if (scope->symtable.alloc < min_symbols)
		if (!scope_symtable_alloc(scope, min_symbols))
			return false;
	if (scope->typetable.alloc < min_types)
		if (!scope_typetable_alloc(scope, min_types))
			return false;
	return true;
}

static struct scope_import* scope_import_new(userp_scope dst, userp_scope src) {
	struct scope_import *imp= NULL;
	if (!USERP_ALLOC_OBJ(dst->env, &imp))
		return false;
	bzero(imp, sizeof(*imp));
	if (src->symtable_count) {
		imp->sym_map_count= src->symtable_stack[src->symtable_count-1]->id_offset
			+ src->symtable_stack[src->symtable_count-1]->used;
		if (!USERP_ALLOC_ARRAY(dst->env, &imp->sym_map, imp->sym_map_count)) {
			USERP_FREE(dst->env, &imp);
			return false;
		}
	}
	if (src->typetable_count) {
		imp->type_map_count= src->typetable_stack[src->typetable_count-1]->id_offset
			+ src->typetable_stack[src->typetable_count-1]->used;
		if (!USERP_ALLOC_ARRAY(dst->env, &imp->type_map, imp->type_map_count)) {
			USERP_FREE(dst->env, &imp->sym_map);
			USERP_FREE(dst->env, &imp);
			return false;
		}
	}
	imp->src= src;
	imp->dst= dst;
	return imp;
}

static void scope_import_free(struct scope_import *imp) {
	userp_env env= imp->dst->env;
	USERP_FREE(env, &imp->type_map);
	USERP_FREE(env, &imp->sym_map);
	USERP_FREE(env, &imp);
}

bool userp_scope_import(userp_scope scope, userp_scope source, int flags) {
	struct scope_import *imp, **ref_p;
	userp_env env= scope->env;
	
	// current scope cannot be final
	if (scope->is_final) {
		userp_diag_set(&env->err, USERP_EDOINGITWRONG, "Can't import into a final scope");
		USERP_DISPATCH_ERR(env);
		return NULL;
	}
	// Source must be final
	if (!source->is_final) {
		userp_diag_set(&env->err, USERP_EDOINGITWRONG, "Can't import from a non-final scope");
		USERP_DISPATCH_ERR(env);
		return NULL;
	}
	// Allocate a mapping between scopes
	if (!(imp= scope_import_new(scope, source)))
		return false;
	if (flags & USERP_LAZY) {
		// Lazy imports hold a reference to the other scope
		if (!userp_grab_scope(source)) {
			scope_import_free(imp);
			return false;
		}
		// Append to linked list of lazy imports
		ref_p= &scope->lazyimports;
		while (*ref_p)
			ref_p= &((*ref_p)->next_import);
		*ref_p= imp;
	}
	else {
		unimplemented("copy symbols and types");
		scope_import_free(imp);
	}
	return true;
}

userp_symbol userp_scope_resolve_relative_symref(userp_scope scope, size_t val) {
	int which_table= 0;
	//  ....0 means "normal symbol ref from 0"
	//  ...01 means "symbol offset from table[-1]"
	//  ..011 means "symbol offset from table[1]"
	while (val & 1) {
		which_table++;
		val >>= 1;
	}
	val >>= 1;
	if (!which_table) return val; // not relative
	// odd numbered "which_table" count down from the top
	if (which_table & 1)
		which_table= scope->symtable_count - 1 - (which_table >> 1);
	else
		which_table >>= 1;
	if (which_table < 0 || which_table >= scope->symtable_count)
		return 0;
	++val; // symbol element 0 is the NULL symbol; skip over it.
	if (val >= scope->symtable_stack[which_table]->used)
		return 0;
	return scope->symtable_stack[which_table]->id_offset + val;
}

userp_type userp_scope_resolve_relative_typeref(userp_scope scope, size_t val) {
	int which_table= 0;
	//  ....0 means "normal symbol ref from 0"
	//  ...01 means "symbol offset from table[-1]"
	//  ..011 means "symbol offset from table[1]"
	while (val & 1) {
		which_table++;
		val >>= 1;
	}
	val >>= 1;
	if (!which_table) return val; // not relative
	// odd numbered "which_table" count down from the top
	if (which_table & 1)
		which_table= scope->typetable_count - 1 - (which_table >> 1);
	else
		which_table >>= 1;
	if (which_table < 0 || which_table >= scope->typetable_count)
		return 0;
	++val; // symbol element 0 is the NULL symbol; skip over it.
	if (val >= scope->typetable_stack[which_table]->used)
		return 0;
	return scope->typetable_stack[which_table]->id_offset + val;
}

#ifdef UNIT_TEST

static void dump_scope(userp_scope scope) {
	struct userp_bstr_part *p;
	int i;

	printf("Scope level=%d  refcnt=%d%s%s%s\n",
		(int)scope->level, (int)scope->refcnt,
		scope->is_final?    " is_final":"",
		scope->has_symbols? " has_symbols":"",
		scope->has_types?   " has_types":""
	);
	printf("  Symbol Table: stack of %d tables, %d symbols\n",
		(int) scope->symtable_count,
		(int)(!scope->symtable_count? 0
			: scope->symtable_stack[scope->symtable_count-1]->id_offset
			+ scope->symtable_stack[scope->symtable_count-1]->used - 1 )
	);
	if (scope->has_symbols) {
		printf("   local table: %d-%d %s (%lld vector bytes)\n",
			(int)scope->symtable.id_offset,
			(int)(scope->symtable.id_offset + scope->symtable.used - 1),
			!scope->symtable.bucket_alloc? "not indexed"
				: scope->symtable.processed == scope->symtable.used? "indexed"
				: "partially indexed",
			(long long)(scope->symtable.alloc * sizeof(scope->symtable.symbols[0]))
		);
		if (scope->symtable.buckets) {
			printf("      hashtree: %d/%d+%d (%lld table bytes, %lld node bytes)\n",
				(int)scope->symtable.bucket_used,
				(int)scope->symtable.bucket_alloc,
				(int)(scope->symtable.node_used? scope->symtable.node_used-1 : 0),
				(long long)( scope->symtable.bucket_alloc * HASHTREE_BUCKET_SIZE(scope->symtable.processed) ),
				(long long)( scope->symtable.node_alloc * HASHTREE_NODE_SIZE(scope->symtable.processed) )
			);
		}
		printf("       buffers:");
		if (scope->symtable.chardata.part_count) {
			for (i= 0; i < scope->symtable.chardata.part_count; i++) {
				p= &scope->symtable.chardata.parts[i];
				printf("  [%ld-%ld]/%ld",
					(long)( p->data - p->buf->data ),
					(long) p->len, (long) p->buf->alloc_len
				);
			}
			printf("\n");
		} else {
			printf("  (none)\n");
		}
	}
	// If the tree is used, verify it's structure
	//if (scope->symtable.tree)
	//	verify_symbol_tree(scope);
	printf("    Type Table: stack of %d tables, %d types\n",
		(int)scope->typetable_count,
		(int)(!scope->typetable_count? 0
			: scope->typetable_stack[scope->typetable_count-1]->id_offset
			+ scope->typetable_stack[scope->typetable_count-1]->used - 1)
	);
}

UNIT_TEST(scope_alloc_free) {
	userp_env env= userp_new_env(logging_alloc, userp_file_logger, stdout, 0);
	printf("# alloc outer\n");
	userp_scope outer= userp_new_scope(env, NULL);
	userp_scope_finalize(outer, 0);
	printf("# alloc inner\n");
	userp_scope inner= userp_new_scope(env, outer);
	printf("# env->refcnt=%d  outer->refcnt=%d  inner->refcnt=%d\n",
		env->refcnt, outer->refcnt, inner->refcnt);
	printf("drop env = %d\n", userp_drop_env(env));
	printf("drop outer = %d\n", userp_drop_scope(outer));
	printf("# free all via refcnt\n");
	printf("drop inner = %d\n", userp_drop_scope(inner));
}
/*OUTPUT
alloc 0x0+ to \d+ = 0x\w+
# alloc outer
alloc 0x0+ to \d+ = 0x\w+
# alloc inner
alloc 0x0+ to \d+ = 0x\w+
# env->refcnt=3  outer->refcnt=2  inner->refcnt=1
drop env = 0
drop outer = 0
# free all via refcnt
alloc 0x\w+ to 0 = 0x0+
alloc 0x\w+ to 0 = 0x0+
alloc 0x\w+ to 0 = 0x0+
drop inner = 1
*/

UNIT_TEST(scope_import_lazy) {
	userp_env env= userp_new_env(NULL, userp_file_logger, stdout, 0);
	env->log_debug= env->log_trace= 1;
	printf("# Create typelib\n");
	userp_scope typelib= userp_new_scope(env, NULL);
	userp_scope_finalize(typelib, 0);
	printf("# Create scope\n");
	userp_scope scope= userp_new_scope(env, NULL);
	printf("# lazy-import\n");
	userp_scope_import(scope, typelib, USERP_LAZY);
	printf("# trigger an import\n");
	// TODO
	printf("# drop ref to typelib\n");
	userp_drop_scope(typelib);
	printf("# drop ref to scope\n");
	userp_drop_scope(scope);
	printf("# drop ref to env\n");
	userp_drop_env(env);
}
/*OUTPUT
# Create typelib
debug: userp_scope: create 1 .*
# Create scope
debug: userp_scope: create 2 .*
# lazy-import
# trigger an import
# drop ref to typelib
# drop ref to scope
debug: userp_scope: destroy 2 .*
debug: userp_scope: destroy 1 .*
# drop ref to env
*/

UNIT_TEST(relative_refs) {
	userp_env env= userp_new_env(NULL, userp_file_logger, stdout, 0);
	userp_scope scopes[4];
	userp_symbol sym;
	size_t ref;
	char buf[16];
	for (int i=0; i < 4; i++) {
		scopes[i]= userp_new_scope(env, i > 0? scopes[i-1] : NULL);
		snprintf(buf, sizeof(buf), "sym%d_%d", i, 1);
		userp_scope_get_symbol(scopes[i], buf, USERP_CREATE);
		snprintf(buf, sizeof(buf), "sym%d_%d", i, 2);
		userp_scope_get_symbol(scopes[i], buf, USERP_CREATE);
		userp_scope_finalize(scopes[i], 0);
	}
	dump_scope(scopes[3]);
	printf("symbol 'sym0_1' = %d\n", (int) userp_scope_get_symbol(scopes[3], "sym0_1", 0));
	printf("symbol 'sym3_2' = %d\n", (int) userp_scope_get_symbol(scopes[3], "sym3_2", 0));
	for (int i= 0; i < 4; i++) {
		ref= i << 1;
		for (int j= 0; j < 8; j++) {
			sym= (int) userp_scope_resolve_relative_symref(scopes[3], ref);
			printf("ref %d%d%d%d%d%d%d%d%d%d = %s%s%s\n",
				(int)(ref>>9)&1, (int)(ref>>8)&1, (int)(ref>>7)&1, (int)(ref>>6)&1, (int)(ref>>5)&1,
				(int)(ref>>4)&1, (int)(ref>>3)&1, (int)(ref>>2)&1, (int)(ref>>1)&1, (int) ref&1,
				sym? "'" : "",
				sym? userp_scope_get_symbol_str(scopes[3], sym) : "NULL",
				sym? "'" : "");
			ref= (ref << 1) | 1;
		}
	}
	for (int i=0; i < 4; i++)
		userp_drop_scope(scopes[i]);
	userp_drop_env(env);
}
/*OUTPUT
Scope level=3  refcnt=1 is_final has_symbols
 *Symbol Table: stack of 4 tables, 8 symbols
 *local table: 6-8 partially indexed .*
 *hashtree:.*
 *buffers:.*
 *Type Table: stack of 0 tables, 0 types
symbol 'sym0_1' = 1
symbol 'sym3_2' = 8
ref 0000000000 = NULL
ref 0000000001 = 'sym3_1'
ref 0000000011 = 'sym1_1'
ref 0000000111 = 'sym2_1'
ref 0000001111 = 'sym2_1'
ref 0000011111 = 'sym1_1'
ref 0000111111 = 'sym3_1'
ref 0001111111 = 'sym0_1'
ref 0000000010 = 'sym0_1'
ref 0000000101 = 'sym3_2'
ref 0000001011 = 'sym1_2'
ref 0000010111 = 'sym2_2'
ref 0000101111 = 'sym2_2'
ref 0001011111 = 'sym1_2'
ref 0010111111 = 'sym3_2'
ref 0101111111 = 'sym0_2'
ref 0000000100 = 'sym0_2'
ref 0000001001 = NULL
ref 0000010011 = NULL
ref 0000100111 = NULL
ref 0001001111 = NULL
ref 0010011111 = NULL
ref 0100111111 = NULL
ref 1001111111 = NULL
ref 0000000110 = 'sym1_1'
ref 0000001101 = NULL
ref 0000011011 = NULL
ref 0000110111 = NULL
ref 0001101111 = NULL
ref 0011011111 = NULL
ref 0110111111 = NULL
ref 1101111111 = NULL
*/

#endif
