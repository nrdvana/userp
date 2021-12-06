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
	userp_bstr_destroy(&scope->symtable.chardata);
	if (scope->symtable.buckets)
		USERP_FREE(env, &scope->symtable.buckets);
	if (scope->symtable.nodes)
		USERP_FREE(env, &scope->symtable.nodes);
	if (scope->symtable.symbols)
		USERP_FREE(env, &scope->symtable.symbols);
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
