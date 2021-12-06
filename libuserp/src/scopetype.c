// This file is included into scope.c

/*----------------------------------------------------------------------------

## Type Tables

The type tables are always parsed from a buffer.  Types can be defined in
terms of arbitrary other types, so the best way to represent a type is with
its protocol encoding.  As the scope parses a type table, it of course pulls
out the interesting bits into structures.  The static-sized structures are
allocated in a vector, and the dynamic-sized structures are packed end-to-end
in an auxiliary buffer.  (types cannot be un-defined, so there is no reason
to allocate them as individual objects)

TODO: come up with an API that helps the user get an encoder to define types,
to save them the trouble of the buffers.

*/

#define MAX_TYPETABLE_ENTRIES MIN((SIZE_MAX / sizeof(struct type_entry)), ((size_t)(1<<31)-1))

static bool scope_typetable_alloc(userp_scope scope, size_t n) {
	userp_env env= scope->env;
	size_t i;

	assert(!scope->is_final);
	assert(n > 0);
	// On the first allocation, allocate the exact number requested (unless '1')
	// After that, allocate in powers of two.
	if (scope->typetable.alloc)
		n= roundup_pow2(n);
	else {
		// n=1 indicates added one at a time, so block-alloc
		if (n == 1) n= 64;
		if (!scope->has_types) {
			// On the first allocation, also need to add this typetable to the set used by the scope.
			// When allocated, the userp_scope left room in typetable_stack for one extra
			i= scope->typetable_count++;
			scope->typetable_stack[i]= &scope->typetable;
			scope->typetable.id_offset= !i? 1
				: scope->typetable_stack[i-1]->id_offset
				+ scope->typetable_stack[i-1]->used;
			scope->typetable.typeobjects.env= env;
			scope->typetable.typedata.env= env;
			scope->has_types= true;
			// All other fields were blanked during the userp_scope constructor
		}
	}
	// Type tables never shrink
	if (n <= scope->typetable.alloc)
		return true; // nothing to do
	// Library implementation is capped at 31-bit references, but on a 32-bit host it's less than that
	// to prevent size_t overflow.
	if (n >= MAX_TYPETABLE_ENTRIES) {
		userp_diag_setf(&env->err, USERP_EDOINGITWRONG,
			"Can't resize type table larger than " USERP_DIAG_SIZE " entries",
			(size_t) MAX_TYPETABLE_ENTRIES
		);
		USERP_DISPATCH_ERR(env);
		return false;
	}

	if (!USERP_ALLOC_ARRAY(env, &scope->typetable.types, n))
		return false;

	scope->typetable.alloc= n;
	return true;
}

userp_type userp_scope_get_type(userp_scope scope, userp_symbol name, int flags) {
	// look up the symbol (binary search on symbol table), return the type if it exists,
	// If not found, go to parent scope and look up symbol of same name
	return 0;
}
userp_type userp_scope_type_by_name(userp_scope scope, const char *name, int flags) {
	// look up the symbol (binary search on symbol table), return the type if it exists,
	// If not found, go to parent scope and look up symbol of same name
	return 0;
}

int userp_scope_parse_types(userp_scope scope, struct userp_bstr_part *parts, size_t part_count, int type_count, int flags) {
	// If scope is finalized, emit an error
	// while there is more input and type_count hasn't been reached,
	//   allocate an entry in the type vector
	//   if the type has aux data, allocate space in the next aux buffer
	//   when the type is fully decoded,
	//     if the type name was already used in this scope, emit an error and return false
	// push a new element onto the type vector
	// have the new type point to the old type's aux data, with a flag to indicate it is shared
	// return the id of the new type
	return 0;
}

