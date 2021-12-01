#include "local.h"
#include "userp.h"
#include "userp_private.h"

/*----------------------------------------------------------------------------

## Symbol Tables

The symbol tables are built either from parsing a symbol table of the protocol,
or one symbol at a time as the user supplies them.

### Table Parsed From Buffers

In the whole-table case, the memory holding the symbols is pre-allocated in
a userp_bstr, and the scope will hold a strong reference to those buffers for
its lifespan.  But, any symbol that crosses the boundary of one buffer to the
next needs copied to a buffer where it can be contiguous.  So, the symbol table
parser may allocate additional buffers to hold those symbols.

The final symbol table is stored in an array of small structs each pointing to
the name, and if it refers to any type in the current scope.  A second array of
pointers lists the symbols in sorted order for quick lookup.

### Table Built by User

In the one-at-a-time case, the memory holding the symbols is allocated one block
at a time, and the symbol vector is allocated one element at a time, sorted with
a red-black tree.

When the scope is finalized, the user has the option to optimize the symbol
table, sorting all the strings into one contiguous buffer and reducing the
lookup from a binary tree to a binary search on an array.

*/

static bool scope_init_symtable(userp_scope scope);

static bool symbol_tree_insert31(struct symbol_table *st) {
	// element 0 is the "sentinel" leaf, and 'last' is the element being added
	struct symbol_tree_node31 *tree= (struct symbol_tree_node31 *) st->tree;
	struct symbol_entry *symbols= st->symbols;
	int cmp;
	uint32_t stack[64], pos, newest= (uint32_t) st->used, parent, new_head, i= 0;
	
	tree[newest].left= 0;
	tree[newest].right= 0;

	// If there is no root, the new node is the root.
	if (!st->tree_root) {
		st->tree_root= newest;
		tree[newest].color= 0;
		return true;
	}
	tree[newest].color= 1;

	// leaf-sentinel will double as the head-sentinel
	stack[i++]= 0;
	pos= (uint32_t) st->tree_root;
	while (pos && i < 64) {
		stack[i++]= pos;
		cmp= strcmp(symbols[newest].name, symbols[pos].name);
		pos= cmp < 0? tree[pos].left : tree[pos].right;
	}
	if (i >= 64) {
		assert(i < 64);
		return false;
	}
	pos= stack[--i];
	if (cmp < 0)
		tree[pos].left= newest;
	else
		tree[pos].right= newest;
	tree[newest].left= 0;
	tree[newest].right= 0;
	tree[newest].color= 1;
	// balance the tree.  Pos points at the parent node of the new node.
	// if current is a black node, no rotations needed
	while (i > 1 && tree[pos].color) {
		// current is red, the imbalanced child is red, and parent is black.
		parent= stack[--i];
		// if the current is on the right of the parent, the parent is to the left
		if (tree[parent].right == pos) {
			// if the sibling is also red, we can pull down the color black from the parent
			if (tree[tree[parent].left].color) {
				tree[tree[parent].left].color= 0;
				tree[pos].color= 0;
				tree[parent].color= 1;
				// jump twice up the tree
				pos= stack[--i];
				continue;
			}
			// if the imbalance (red node) is on the left, and the parent is on the left,
			//  rotate the inner tree to the right
			if (tree[tree[pos].left].color) {
				// rotate right
				new_head= tree[pos].left;
				if (tree[parent].right == pos) tree[parent].right= new_head;
				else tree[parent].left= new_head;
				tree[pos].left= tree[new_head].right;
				tree[new_head].right= pos;
				pos= new_head;
			}

			// Now we can do the left rotation to balance the tree.
			pos= parent;
			parent= stack[--i];
			new_head= tree[pos].right;
			if (!parent) st->tree_root= new_head;
			else if (tree[parent].right == pos) tree[parent].right= new_head;
			else tree[parent].left= new_head;
			tree[pos].right= tree[new_head].left;
			tree[new_head].left= pos;
			tree[pos].color= 1;
			tree[parent].color= 0;
			return true;
		}
		// else the parent is to the right
		else {
			// if the sibling is also red, we can pull down the color black from the parent
			if (tree[tree[parent].right].color) {
				tree[tree[parent].right].color= 0;
				tree[pos].color= 0;
				tree[parent].color= 1;
				// jump twice up the tree
				pos= stack[--i];
				continue;
			}
			// if the imbalance (red node) is on the right, and the parent is on the right,
			//  rotate the inner tree to the left
			if (tree[tree[pos].right].color) {
				// rotate left
				new_head= tree[pos].right;
				if (tree[parent].right == pos) tree[parent].right= new_head;
				else tree[parent].left= new_head;
				tree[pos].right= tree[new_head].left;
				tree[new_head].left= pos;
				pos= new_head;
			}

			// Now we can do the right rotation to balance the tree.
			pos= parent;
			parent= stack[--i];
			new_head= tree[pos].left;
			if (!parent) st->tree_root= new_head;
			else if (tree[parent].right == pos) tree[parent].right= new_head;
			else tree[parent].left= new_head;
			tree[pos].left= tree[new_head].right;
			tree[new_head].right= pos;
			tree[pos].color= 1;
			tree[parent].color= 0;
			return true;
		}
	}
	// ensure the root node is black
	tree[st->tree_root].color= 0;
	return true;
}

