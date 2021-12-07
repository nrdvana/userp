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
	scope->env= env;
	scope->parent= parent;
	scope->level= parent? parent->level + 1 : 0;
	scope->refcnt= 1;
	scope->symtable_count= num_sym_tables-1; // final element is left off until first use
	scope->symtable_stack= (struct userp_symtable**) (scope->typetable_stack + num_type_tables);
	scope->typetable_count= num_type_tables-1; // final element is left off until first use
	if (parent) {
		memcpy(scope->symtable_stack, parent->symtable_stack, parent->symtable_count);
		memcpy(scope->typetable_stack, parent->typetable_stack, parent->typetable_count);
	}
	return scope;
}

static void userp_free_scope(userp_scope scope) {
	userp_env env= scope->env;
	userp_scope parent= scope->parent;
	if (scope->has_symbols) {
		if (scope->symtable.nodes)
			USERP_FREE(env, &scope->symtable.nodes);
		if (scope->symtable.buckets)
			USERP_FREE(env, &scope->symtable.buckets);
		if (scope->symtable.symbols)
			USERP_FREE(env, &scope->symtable.symbols);
		userp_bstr_destroy(&scope->symtable.chardata);
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
		printf("   local table: %d/%d %s (%lld vector bytes)\n",
			(int)scope->symtable.used-1,
			(int)scope->symtable.alloc,
			!scope->symtable.bucket_alloc? "not indexed"
				: scope->symtable.processed == scope->symtable.used? "indexed"
				: "partially indexed",
			(long long)(scope->symtable.alloc * sizeof(scope->symtable.symbols[0]))
		);
		if (scope->symtable.buckets) {
			printf("      hashtree: %d/%d+%d (%lld table bytes, %lld node bytes)\n",
				(int)scope->symtable.bucket_used,
				(int)scope->symtable.bucket_alloc,
				(int)scope->symtable.node_used-1,
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

#endif
