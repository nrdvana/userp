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

struct symbol_entry {
	const char *data;
	userp_type type_ref;
};
struct symbol_tree_node31 {
	uint16_t left, right:31, color:1;
};
struct symbol_tree_node15 {
	uint16_t left, right:15, color:1;
};

struct symbol_table {
	struct symbol_entry *symbols; // an array pointing to each symbol
	size_t count, alloc;
	void *symbol_tree;            // an optional red/black tree sorting the symbols
	userp_bstr chardata;          // stores all buffers used by the symbols
};
typedef struct symbol_table symtable;

#define INIT_SYMTABLE(self, alloc_count) \
	if (!USERP_ALLOC_ARRAY(env, &((self)->symbols), alloc_count)) \
		return false; \
	&((self)->count= 0; \
	&((self)->alloc= alloc_count; \
	&((self)->symbol_tree= NULL; \
	&((self)->chardata= NULL;

#define DESTROY_SYMTABLE(self) \
	if (self->chardata) \
		userp_bstr_free(env, &((self)->chardata)); \
	if (self->symbol_tree) \
		USERP_FREE(env, &((self)->symbol_tree)); \
	if (self->symbols) \
		USERP_FREE(env, &((self)->symbols));

userp_symbol userp_scope_get_symbol(userp_scope scope, const char *name, userp_search_flags flags) {
	// if current scope has symbols, look it up first, either using tree or binary search
	// search parent scopes for symbol, unless flags request local-only
	// if needs added:
	//   if scope is finalized, emit an error
	//   if tree is not built, and symbol does not compare greater than previous,
	//     allocate and build the tree
	//   find length of string
	//   If symbol storage not allocated, allocate a default size
	//   add symbol to the storage
	//   If symbols vector not allocated, allocate a default size
	//   add symbol to the vector
	//   if tree not allocated, allodate it
	//   add symbol to the tree
	// return symbol
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

userp_type userp_scope_get_type(userp_scope scope, userp_symbol name, userp_search_flags flags) {
	// look up the symbol (binary search on symbol table), return the type if it exists,
	// If not found, go to parent scope and look up symbol of same name
}
userp_type userp_scope_type_by_name(userp_scope scope, const char *name, userp_search_flags flags) {
	// look up the symbol (binary search on symbol table), return the type if it exists,
	// If not found, go to parent scope and look up symbol of same name
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
}

// static bool userp_scope_finalize(userp_scope *scope, int flags);
//   if symbol table has a tree, build a new buffer large enough for all symbol data,
//   then walk the tree copying each symbol into the new buffer,
//   then replace the vector with a new vector of sorted elements,
//   then free the tree and the old vector.

static void unimplemented(const char* msg) {
	fprintf(stderr, "Unimplemented: %s", msg);
	abort();
}

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
	if (!USERP_ALLOC(env, &scope, size, USERP_HINT_STATIC)) {
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
	if (scope->symtable) free_symtable(env, &scope->symtable);
	if (scope->typetable) free_typetable(env, &scope->typetable);
	USERP_FREE(env, &scope);
}

bool userp_grab_scope(userp_scope scope) {
	if (!scope) return false;
	if (!scope->refcnt) return true;
	if (!(scope->refcnt+1)) {
		scope->env->diag_code= USERP_EDOINGITWRONG;
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
			if (0 == strcmp(t->symbol_text.start + (i==0? 0 : t->symbol_end[i-1]+1), name)) {
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
		scope->env->diag_tpl= "Can't modify a finalized scope";
		return 0;
	}
	len= strlen(name);
	// Create the symbol table for this scope if it wasn't initialized yet
	if (!scope->symtable) {
		if (!alloc_symtable(scope->env, &scope->symtable, 2, USERP_HINT_DYNAMIC))
			return 0;
		if (!(scope->symtable->symbol_text.buf= userp_new_buffer(scope->env, NULL, (len+1+1023)&~1023)))
			return 0;
		// symtable_stack was originally allocated with a spare slot for this exact purpose
		tbl_i= scope->symtable_count++;
		scope->symtable_stack[tbl_i]= scope->symtable;
	}
	else {
		// Ensure space for one more symbol entry
		if (scope->symtable->count >= scope->symtable->n_alloc)
			if (!alloc_symtable(scope->env, &scope->symtable, scope->symtable->count+1, USERP_HINT_DYNAMIC))
				return 0;
		tbl_i= scope->symtable_count-1;
	}
	if (!userp_bstring_grow(scope->env, &scope->symtable->symbol_text, len+1, name))
		return 0;
	sym_i= scope->symtable->count++;
	scope->symtable->symbol_end[sym_i]= scope->symtable->symbol_text.len-1;
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


bool alloc_symtable(userp_env env, symbol_table *st_p, size_t count, int flags) {
	size_t n;
	bool is_new= !*st_p;
	if (!is_new && (*st_p)->n_alloc >= count)
		return true;
	n= is_new? count : (count + 63)&~63;
	if (!USERP_ALLOC(env, st_p, sizeof(struct symbol_table) + sizeof((*st_p)->symbol_end[0]) * n, flags))
		return false;
	if (is_new)
		bzero(*st_p, sizeof(struct symbol_table));
	(*st_p)->n_alloc= n;
	return true;
}

void free_symtable(userp_env env, symbol_table *st_p) {
	if (*st_p) {
		if ((*st_p)->symbol_text.buf)
			userp_drop_buffer((*st_p)->symbol_text.buf);
		USERP_FREE(env, st_p);
	}
}

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

#ifdef WITH_UNIT_TESTS

UNIT_TEST(test_userp_scope_parse_symtable) {
	int i, ofs;
	userp_env env= userp_new_env(NULL,NULL,NULL);
	userp_scope scope= userp_new_scope(env, NULL);
	struct userp_bstring str= { .start= argv[0], .len= strlen(argv[0])+1
	bool ret= userp_scope_parse_symtable(scope, argv[0], );
	printf("return: %d\n", ret);
	if (ret) {
		printf("symtable: %p\n", scope->symtable);
		printf("symtable->count: %d\n", scope->symtable->count);
		printf("symtable->n_alloc: %d\n", scope->symtable->n_alloc);
		printf("symtable->symbol_text.start: %p\n", scope->symtable->symbol_text.start);
		printf("symtable->symbol_text.len: %d\n", scope->symtable->symbol_text.len);
		fflush(stdout);
		for (i= 0; i < scope->symtable->count; i++) {
			ofs= (i == 0? 0 : scope->symtable->symbol_end[i-1]+1);
			printf("symbol %d: %d %s\n", i,
				scope->symtable->symbol_end[i] - ofs,
				scope->symtable->symbol_text.start + ofs
			);
		}
		fflush(stdout);
	}
	else {
		userp_env_print_diag(env, stdout);
	}
	return ret? 0 : 2;
}

#endif