static bool symbol_tree_insert15(struct symbol_table *st) {
	// element 0 is the "sentinel" leaf, and 'last' is the element being added
	struct symbol_tree_node15 *tree= (struct symbol_tree_node15 *) st->tree;
	struct symbol_entry *symbols= st->symbols;
	int cmp;
	uint16_t stack[64], pos, newest= (uint16_t) st->used, parent, new_head, i= 0;
	
	tree[newest].left= 0;
	tree[newest].right= 0;
	
	// If there is no root, the new node is the root.
	if (!st->tree_root) {
		st->tree_root= newest;
		tree[newest].color= 0;
		return true;
	}
	tree[newest].color= 1;
	
	// leaf-sentinel will double as the head-sentinel
	stack[i++]= 0;
	pos= (uint16_t) st->tree_root;
	while (pos && i < 32) {
		stack[i++]= pos;
		cmp= strcmp(symbols[newest].name, symbols[pos].name);
		pos= cmp < 0? tree[pos].left : tree[pos].right;
	}
	if (i >= 32) {
		assert(i < 32);
		return false;
	}
	pos= stack[--i];
	if (cmp < 0)
		tree[pos].left= newest;
	else
		tree[pos].right= newest;
	tree[newest].left= 0;
	tree[newest].right= 0;
	tree[newest].color= 1;
	// balance the tree.  Pos points at the parent node of the new node.
	// if current is a black node, no rotations needed
	while (i > 1 && tree[pos].color) {
		// current is red, the imbalanced child is red, and parent is black.
		parent= stack[--i];
		// if the current is on the right of the parent, the parent is to the left
		if (tree[parent].right == pos) {
			// if the sibling is also red, we can pull down the color black from the parent
			if (tree[tree[parent].left].color) {
				tree[tree[parent].left].color= 0;
				tree[pos].color= 0;
				tree[parent].color= 1;
				// jump twice up the tree
				pos= stack[--i];
				continue;
			}
			// if the imbalance (red node) is on the left, and the parent is on the left,
			//  rotate the inner tree to the right
			if (tree[tree[pos].left].color) {
				// rotate right
				new_head= tree[pos].left;
				if (tree[parent].right == pos) tree[parent].right= new_head;
				else tree[parent].left= new_head;
				tree[pos].left= tree[new_head].right;
				tree[new_head].right= pos;
				pos= new_head;
			}

			// Now we can do the left rotation to balance the tree.
			pos= parent;
			parent= stack[--i];
			new_head= tree[pos].right;
			if (!parent) st->tree_root= new_head;
			else if (tree[parent].right == pos) tree[parent].right= new_head;
			else tree[parent].left= new_head;
			tree[pos].right= tree[new_head].left;
			tree[new_head].left= pos;
			tree[pos].color= 1;
			tree[parent].color= 0;
			return true;
		}
		// else the parent is to the right
		else {
			// if the sibling is also red, we can pull down the color black from the parent
			if (tree[tree[parent].right].color) {
				tree[tree[parent].right].color= 0;
				tree[pos].color= 0;
				tree[parent].color= 1;
				// jump twice up the tree
				pos= stack[--i];
				continue;
			}
			// if the imbalance (red node) is on the right, and the parent is on the right,
			//  rotate the inner tree to the left
			if (tree[tree[pos].right].color) {
				// rotate left
				new_head= tree[pos].right;
				if (tree[parent].right == pos) tree[parent].right= new_head;
				else tree[parent].left= new_head;
				tree[pos].right= tree[new_head].left;
				tree[new_head].left= pos;
				pos= new_head;
			}

			// Now we can do the right rotation to balance the tree.
			pos= parent;
			parent= stack[--i];
			new_head= tree[pos].left;
			if (!parent) st->tree_root= new_head;
			else if (tree[parent].right == pos) tree[parent].right= new_head;
			else tree[parent].left= new_head;
			tree[pos].left= tree[new_head].right;
			tree[new_head].right= pos;
			tree[pos].color= 1;
			tree[parent].color= 0;
			return true;
		}
	}
	// ensure the root node is black
	tree[st->tree_root].color= 0;
	return true;
}

userp_symbol userp_scope_get_symbol(userp_scope scope, const char *name, int flags) {
	int i, pos, start, limit, cmp, len;
	size_t size;
	struct symbol_table *st;
	userp_env env= scope->env;
	struct symbol_tree_node15 *tree15;
	struct symbol_tree_node31 *tree31;
	
	// search self and parent scopes for symbol, unless flags request local-only
	for (i= scope->symtable_count - 1; i >= 0; --i) {
		st= scope->symtable_stack[i];
		if (st->tree) {
			// tree search
			if (st->alloc >> 15) { // more than 15 bits of entries uses a tree of 31-bit integers.
				tree31= ((struct symbol_tree_node31*) st->tree);
				for (pos= st->tree_root; pos;) {
					cmp= strcmp(name, st->symbols[pos].name);
					if (cmp == 0) return st->id_offset + pos;
					else if (cmp > 0) pos= tree31[pos].right;
					else pos= tree31[pos].left;
				}
			}
			else {
				tree15= ((struct symbol_tree_node15*) st->tree);
				for (pos= st->tree_root; pos;) {
					cmp= strcmp(name, st->symbols[pos].name);
					if (cmp == 0) return st->id_offset + pos;
					else if (cmp > 0) pos= tree15[pos].right;
					else pos= tree15[pos].left;
				}
			}
		}
		else {
			// binary search
			for (start= 1, limit= st->used; start < limit;) {
				pos= (start+limit)>>1;
				cmp= strcmp(name, st->symbols[pos].name);
				if (cmp == 0) return st->id_offset + pos;
				else if (cmp > 0) start= pos+1;
				else limit= pos;
			}
		}
		// User can request only searching immediate scope
		if (flags & USERP_GET_LOCAL)
			break;
	}
	// if needs added:
	if (!(flags & USERP_CREATE))
		return 0;
	// if scope is finalized, emit an error
	if (scope->is_final) {
		userp_diag_set(&env->err, USERP_ESCOPEFINAL, "Can't add symbol to a finalized scope");
		USERP_DISPATCH_ERROR(env);
		return 0;
	}
	if (!scope->has_symbols)
		scope_init_symtable(scope);
	pos= scope->symtable.used;
	if (!pos) ++pos; // leave slot 0 blank
	// Grow the symbols array if needed
	if (pos >= scope->symtable.alloc) {
		limit= USERP_SCOPE_TABLE_ALLOC_ROUND(pos+1);
		if (!USERP_ALLOC_ARRAY(env, &scope->symtable.symbols, limit))
			return 0;
		// But also, if this is the moment that the size exceeds the limit of 15-bits, upgrade the tree.
		if (scope->symtable.tree && (limit >> 15) && !(scope->symtable.alloc >> 15)) {
			// well that would be a lot of symbols
			if (scope->symtable.alloc >> 31) {
				userp_diag_set(&env->err, USERP_EDOINGITWRONG, "Symbol table full (at 2**31 entries)");
				USERP_DISPATCH_ERROR(env);
				return 0;
			}
			tree15= (struct symbol_tree_node15*) scope->symtable.tree;
			tree31= NULL;
			if (!USERP_ALLOC_ARRAY(env, &tree31, limit))
				return 0;
			for (i= 0; i < scope->symtable.used; i++) {
				tree31[i].left=  tree15[i].left;
				tree31[i].right= tree15[i].right;
				tree31[i].color= tree15[i].color;
			}
			USERP_FREE(env, tree15);
			scope->symtable.tree= tree31;
		}
		else {
			size= limit * ((limit>>15)? sizeof(struct symbol_tree_node31) : sizeof(struct symbol_tree_node15));
			if (!userp_alloc(env, &scope->symtable.tree, size, 0))
				return 0;
		}
		scope->symtable.alloc= limit;
	}
	// if tree is not built, and symbol does not compare greater than previous, need the tree.
	if (!scope->symtable.tree && (pos > 1 && strcmp(name, scope->symtable.symbols[pos-1].name) <= 0)) {
		size= limit * ((limit>>15)? sizeof(struct symbol_tree_node31) : sizeof(struct symbol_tree_node15));
		if (!userp_alloc(env, &scope->symtable.tree, size, 0))
			return 0;
		// TODO: build tree
	}
	len= strlen(name);
	// Copy the name into scope storage.  This can initialize a NULL pointer with a new bstr.
	if (!(name= (char*) userp_bstr_append_bytes(env, &scope->symtable.chardata, (const uint8_t*) name, len+1)))
		return 0;
	// add symbol to the vector
	scope->symtable.symbols[pos].name= name; // name was replaced with the local pointer, above
	scope->symtable.symbols[pos].type_ref= NULL;
	scope->symtable.used= pos+1;
	if (scope->symtable.tree) {
		// add symbol to the tree.  The new node is automatically implied to be the final on the vector
		if (scope->symtable.alloc >> 15)
			symbol_tree_insert15(&scope->symtable);
		else
			symbol_tree_insert31(&scope->symtable);
	}
	return pos + scope->symtable.id_offset;
}

bool userp_scope_parse_symbols(userp_scope scope, struct userp_bstr_part *parts, size_t part_count, int sym_count, int flags) {
	// If scope is finalized, emit an error
	// If chardata is not initialized, allocate input.part_count*2 - 1 parts
	// ensure symbol vector has sym_count slots allocated (if provided)
	// loop through the parts of input
	//   loop through the characters of current part
	//   If invalid character encountered, emit error and return false.
	//   If a NUL is found
	//     If the symbol started in the current part,
	//       If the buffer was not added to chardata, add it.
	//     else
	//       allocate a new buffer to hold this symbol, and copy it
    //     If tree is initialized, add it to the tree as well.
	//     else check if the symbol compares greater than previous.
	//       If not, allocate tree and load it and all previous into the tree
	//     Add the symbol to the vector
	// If the input ran out of characters before sym_count (and provided), emit an error
	// If the input had extra characters, no problem.
	// If the symbols were not in order, emit a warning
	return false;
}

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
			USERP_DISPATCH_ERROR(env);
			return NULL;
		}
		// Parent must be final
		if (!parent->is_final) {
			userp_diag_set(&env->err, USERP_EDOINGITWRONG, "Cannot create a nested scope until the parent is finalized");
			USERP_DISPATCH_ERROR(env);
			return NULL;
		}
		if (parent->level >= env->scope_stack_max) {
			userp_diag_setf(&env->err, USERP_ELIMIT,
				"Scope nesting level exceeds limit of " USERP_DIAG_SIZE,
				(size_t) env->scope_stack_max);
			USERP_DISPATCH_ERROR(env);
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
	scope->symtable_stack= (struct symbol_table**) (scope->typetable_stack + num_type_tables);
	scope->typetable_count= num_type_tables-1; // final element is left off until first use
	if (parent) {
		memcpy(scope->symtable_stack, parent->symtable_stack, parent->symtable_count);
		memcpy(scope->typetable_stack, parent->typetable_stack, parent->typetable_count);
	}
	return scope;
}

static bool scope_init_symtable(userp_scope scope) {
	// sanity check
	if (scope->is_final || scope->has_symbols) return false;

	scope->symtable.id_offset= !scope->symtable_count? 0
		: scope->symtable_stack[scope->symtable_count-1]->id_offset
		  + scope->symtable_stack[scope->symtable_count-1]->used
		  - 1; // because slot 0 of each symbol table is used as a NUL element.

	// When allocated, the userp_scope left room in symtable_stack for one extra
	scope->symtable_stack[scope->symtable_count++]= &scope->symtable;
	scope->has_symbols= true;

	// All other fields were blanked during the userp_scope constructor
	return true;
}

static void userp_free_scope(userp_scope scope) {
	userp_env env= scope->env;
	userp_scope parent= scope->parent;
	if (scope->symtable.chardata)
		userp_bstr_alloc(scope->env, &scope->symtable.chardata, 0);
	if (scope->symtable.tree)
		USERP_FREE(env, &scope->symtable.tree);
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
		USERP_DISPATCH_ERROR(scope->env);
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

/*
#define SYMBOL_PARSE_EOF -1
#define SYMBOL_PARSE_BAD_UTF8 -2
#define SYMBOL_PARSE_OVERLONG_UTF8 -3
#define SYMBOL_PARSE_FORBIDDEN_CHAR -4
struct symbol_parse_state {
	size_t pos;
	size_t *ends, ends_max, ends_count;
	const char *diag_tpl;
	size_t diag_code, diag_count, diag_size;
};
bool parse_symbols(struct symbol_parse_state *parse, const uint8_t *data, size_t len) {
	size_t pos= parse->pos, start;
	uint_least32_t codepoint;
	while (pos < len && parse->ends_count < parse->ends_max) {
		start= pos;
		// Begin one symbol
		while (1) {
			// 1-byte code: 0x00-0x7F
			// 2-byte code: 0xE0-0xEF + 6 bits
			// 3-byte code: 0xF0-0xF7 + 12 bits
			// 4-byte code: 0xF8-0xFB + 18 bits
			// larger byte sequences are currently disallowed by unicode standard
			// continuation bytes are all 0xC0-0xDF
			// Nothing above cares about the low 3 bits except 4-byte code, so optimize the switch a bit
			switch (data[pos] >> 3) {
			// Disallow control chars
			case 0:
				if (!data[pos]) goto end_of_string;
			case 1: case 2: case 3: 
				goto invalid_char;
			case 15:
				if (data[pos] == 0x7F) goto invalid_char;
			case 4: case 5: case 6: case 7: case 8: case 9: case 10: case 11: case 12: case 13: case 14:
				if (++pos >= len) goto invalid_eof;
				continue; // valid single-byte char
			case 0b11100: case 0b11101:
				if (pos + 1 + 1 >= len) goto invalid_eof;
				if ((data[pos+1] >> 6) != 2) goto invalid_encoding;
				codepoint= ((uint_least32_t)(data[pos] & 0x0F) << 6)
						| (uint_least32_t)(data[pos+1] & 0x3F);
				if (!(codepoint >> 7)) goto invalid_overlong;
				pos += 2;
				break;
			case 0b11110:
				if (pos + 2 + 1 >= len) goto invalid_eof;
				if ((data[pos+1] >> 6) != 2 || (data[pos+2] >> 6) != 2) goto invalid_encoding;
				codepoint= ((((uint_least32_t)(data[pos] & 0x07) << 6)
						| (uint_least32_t)(data[pos+1] & 0x3F)) << 6)
						| (uint_least32_t)(data[pos+2] & 0x3F);
				if (!(codepoint >> 10)) goto invalid_overlong;
				pos += 3;
				break;
			case 0b11111:
				// need to also check the bit that got clipped on shift-right
				if (!(data[pos] & 4)) {
					if (pos + 3 + 1 >= len) goto invalid_eof;
					if ((data[pos+1] >> 6) != 2 || (data[pos+2] >> 6) != 2 || (data[pos+3] >> 6) != 2) goto invalid_encoding;
					codepoint= (((((
						(uint_least32_t)(data[pos] & 0x03) << 6)
						| (uint_least32_t)(data[pos+1] & 0x3F)) << 6)
						| (uint_least32_t)(data[pos+2] & 0x3F)) << 6)
						| (uint_least32_t)(data[pos+3] & 0x3F);
					if (!(codepoint >> 15)) goto invalid_overlong;
					pos += 4;
					break;
				}
			default: // any other prefix is invalid
				goto invalid_encoding;
			}
			// TODO: further restrictions on charset for identifiers
		}
		end_of_string:
		// Here, pos points to the NUL character and start is the beginning of the string

		// zero-length identifier is forbidden
		if (pos == start)
			goto invalid_char;
		parse->ends[parse->ends_count++]= pos;
		++pos; // resume parsing at char beyond '\0'
	}
	parse->pos= pos;
	return true;

	{
		CATCH(invalid_encoding) {
			parse->diag_tpl= "Symbol table entry " USERP_DIAG_COUNT " is not valid UTF-8";
		}
		CATCH(invalid_overlong) {
			parse->diag_tpl= "Symbol table entry " USERP_DIAG_COUNT " contains over-long UTF-8 encoding";
		}
		CATCH(invalid_char) {
			parse->diag_size= codepoint;
			parse->diag_tpl= "Symbol table entry " USERP_DIAG_COUNT " contains forbidden codepoint " USERP_DIAG_SIZE;
		}
		parse->diag_count= parse->ends_count;
		parse->diag_code= USERP_ESYMBOL;
	}
	CATCH(invalid_eof) {
		parse->diag_code= USERP_EEOF;
		parse->diag_tpl= "Symbol table ended mid-symbol";
	}
	parse->pos= pos;
	return false;
}

bool userp_validate_symbol(const char *buf, size_t buflen) {
	struct symbol_parse_state pst;
	size_t end;
	bzero(&pst, sizeof(pst));
	pst.ends= &end;
	pst.ends_max= 1;
	return parse_symbols(&pst, buf, buflen) && pst.pos == buflen;
}

bool userp_scope_parse_symtable(userp_scope scope, struct userp_bstring *symbol_text) {
	symbol_table st= NULL;
	struct symbol_parse_state pst;
	size_t end;
	
	if (!scope || !scope->env) return false;
	if (scope->is_final || scope->symtable) {
		scope->env->diag_code= USERP_EINVAL;
		scope->env->diag_tpl= scope->is_final? "Can't modify a finalized scope"
			: "Can't parse symbols after symbol table initialized";
		return false;
	}
	if (!symbol_text->len) return true; // no symbols.
	// guess at initial allocation of bytes / 32  (identifiers will probably be smaller) round up to 16
	if (!alloc_symtable(scope->env, &st, ((symbol_text->len >> 5) + 15) & ~15, USERP_HINT_DYNAMIC))
		goto alloc_fail;

	bzero(&pst, sizeof(pst));
	pst.ends= st->symbol_end;
	pst.ends_max= st->n_alloc;
	while (parse_symbols(&pst, symbol_text->start, symbol_text->len)) {
		st->count= pst.ends_count;
		// Did the parse reach the end of the buffer? or did it fill up the table first?
		if (pst.pos >= symbol_text->len) {
			// The Scope needs to retain a reference to the buffer
			st->symbol_text= *symbol_text;
			if (!userp_grab_buffer(st->symbol_text.buf))
				goto alloc_fail;
			// All done.  Keep this symbol table
			scope->symtable= st;
			// When allocated, the stack has room for one more element if scope->symtable was not set.
			scope->symtable_stack[scope->symtable_count++]= st;
			return true;
		}
		else {
			// parse_symbols should only return true if len reached or ends_max reached
			assert(pst.ends_count == pst.ends_max);
			if (!alloc_symtable(scope->env, &st, st->n_alloc + (st->n_alloc >> 1), USERP_HINT_DYNAMIC))
				goto alloc_fail;
			pst.ends_max= st->n_alloc;
		}
	}
	// Parse failure.  Copy the message into the userp_env
	scope->env->diag_tpl= pst.diag_tpl;
	scope->env->diag_code= pst.diag_code;
	scope->env->diag_count= pst.diag_count;
	scope->env->diag_size= pst.diag_size;

	CATCH(alloc_fail) {
		free_symtable(scope->env, &st);
		// diag_code and diag_size have already been set, but change the message
		scope->env->diag_tpl= "Unable to allocate symbol table of " USERP_DIAG_SIZE " bytes";
	}
	return false;
}

bool alloc_typetable(userp_env env, type_table *tt_p, size_t count, int flags) {
	size_t n;
	bool is_new= !*tt_p;
	if (!is_new && (*tt_p)->n_alloc >= count)
		return true;
	n= is_new? count : (count + 15)&~15;
	if (!USERP_ALLOC(env, tt_p, sizeof(struct type_table) + sizeof((*tt_p)->types[0]) * n, flags))
		return false;
	if (is_new)
		bzero(*tt_p, sizeof(struct type_table));
	(*tt_p)->n_alloc= n;
	return true;
}

void free_typetable(userp_env env, type_table *tt_p) {
	int i;
	if (*tt_p) {
		for (i= 0; i < (*tt_p)->count; i++)
			free_type(env, (*tt_p)->types[i]);
		TRACE("free type_table %p\n", *tt_p);
		USERP_FREE(env, tt_p);
	}
}

void free_type(userp_env env, userp_type t) {
	unimplemented("free_type");
}

*/

#ifdef UNIT_TEST

//NIT_TEST(test_userp_scope_parse_symtable) {
//	int i, ofs;
//	userp_env env= userp_new_env(NULL,NULL,NULL);
//	userp_scope scope= userp_new_scope(env, NULL);
//	struct userp_bstring str= { .start= argv[0], .len= strlen(argv[0])+1
//	bool ret= userp_scope_parse_symtable(scope, argv[0], );
//	printf("return: %d\n", ret);
//	if (ret) {
//		printf("symtable: %p\n", scope->symtable);
//		printf("symtable->count: %d\n", scope->symtable->count);
//		printf("symtable->n_alloc: %d\n", scope->symtable->n_alloc);
//		printf("symtable->symbol_text.start: %p\n", scope->symtable->symbol_text.start);
//		printf("symtable->symbol_text.len: %d\n", scope->symtable->symbol_text.len);
//		fflush(stdout);
//		for (i= 0; i < scope->symtable->count; i++) {
//			ofs= (i == 0? 0 : scope->symtable->symbol_end[i-1]+1);
//			printf("symbol %d: %d %s\n", i,
//				scope->symtable->symbol_end[i] - ofs,
//				scope->symtable->symbol_text.start + ofs
//			);
//		}
//		fflush(stdout);
//	}
//	else {
//		userp_env_print_diag(env, stdout);
//	}
//	return ret? 0 : 2;
//}

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
/alloc 0x0+ to \d+ = 0x\w+/
# alloc outer
/alloc 0x0+ to \d+ = 0x\w+/
# alloc inner
/alloc 0x0+ to \d+ = 0x\w+/
# env->refcnt=3  outer->refcnt=2  inner->refcnt=1
drop env = 0
drop outer = 0
# free all via refcnt
/alloc 0x\w+ to 0 = 0x0+/
/alloc 0x\w+ to 0 = 0x0+/
/alloc 0x\w+ to 0 = 0x0+/
drop inner = 1
*/

#endif
