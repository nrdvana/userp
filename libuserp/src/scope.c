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
static bool scope_symbol_vec_alloc(userp_scope scope, size_t n);

// This handles both the case of limiting symbols to the 2**31 limit imposed by the hash table,
// and also guards against overflow of size_t for allocations on 32-bit systems.
#define MAX_SYMTABLE_STRUCT_ALLOC  (SIZE_MAX/sizeof(struct symbol_entry))
#define MAX_SYMTABLE_ENTRIES (MAX_SYMTABLE_STRUCT_ALLOC < (size_t)(1<<31)? MAX_SYMTABLE_STRUCT_ALLOC : (size_t)(1<<31))

static bool scope_symbol_vec_alloc(userp_scope scope, size_t n) {
	userp_env env= scope->env;
	struct symbol_tree_node15 *tree15;
	struct symbol_tree_node31 *tree31;
	size_t size, i;
	n= USERP_SCOPE_TABLE_ALLOC_ROUND(n);
	if (n <= scope->symtable.alloc)
		return true; // nothing to do
	// well that would be a lot of symbols
	if (n >= MAX_SYMTABLE_ENTRIES) {
		userp_diag_setf(&env->err, USERP_EDOINGITWRONG,
			"Can't resize symbol table larger than " USERP_DIAG_SIZE " entries",
			(size_t) MAX_SYMTABLE_ENTRIES
		);
		USERP_DISPATCH_ERROR(env);
		return false;
	}

	if (!USERP_ALLOC_ARRAY(env, &scope->symtable.symbols, n))
		return false;

	scope->symtable.alloc= n;
	return true;
}


#define HASHTREE_GET_BUCKET(hashtree, symcount, bucketcount, hash) \
	( ((symcount) >> 15)? (size_t) ((uint32_t *) (hashtree))[ (hash) % (bucketcount) ] \
	: ((symcount) >>  7)? (size_t) ((uint16_t *) (hashtree))[ (hash) % (bucketcount) ] \
	:                     (size_t) ((uint8_t  *) (hashtree))[ (hash) % (bucketcount) ] \
    )

#define HASHTREE_SET_BUCKET(hashtree, symcount, bucketcount, hash, value) \
	( ((symcount) >> 15)? (size_t) (((uint32_t *) (hashtree))[ (hash) % (bucketcount) ]= (uint32_t) (value)) \
	: ((symcount) >>  7)? (size_t) (((uint16_t *) (hashtree))[ (hash) % (bucketcount) ]= (uint16_t) (value)) \
	:                     (size_t) (((uint8_t  *) (hashtree))[ (hash) % (bucketcount) ]= (uint8_t ) (value)) \
    )

#define HASHTREE_HASHAREA_SIZE(symcount, bucketcount) \
	((bucketcount) * (((symcount) >> 15)? 4 : ((symcount) >> 7)? 2 : 1))

struct rbtree_node15 {
	uint16_t
		sym,
		hash,
		left,
		right: 15,
		color: 1;
};
struct rbtree_node31 {
	uint32_t
		sym,
		hash,
		left,
		right: 31,
		color: 1;
};

#define HASHTREE_IS_TREE31(symcount) ( (symcount) >> 15 )

#define HASHTREE_TREE15(hashtree, symcount, bucketcount) ( \
	(struct rbtree_node15*) ( \
		(((char*)(hashtree)) \
		+ HASHTREE_HASHAREA_SIZE(symcount,bucketcount) \
		+ 3) \
	& ~(size_t) 3 ) \
)

#define HASHTREE_TREE31(hashtree, symcount, bucketcount) ( \
	(struct rbtree_node31*) ( \
		(((char*)(hashtree)) \
		+ HASHTREE_HASHAREA_SIZE(symcount,bucketcount) \
		+ 3) \
	& ~(size_t) 3 ) \
)

#define HASHTREE_NODE_CMP(node1, node2, symbols) ( \
	(node1)->hash < (node2)->hash? -1 \
	: (node1)->hash > (node2)->hash? 1 \
	: strcmp( (symbols)[(node1)->sym].name, (symbols)[(node2)->sym].name ) \
)

bool userp_symtable_hashtree_get(struct userp_symtable *st, const char *name) {
	// Calculate a hash for 'name' using FNV-1a
	uint64_t hash= 0xcbf29ce484222325LL;
	for (p= name; *p;) {
		hash ^= *p++;
		hash *= 0x100000001b3;
	}
	hash ^= st->ht_salt;
	hash *= 0x100000001b3;
	// read the bucket for the hash
	int bucket_val= HASHTREE_GET_BUCKET(st->hashtree, st->processed, st->ht_buckets, hash);
	// If low bit is 1, there was a collision, and this is the root of a tree
	if (bucket_val & 1) {
		if (!HASHTREE_IS_TREE31(st->processed)) {
			tree15= HASHTREE_TREE15(st->hashtree, st->processed, st->ht_buckets);
			
		}
}

bool hashtree_insert

bool userp_symtable_update(struct userp_symtable *st) {
}

struct symbol_entry *userp_symtable_find(struct userp_symtable *st, const char *name) {
	if (st->used > st->processed)
		if (!userp_symtable_update(st))
			return NULL;
	// hash the input string
}


userp_symbol userp_scope_get_symbol(userp_scope scope, const char *name, int flags) {
	int i, pos, start, limit, cmp, len;
	bool need_tree= false;
	struct symbol_table *st;
	userp_env env= scope->env;

	// search self and parent scopes for symbol (but flags can request local-only)
	for (i= scope->symtable_count - 1; i >= 0; --i) {
		st= scope->symtable_stack[i];
		// Does this symtable have a current hashtable?  If not, build it.
		userp_symtable_
		if (st->processed < st->used) {
			userp_symtable_proces_symbols(st);
		// Now use the hashtable
		
		if (st->tree) {
			// tree search
			if (USE_TREE31(st->alloc)) { // more than 15 bits of entries uses a tree of 31-bit integers.
				tree31= st->tree31;
				for (pos= st->tree_root; pos;) {
					cmp= strcmp(name, st->symbols[pos].name);
					if (cmp == 0) return st->id_offset + pos;
					else if (cmp > 0) pos= tree31[pos].right;
					else pos= tree31[pos].left;
				}
			}
			else {
				tree15= st->tree15;
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
			// capture whether it was greater than all others in local symtable or not
			if (st == &scope->symtable && !scope->symtable.tree)
				need_tree= (limit < st->used);
		}
		// User can request only searching immediate scope
		if (flags & USERP_GET_LOCAL)
			break;
	}
	// Not found.  Does it need added?
	if (!(flags & USERP_CREATE))
		return 0;
	
	// In case it's the first symbol
	if (!scope->has_symbols)
		if (!scope_init_symtable(scope))
			return 0;
	// if scope is finalized, emit an error
	if (scope->is_final) {
		userp_diag_set(&env->err, USERP_ESCOPEFINAL, "Can't add symbol to a finalized scope");
		USERP_DISPATCH_ERROR(env);
		return 0;
	}
	pos= scope->symtable.used;
	// Grow the symbols array if needed
	if (pos >= scope->symtable.alloc)
		if (!scope_symbol_vec_alloc(scope, pos+1))
			return 0;
	// if tree is not built, and symbol does not compare greater than previous, need the tree.
	if (need_tree)
		if (!scope_symbol_tree_init(scope))
			return 0;
	len= strlen(name);
	// Copy the name into scope storage
	if (!(name= (char*) userp_bstr_append_bytes(&scope->symtable.chardata, (const uint8_t*) name, len+1, USERP_CONTIGUOUS)))
		return 0;
	// add symbol to the vector
	scope->symtable.symbols[pos].name= name; // name was replaced with the local pointer, above
	scope->symtable.symbols[pos].type_ref= 0;
	scope->symtable.used= pos+1;
	if (scope->symtable.tree) {
		// add symbol to the tree
		if (!symbol_tree_insert(&scope->symtable, pos, pos+1))
			return 0;
	}
	return pos + scope->symtable.id_offset;
}

struct symbol_parse_state {
	uint8_t
		*start,           // character where the most recent token started
		*limit,           // one-beyond-end of buffer to parse
		*pos,             // current character to consider
		*prev;            // previous symbol, used for testing 'sorted' status
	struct symbol_entry
		*dest_pos,        // current position for recording the next symbol
		*dest_lim,        // one-beyond-end of the symbol buffer
	    *dest_last_sorted;// last element which compared greater than the previous
	bool sorted;          // whether elements found so far are in correct order
	struct userp_diag diag;
};
static bool parse_symbols(struct symbol_parse_state *parse) {
	const uint8_t
		*limit= parse->limit,
		*pos= parse->pos;
	uint_least32_t codepoint;

	while (pos < limit && parse->dest_pos < parse->dest_lim) {
		parse->start= (uint8_t*) pos;
		// Begin one symbol
		while (1) {
			// 1-byte code: 0x00-0x7F
			// 2-byte code: 0xE0-0xEF + 6 bits
			// 3-byte code: 0xF0-0xF7 + 12 bits
			// 4-byte code: 0xF8-0xFB + 18 bits
			// larger byte sequences are currently disallowed by unicode standard
			// continuation bytes are all 0xC0-0xDF
			// Nothing above cares about the low 3 bits except 4-byte code, so optimize the switch a bit
			switch (pos[0] >> 3) {
			// Disallow control chars
			case 0:
				if (!pos[0])
					goto end_of_string;
			case 1: case 2: case 3:
				codepoint= pos[0];
				goto invalid_char;
			case 15:
				if (pos[0] == 0x7F) {
					codepoint= pos[0];
					goto invalid_char;
				}
			case 4: case 5: case 6: case 7: case 8: case 9: case 10: case 11: case 12: case 13: case 14:
				if (pos+1 >= limit)
					goto invalid_eof;
				++pos;
				continue; // valid single-byte char
			case 0b11100: case 0b11101:
				if (pos + 1 + 1 >= limit)
					goto invalid_eof;
				if ((pos[1] >> 6) != 2)
					goto invalid_encoding;
				codepoint= ((uint_least32_t)(pos[0] & 0x0F) << 6)
						| (uint_least32_t)(pos[1] & 0x3F);
				if (!(codepoint >> 7))
					goto invalid_overlong;
				pos += 2;
				break;
			case 0b11110:
				if (pos + 2 + 1 >= limit)
					goto invalid_eof;
				if ((pos[1] >> 6) != 2 || (pos[2] >> 6) != 2)
					goto invalid_encoding;
				codepoint= ((((uint_least32_t)(pos[0] & 0x07) << 6)
						| (uint_least32_t)(pos[1] & 0x3F)) << 6)
						| (uint_least32_t)(pos[2] & 0x3F);
				if (!(codepoint >> 10))
					goto invalid_overlong;
				pos += 3;
				break;
			case 0b11111:
				// need to also check the bit that got clipped on shift-right
				if (!(pos[0] & 4)) {
					if (pos + 3 + 1 >= limit)
						goto invalid_eof;
					if ((pos[1] >> 6) != 2 || (pos[2] >> 6) != 2 || (pos[3] >> 6) != 2)
						goto invalid_encoding;
					codepoint= (((((
						(uint_least32_t)(pos[0] & 0x03) << 6)
						| (uint_least32_t)(pos[1] & 0x3F)) << 6)
						| (uint_least32_t)(pos[2] & 0x3F)) << 6)
						| (uint_least32_t)(pos[3] & 0x3F);
					if (!(codepoint >> 15))
						goto invalid_overlong;
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
		if (pos == parse->start) {
			codepoint= 0;
			goto invalid_char;
		}
		if (parse->sorted) {
			if (parse->prev && strcmp((char*)parse->prev, (char*)parse->start) >= 0) {
				parse->dest_last_sorted= parse->dest_pos - 1;
				parse->sorted= false;
			}
			parse->prev= parse->start;
		}
		parse->dest_pos->name= (char*)parse->start;
		++parse->dest_pos;
		++pos; // resume parsing at char beyond '\0'
	}
	parse->pos= (uint8_t*) pos;
	return true;

	CATCH(invalid_encoding) {
		userp_diag_setf(&parse->diag, USERP_ESYMBOL, "Symbol table: encountered invalid UTF-8 sequence at " USERP_DIAG_BUFSTR,
			parse->start, (size_t)(pos - parse->start), (size_t)(pos - parse->start + 1));
	}
	CATCH(invalid_overlong) {
		userp_diag_setf(&parse->diag, USERP_ESYMBOL, "Symbol table: encountered over-long UTF-8 sequence at " USERP_DIAG_BUFSTR,
			parse->start, (size_t)(pos - parse->start), (size_t)(pos - parse->start + 1));
	}
	CATCH(invalid_char) {
		userp_diag_setf(&parse->diag, USERP_ESYMBOL, "Symbol table: encountered forbidden codepoint " USERP_DIAG_SIZE " at " USERP_DIAG_BUFSTR,
			(size_t)codepoint,
			parse->start, (size_t)(pos - parse->start), (size_t)(pos - parse->start + 1));
	}
	CATCH(invalid_eof) {
		userp_diag_set(&parse->diag, USERP_EOVERRUN, "Symbol table ended mid-symbol");
	}
	parse->pos= (uint8_t*) pos;
	return false;
}

bool userp_scope_parse_symbols(userp_scope scope, struct userp_bstr_part *parts, size_t part_count, int sym_count, int flags) {
	userp_env env= scope->env;
	struct userp_bstr_part *part, *part2, *plim;
	userp_buffer buf;
	bool success;
	struct symbol_parse_state parse;
	size_t n, n_sorted, i, segment,
		orig_sym_used, orig_sym_partcnt, syms_added,
		pos_ofs, lastsort_ofs;
	uint8_t *p1, *p2;

	// If scope is finalized, emit an error
	if (scope->is_final) {
		userp_diag_set(&env->err, USERP_ESCOPEFINAL, "Can't add symbol to a finalized scope");
		USERP_DISPATCH_ERROR(env);
		return false;
	}
	// If no input, return
	if (!part_count || (part_count == 1 && !parts[0].len)) {
		if (!sym_count)
			return true;
		userp_diag_setf(&env->err, USERP_EOVERRUN, "Can't parse " USERP_DIAG_COUNT " symbols from empty buffer",
			(size_t) sym_count);
		USERP_DISPATCH_ERROR(env);
		return false;
	}
	// Initialize chardata (if not already) and ensure space for input.part_count*2 - 1 new parts.
	// There can be at most one part added for each input part and one for each boundary between input parts.
	if (!scope->has_symbols)
		if (!scope_init_symtable(scope))
			return false;
	n= scope->symtable.chardata.part_count + part_count*2 - 1;
	if (n > scope->symtable.chardata.part_alloc)
		if (!userp_bstr_partalloc(&scope->symtable.chardata, n))
			return false;
	
	// ensure symbol vector has sym_count slots allocated (if sym_count provided)
	// if sym_coun not given, start with 32. TODO: make it configurable.
	n= scope->symtable.used + (sym_count > 0? sym_count : 32);
	if (n > scope->symtable.alloc)
		if (!scope_symbol_vec_alloc(scope, n))
			return false;
	// Record the original status of the symbol table, to be able to revert changes
	orig_sym_used= scope->symtable.used;
	orig_sym_partcnt= scope->symtable.chardata.part_count;
	// Set up the parser's view of the situation
	bzero(&parse, sizeof(parse));
	parse.dest_pos= scope->symtable.symbols + scope->symtable.used;
	parse.dest_lim= scope->symtable.symbols
		+ (sym_count? scope->symtable.used+sym_count : scope->symtable.alloc);
	parse.sorted= !scope->symtable.tree;
	parse.dest_last_sorted= parse.sorted? parse.dest_pos : NULL;
	parse.prev= scope->symtable.used <= 1? NULL
		: (uint8_t*) scope->symtable.symbols[scope->symtable.used-1].name;
	parse.pos= parts[0].data;
	parse.limit= parts[0].data + parts[0].len;
	// loop through the parts of input
	for (part= parts, plim= parts + part_count; part < plim;) {
		p1= parse.pos;
		while (
			(success= parse_symbols(&parse))
			// If the parse ran out of symbol table slots (which will only happen if
			//  part_count was not known) allocate more and try again.
			&& part_count == 0 && parse.dest_pos == parse.dest_lim && parse.pos < parse.limit
		) {
			// If the vector gets reallocated to a new address, need to update the pointers in parse
			pos_ofs= parse.dest_pos - scope->symtable.symbols;
			lastsort_ofs= parse.dest_last_sorted - scope->symtable.symbols;
			// Perform re-alloc
			if (!scope_symbol_vec_alloc(scope, scope->symtable.alloc * 2)) {
				success= false;
				break;
			}
			// Repair parse pointers
			parse.dest_pos= scope->symtable.symbols + pos_ofs;
			parse.dest_lim= scope->symtable.symbols + scope->symtable.alloc;
			if (parse.dest_last_sorted)
				parse.dest_last_sorted= scope->symtable.symbols + lastsort_ofs;
		}
		// Add all consumed bytes to the chardata string
		syms_added= parse.dest_pos - scope->symtable.symbols - scope->symtable.used;
		if (syms_added) {
			if (!userp_grab_buffer(part->buf))
				success= false;
			else {
				// vector space was pre-allocated above
				part2= scope->symtable.chardata.parts + scope->symtable.chardata.part_count++;
				part2->buf= part->buf;
				part2->data= p1;
				part2->len= (success? parse.pos : parse.start) - p1;
				scope->symtable.used += syms_added;
			}
		}
		if (success) {
			// set up the next loop iteration
			part++;
			parse.pos= part->data;
			parse.limit= part->data + part->len;
		}
		// check for failure due to symbol split between parts
		else if (parse.diag.code == USERP_EOVERRUN && part+1 < plim) {
			// Make a temporary buffer to store the entire unbroken symbol,
			// then parse again to verify unicode status.
			n= parse.limit - parse.start;
			// find end of the symbol in next buffer (or any buffer after)
			for (part2= part+1; part2 < plim; part2++) {
				for (p1= part2->data, p2= part2->data + part2->len; p1 < p2 && *p1; p1++);
				n += p1 - part2->data;
				if (p1 < p2)
					break;
			}
			if (part2 >= plim)
				goto parse_failure; // the error code of EOF was accurate afterall
			// found the end of the symbol
			++n; // include the NUL terminator
			++p1;
			parse.diag.code= 0;
			// need a new buffer n bytes long
			if (!(buf= userp_new_buffer(env, NULL, n, USERP_HINT_STATIC)))
				goto failure;
			// Now repeat the iteration copying the data
			i= parse.limit - parse.start;
			memcpy(buf->data, parse.start, i);
			for (part2= part; i < n;) {
				++part2;
				segment= n - i < part2->len? n - i : part2->len;
				memcpy(buf->data + i, part2->data, segment);
				i+= segment;
			}
			// Now have the symbol loaded into a single buffer, re-parse it.
			parse.pos= buf->data;
			parse.limit= buf->data + n;
			// If success, it will have parsed exactly one symbol, and this buffer
			// needs added to the chardata
			if (!parse_symbols(&parse)) {
				userp_drop_buffer(buf);
				goto parse_failure;
			}
			// set up the next loop iteration
			part= part2;
			parse.pos= p1;
			parse.limit= p2;
			// record the new part holding this buffer
			part2= scope->symtable.chardata.parts + scope->symtable.chardata.part_count++;
			part2->buf= buf; // already has refcnt of 1.
			part2->data= buf->data;
			part2->len= n;
		}
		else goto parse_failure;
	}
	// If the input ran out of characters before sym_count (and provided), emit an error
	if (sym_count && scope->symtable.used - orig_sym_used < sym_count) {
		userp_diag_setf(&env->err, USERP_EOVERRUN,
			"Symbol table: only found " USERP_DIAG_POS " of " USERP_DIAG_SIZE " symbols before end of buffer",
			(size_t) (scope->symtable.used - orig_sym_used),
			(size_t) sym_count
		);
		USERP_DISPATCH_ERROR(env);
		goto failure;
	}		
	// If the input had extra characters, no problem.  Maybe add a flag to error on that?
	// If the tree is already being used, or if the elements were not in sorted order,
	// need to add every new element to the tree.
	if (!parse.sorted) {
		n_sorted= !parse.dest_last_sorted? orig_sym_used
			: parse.dest_last_sorted - scope->symtable.symbols;
		if (!scope->symtable.tree) {
			// temporarily replace 'used' with the number of elements known to be in sorted order
			// so that the tree builds those more efficiently
			n= scope->symtable.used;
			scope->symtable.used= n_sorted;
			success= scope_symbol_tree_init(scope);
			// restore the correct 'used' value
			scope->symtable.used= n;
			if (!success)
				goto failure;
		}
		// remainder are un-sorted, and each needs a tree insert
		return symbol_tree_insert(&scope->symtable, n_sorted, scope->symtable.used);
	}
	return true;

	CATCH(failure) {
		CATCH(parse_failure) {
			memcpy(&env->err, &parse.diag, sizeof(parse.diag));
			USERP_DISPATCH_ERROR(env);
		}
		// Remove new additions to the symbol table
		for (i= orig_sym_partcnt; i < scope->symtable.chardata.part_count; i++)
			userp_drop_buffer(scope->symtable.chardata.parts[i].buf);
		scope->symtable.chardata.part_count= orig_sym_partcnt;
		scope->symtable.used= orig_sym_used;
	}
	return false;
}

//bool userp_validate_symbol(const char *buf, size_t buflen) {
//	struct symbol_parse_state pst;
//	size_t end;
//	bzero(&pst, sizeof(pst));
//	pst.ends= &end;
//	pst.ends_max= 1;
//	return parse_symbols(&pst, buf, buflen) && pst.pos == buflen;
//}

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
	if (scope->has_symbols)
		return true;
	if (scope->is_final) {
		userp_diag_set(&scope->env->err, USERP_ESCOPEFINAL, "Can't add symbols to a finalized scope");
		USERP_DISPATCH_ERROR(scope->env);
		return false;
	}

	scope->symtable.id_offset= !scope->symtable_count? 0
		: scope->symtable_stack[scope->symtable_count-1]->id_offset
		  + scope->symtable_stack[scope->symtable_count-1]->used
		  - 1; // because slot 0 of each symbol table is used as a NUL element.

	// When allocated, the userp_scope left room in symtable_stack for one extra
	scope->symtable_stack[scope->symtable_count++]= &scope->symtable;
	scope->symtable.chardata.env= scope->env;
	scope->symtable.used= 1; // elem 0 is always reserved
	scope->has_symbols= true;
	return scope_symbol_vec_alloc(scope, 1);
	// All other fields were blanked during the userp_scope constructor
}

static void userp_free_scope(userp_scope scope) {
	userp_env env= scope->env;
	userp_scope parent= scope->parent;
	userp_bstr_destroy(&scope->symtable.chardata);
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

static bool symbol_tree_insert31(struct symbol_table *st, uint32_t newest, uint32_t limit) {
	// element 0 is the "sentinel" leaf, and 'last' is the element being added
	struct symbol_tree_node31 *tree= st->tree31;
	struct symbol_entry *symbols= st->symbols;
	int cmp;
	uint32_t stack[64], pos, parent, new_head, i;
	
	while (newest < limit) {
		assert(tree != NULL);
		assert(tree[0].left == 0);
		assert(tree[0].right == 0);
		assert(tree[0].color == 0);

		tree[newest].left= 0;
		tree[newest].right= 0;
		
		// If there is no root, the new node is the root.
		if (!st->tree_root) {
			st->tree_root= newest;
			tree[newest++].color= 0;
			continue;
		}
		tree[newest].color= 1;
		
		// leaf-sentinel will double as the head-sentinel
		stack[i= 0]= 0;
		pos= (uint16_t) st->tree_root;
		while (pos && i < 64) {
			stack[++i]= pos;
			cmp= strcmp(symbols[newest].name, symbols[pos].name);
			pos= cmp < 0? tree[pos].left : tree[pos].right;
		}
		if (i >= 64) {
			assert(i < 64);
			return false;
		}
		pos= stack[i--];
		//printf("insert at %s of %d (%s depth %d)\n", (cmp < 0? "left":"right"), (int)pos, tree[pos].color? "red":"black", (int)i);
		if (cmp < 0)
			tree[pos].left= newest;
		else
			tree[pos].right= newest;
		// balance the tree.  Pos points at the parent node of the new node.
		// if current is a black node, no rotations needed
		while (i > 0 && tree[pos].color) {
			// current is red, the imbalanced child is red, and parent is black.
			parent= stack[i];
			assert(tree[parent].color == 0);
			// if the current is on the right of the parent, the parent is to the left
			if (tree[parent].right == pos) {
				// if the sibling is also red, we can pull down the color black from the parent
				if (tree[tree[parent].left].color) {
					tree[tree[parent].left].color= 0;
					tree[pos].color= 0;
					tree[parent].color= 1;
					// jump twice up the tree
					pos= stack[--i];
					i--;
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
				new_head= tree[parent].right;
				pos= parent;
				parent= stack[--i];
				if (!parent) st->tree_root= new_head;
				else if (tree[parent].right == pos) tree[parent].right= new_head;
				else tree[parent].left= new_head;
				tree[pos].right= tree[new_head].left;
				tree[new_head].left= pos;
				tree[pos].color= 1;
				tree[new_head].color= 0;
				break; // balanced
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
					i--;
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
				new_head= tree[parent].left;
				pos= parent;
				parent= stack[--i];
				if (!parent) st->tree_root= new_head;
				else if (tree[parent].right == pos) tree[parent].right= new_head;
				else tree[parent].left= new_head;
				tree[pos].left= tree[new_head].right;
				tree[new_head].right= pos;
				tree[pos].color= 1;
				tree[new_head].color= 0;
				break; // balanced
			}
		}
		// ensure the root node is black
		tree[st->tree_root].color= 0;
		newest++;
	}
	return true;
}

static bool symbol_tree_insert15(struct symbol_table *st, uint16_t newest, uint16_t limit) {
	// element 0 is the "sentinel" leaf, and 'last' is the element being added
	struct symbol_tree_node15 *tree= st->tree15;
	struct symbol_entry *symbols= st->symbols;
	int cmp, i;
	uint16_t stack[64], pos, parent, new_head;

	while (newest < limit) {
		assert(tree != NULL);
		assert(tree[0].left == 0);
		assert(tree[0].right == 0);
		assert(tree[0].color == 0);

		tree[newest].left= 0;
		tree[newest].right= 0;
		
		// If there is no root, the new node is the root.
		if (!st->tree_root) {
			st->tree_root= newest;
			tree[newest++].color= 0;
			continue;
		}
		tree[newest].color= 1;
		
		// leaf-sentinel will double as the head-sentinel
		stack[i= 0]= 0;
		pos= (uint16_t) st->tree_root;
		while (pos && i < 32) {
			stack[++i]= pos;
			cmp= strcmp(symbols[newest].name, symbols[pos].name);
			pos= cmp < 0? tree[pos].left : tree[pos].right;
		}
		if (i >= 32) {
			assert(i < 32);
			return false;
		}
		pos= stack[i--];
		//printf("insert at %s of %d (%s depth %d)\n", (cmp < 0? "left":"right"), (int)pos, tree[pos].color? "red":"black", (int)i);
		if (cmp < 0)
			tree[pos].left= newest;
		else
			tree[pos].right= newest;
		// balance the tree.  Pos points at the parent node of the new node.
		// if current is a black node, no rotations needed
		while (i > 0 && tree[pos].color) {
			// current is red, the imbalanced child is red, and parent is black.
			parent= stack[i];
			assert(tree[parent].color == 0);
			// if the current is on the right of the parent, the parent is to the left
			if (tree[parent].right == pos) {
				// if the sibling is also red, we can pull down the color black from the parent
				if (tree[tree[parent].left].color) {
					tree[tree[parent].left].color= 0;
					tree[pos].color= 0;
					tree[parent].color= 1;
					// jump twice up the tree
					pos= stack[--i];
					i--;
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
				new_head= tree[parent].right;
				pos= parent;
				parent= stack[--i];
				if (!parent) st->tree_root= new_head;
				else if (tree[parent].right == pos) tree[parent].right= new_head;
				else tree[parent].left= new_head;
				tree[pos].right= tree[new_head].left;
				tree[new_head].left= pos;
				tree[pos].color= 1;
				tree[new_head].color= 0;
				break; // balanced
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
					i--;
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
				new_head= tree[parent].left;
				pos= parent;
				parent= stack[--i];
				if (!parent) st->tree_root= new_head;
				else if (tree[parent].right == pos) tree[parent].right= new_head;
				else tree[parent].left= new_head;
				tree[pos].left= tree[new_head].right;
				tree[new_head].right= pos;
				tree[pos].color= 1;
				tree[new_head].color= 0;
				break; // balanced
			}
		}
		// ensure the root node is black
		tree[st->tree_root].color= 0;
		newest++;
	}
	return true;
}

static bool symbol_tree_walk31(struct symbol_table *st, const char *key, bool (*walk_cb)(void *context, int sym_ofs), void *context) {
	struct symbol_tree_node31 *tree;
	uint32_t node, parent[64];
	int parent_pos, cmp;

	assert(st->tree);
	assert(st->tree31[0].left == 0);
	assert(st->tree31[0].right == 0);
	assert(st->tree31[0].color == 0);

	tree= st->tree31;
	parent[parent_pos= 0]= 0;
	node= (uint32_t) st->tree_root;
	// seek initial node, if given
	if (key) {
		while (node && parent_pos < 64) {
			cmp= strcmp(key, st->symbols[node].name);
			if (cmp == 0) break;
			parent[++parent_pos]= node;
			node= (cmp > 0)? tree[node].right : tree[node].left;
		}
		if (node) // ran out of stack, tree is corrupt
			return false;
		node= parent[parent_pos--];
		goto emit_node;
	}
	// Now begin iterating
	{
	seek_left:
		while (tree[node].left) {
			if (parent_pos+1 >= 64) return false; // tree corrupt
			parent[++parent_pos]= node;
			node= tree[node].left;
		}
	emit_node:
		if (!walk_cb(context, node))
			return true; // user requested end, which is success
	//step_right:
		// start over from right subtree, if any
		if (tree[node].right) {
			if (parent_pos+1 >= 64) return false; // tree corrupt
			parent[++parent_pos]= node;
			node= tree[node].right;
			goto seek_left;
		}
		// else walk upward while this node was to the right of the parent
		while (tree[parent[parent_pos]].right == node)
			node= parent[parent_pos--];
		// then up one more parent
		node= parent[parent_pos--];
		if (node) goto emit_node;
	}
	return true;
}

static bool symbol_tree_walk15(struct symbol_table *st, const char *key, bool (*walk_cb)(void *context, int sym_ofs), void *context) {
	struct symbol_tree_node15 *tree;
	uint16_t node, parent[32];
	int parent_pos, cmp;

	assert(st->tree);
	assert(st->tree15[0].left == 0);
	assert(st->tree15[0].right == 0);
	assert(st->tree15[0].color == 0);

	tree= st->tree15;
	parent[0]= 0;
	parent_pos= 0;
	node= (uint16_t) st->tree_root;
	// seek initial node, if given
	if (key) {
		while (node && parent_pos < 32) {
			cmp= strcmp(key, st->symbols[node].name);
			if (cmp == 0) break;
			parent[++parent_pos]= node;
			node= (cmp > 0)? tree[node].right : tree[node].left;
		}
		if (node) // ran out of stack, tree is corrupt
			return false;
		node= parent[parent_pos--];
		goto emit_node;
	}
	// Now begin iterating
	{
	seek_left:
		while (tree[node].left) {
			if (parent_pos+1 >= 32) return false; // tree corrupt
			parent[++parent_pos]= node;
			node= tree[node].left;
		}
	emit_node:
		if (!walk_cb(context, node))
			return true; // user requested end, which is success
	//step_right:
		// start over from right subtree, if any
		if (tree[node].right) {
			if (parent_pos+1 >= 32) return false; // tree corrupt
			parent[++parent_pos]= node;
			node= tree[node].right;
			goto seek_left;
		}
		// else walk upward while this node was to the right of the parent
		while (tree[parent[parent_pos]].right == node)
			node= parent[parent_pos--];
		// then up one more parent
		node= parent[parent_pos--];
		if (node) goto emit_node;
	}
	return true;
}

#ifdef UNIT_TEST

struct verify_symbol_tree {
	userp_scope scope;
	int prev;
};
bool verify_symbol_tree__checknode(struct verify_symbol_tree *v, int node) {
	if (v->prev)
		if (strcmp(v->scope->symtable.symbols[v->prev].name, v->scope->symtable.symbols[node].name) >= 0) {
			printf("Tree nodes out of order!  '%s' -> '%s'\n",
				v->scope->symtable.symbols[v->prev].name, v->scope->symtable.symbols[node].name);
			return false;
		}
	v->prev= node;
	return true;
}
void verify_symbol_tree(userp_scope scope) {
	struct verify_symbol_tree v= { .scope= scope, .prev= 0 };
	size_t i;
	// verify each red node does not have a red child
	if (USE_TREE31(scope->symtable.alloc)) {
		if (scope->symtable.tree31[0].left || scope->symtable.tree31[0].right
			|| scope->symtable.tree31[0].color)
		{
			printf("Tree leaf sentinel is corrupt!\n");
			return;
		}
		for (i= 0; i < scope->symtable.used; i++) {
			if (scope->symtable.tree31[i].color) {
				if (scope->symtable.tree31[scope->symtable.tree31[i].left].color
					|| scope->symtable.tree31[scope->symtable.tree31[i].right].color
				) {
					printf("Tree node color corrupt!\n");
					return;
				}
			}
		}
	} else {
		if (scope->symtable.tree15[0].left || scope->symtable.tree15[0].right
			|| scope->symtable.tree15[0].color)
		{
			printf("Tree leaf sentinel is corrupt!\n");
			return;
		}
		for (i= 0; i < scope->symtable.used; i++) {
			if (scope->symtable.tree15[i].color) {
				if (scope->symtable.tree15[scope->symtable.tree15[i].left].color
					|| scope->symtable.tree15[scope->symtable.tree15[i].right].color
				) {
					printf("Tree node color corrupt!\n");
					return;
				}
			}
		}
	}
	// ensure all nodes are in sorted order
	if (!symbol_tree_walk(&scope->symtable, NULL, (bool(*)(void*,int)) verify_symbol_tree__checknode, &v))
		printf("Tree corrupt!\b");
}

void dump_scope(userp_scope scope) {
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
	printf("   local table: %d/%d %s\n",
		(int)scope->symtable.used, (int)scope->symtable.alloc,
		!scope->symtable.tree? "binary-search"
			: USE_TREE31(scope->symtable.alloc)? "31-bit-tree"
			: "15-bit-tree"
	);
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
	// If the tree is used, verify it's structure
	if (scope->symtable.tree)
		verify_symbol_tree(scope);
	printf("  Type Table: stack of %d tables, %d types\n",
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

UNIT_TEST(scope_symbol_sorted) {
	char buf[32];
	int i;
	userp_env env= userp_new_env(logging_alloc, userp_file_logger, stdout, 0);
	userp_scope scope= userp_new_scope(env, NULL);
	for (i= 0; i < 1000; i++) {
		snprintf(buf, sizeof(buf), "%8d", i);
		if (!userp_scope_get_symbol(scope, buf, USERP_CREATE))
			printf("Failed to add %s\n", buf);
	}
	dump_scope(scope);
	userp_drop_scope(scope);
	userp_drop_env(env);
}
/*OUTPUT
/alloc 0x0+ to \d+ = 0x\w+/
/alloc 0x0+ to \d+ = 0x\w+/
/alloc 0x0+ to \d+ = 0x\w+/
/alloc 0x0+ to \d+ = 0x\w+/
/alloc 0x0+ to \d+ = 0x\w+/
/alloc 0x0+ to \d+ = 0x\w+ POINTER_IS_BUFFER_DATA/
Scope level=0  refcnt=1 has_symbols
  Symbol Table: stack of 1 tables, 5 symbols
   local table: 6/256 binary-search
       buffers:  [0-20]/4096
  Type Table: stack of 0 tables, 0 types
/alloc 0x\w+ to 0 = 0x0+ POINTER_IS_BUFFER_DATA/
/alloc 0x\w+ to 0 = 0x0+/
/alloc 0x\w+ to 0 = 0x0+/
/alloc 0x\w+ to 0 = 0x0+/
/alloc 0x\w+ to 0 = 0x0+/
/alloc 0x\w+ to 0 = 0x0+/
*/

UNIT_TEST(scope_symbol_treesort) {
	char buf[32];
	int i;
	userp_env env= userp_new_env(NULL, userp_file_logger, stdout, 0);
	userp_scope scope= userp_new_scope(env, NULL);
	// Insert two nodes out of order
	printf("# two out-of-order nodes\n");
	buf[0]= 'b'; buf[1]= '\0';
	if (!userp_scope_get_symbol(scope, buf, USERP_CREATE))
		printf("Failed to add %s\n", buf);
	printf("tree is %s\n", scope->symtable.tree? "allocated" : "null");
	buf[0]= 'a';
	if (!userp_scope_get_symbol(scope, buf, USERP_CREATE))
		printf("Failed to add %s\n", buf);
	printf("tree is %s\n", scope->symtable.tree? "allocated" : "null");
	// Now start over
	printf("# reset\n");
	userp_drop_scope(scope);
	scope= userp_new_scope(env, NULL);
	printf("# 10K sorted nodes\n");
	// Insert some nodes in order
	for (i= 20000; i < (1<<15)-1; i++) {
		snprintf(buf, sizeof(buf), "%05d", i);
		if (!userp_scope_get_symbol(scope, buf, USERP_CREATE))
			printf("Failed to add %s\n", buf);
	}
	// The tree should not be created yet
	printf("tree is %s\n", scope->symtable.tree? "allocated" : "null");
	printf("# 1 node out of order\n");
	// Now insert one node out of order, causing it to tree up everything
	snprintf(buf, sizeof(buf), "%05d", 9999);
	if (!userp_scope_get_symbol(scope, buf, USERP_CREATE))
		printf("Failed to add %s\n", buf);
	printf("tree is %s\n", scope->symtable.tree? "allocated" : "null");
	if (!scope->symtable.tree)
		return;
	verify_symbol_tree(scope);
	
	// Let it allocate right up to the limit of the 15-bit tree implementation.
	scope_symbol_vec_alloc(scope, 1<<15);

	printf("# 10K nodes decreasing\n");
	for (i= 9999; i >= 0; i--) {
		snprintf(buf, sizeof(buf), "%05d", i);
		if (!userp_scope_get_symbol(scope, buf, USERP_CREATE))
			printf("Failed to add %s\n", buf);
	}

	printf("# 10K nodes into the middle of the tree\n");
	for (i= 0; i < 5000; i++) {
		snprintf(buf, sizeof(buf), "%05d", 10000 + i);
		if (!userp_scope_get_symbol(scope, buf, USERP_CREATE))
			printf("Failed to add %s\n", buf);
		snprintf(buf, sizeof(buf), "%05d", 19999 - i);
		if (!userp_scope_get_symbol(scope, buf, USERP_CREATE))
			printf("Failed to add %s\n", buf);
	}
	dump_scope(scope);
	
	printf("# insert one more node forcing it to upgrade to 31-bit tree\n");
	snprintf(buf, sizeof(buf), "%05d", 1<<15);
	if (!userp_scope_get_symbol(scope, buf, USERP_CREATE))
		printf("Failed to add %s\n", buf);
	dump_scope(scope);

	userp_drop_scope(scope);
	userp_drop_env(env);
}
/*OUTPUT
/.* ?/
*/

static const char symbol_data_sorted[]=
	"ace\0bat\0car\0dog\0egg";
static struct userp_buffer symbol_data_sorted_buf= {
	.data= (uint8_t*) symbol_data_sorted,
	.alloc_len= sizeof(symbol_data_sorted)
};

static const char symbol_data_sorted2[]=
	"fun\0get\0has\0imp\0jam";
static struct userp_buffer symbol_data_sorted2_buf= {
	.data= (uint8_t*) symbol_data_sorted2,
	.alloc_len= sizeof(symbol_data_sorted2)
};

UNIT_TEST(scope_parse_symtable_sorted) {
	userp_env env= userp_new_env(logging_alloc, userp_file_logger, stdout, 0);
	userp_scope scope= userp_new_scope(env, NULL);
	struct userp_bstr_part str[]= {
		{ .buf= userp_new_buffer(env, (void*) symbol_data_sorted, sizeof(symbol_data_sorted), 0),
		  .len= sizeof(symbol_data_sorted),
		  .data= (uint8_t*) symbol_data_sorted,
		}
	};
	bool ret= userp_scope_parse_symbols(scope, str, 1, 5, 0);
	printf("return: %d\n", (int)ret);
	if (ret)
		dump_scope(scope);
	else
		userp_diag_print(&env->err, stdout);
	printf("# drop buffer\n");
	userp_drop_buffer(str[0].buf);
	printf("# drop scope\n");
	userp_drop_scope(scope);
	printf("# drop env\n");
	userp_drop_env(env);
}
/*OUTPUT
/alloc 0x0+ to \d+ = 0x\w+/
/alloc 0x0+ to \d+ = 0x\w+/
/alloc 0x0+ to \d+ = 0x\w+/
/alloc 0x0+ to \d+ = 0x\w+/
/alloc 0x0+ to \d+ = 0x\w+/
return: 1
Scope level=0  refcnt=1 has_symbols
  Symbol Table: stack of 1 tables, 5 symbols
   local table: 6/256 binary-search
       buffers:  [0-20]/20
  Type Table: stack of 0 tables, 0 types
# drop buffer
# drop scope
/alloc 0x\w+ to 0 = 0x0+/
/alloc 0x\w+ to 0 = 0x0+/
/alloc 0x\w+ to 0 = 0x0+/
/alloc 0x\w+ to 0 = 0x0+/
# drop env
/alloc 0x\w+ to 0 = 0x0+/
*/


/* This test parses a buffer of two segments each having a perfectly aligned
 * list of symbols.  It should translate to two succesful calls to to
 * parse_symbols(), each returning true.
 */
UNIT_TEST(scope_parse_split_symtable) {
	userp_env env= userp_new_env(logging_alloc, userp_file_logger, stdout, 0);
	userp_scope scope= userp_new_scope(env, NULL);
	struct userp_bstr_part str[]= {
		{ .buf= &symbol_data_sorted_buf,
		  .data= symbol_data_sorted_buf.data,
		  .len=  symbol_data_sorted_buf.alloc_len
		},
		{ .buf= &symbol_data_sorted2_buf,
		  .data= symbol_data_sorted2_buf.data,
		  .len=  symbol_data_sorted2_buf.alloc_len
		},
	};
	bool ret= userp_scope_parse_symbols(scope, str, 2, 10, 0);
	printf("return: %d\n", (int)ret);
	if (ret)
		dump_scope(scope);
	else
		userp_diag_print(&env->err, stdout);
	printf("# drop scope\n");
	userp_drop_scope(scope);
	printf("# drop env\n");
	userp_drop_env(env);
}
/*OUTPUT
/alloc 0x0+ to \d+ = 0x\w+/
/alloc 0x0+ to \d+ = 0x\w+/
/alloc 0x0+ to \d+ = 0x\w+/
/alloc 0x0+ to \d+ = 0x\w+/
return: 1
Scope level=0  refcnt=1 has_symbols
  Symbol Table: stack of 1 tables, 10 symbols
   local table: 11/256 binary-search
       buffers:  [0-20]/20  [0-20]/20
  Type Table: stack of 0 tables, 0 types
# drop scope
/alloc 0x\w+ to 0 = 0x0+/
/alloc 0x\w+ to 0 = 0x0+/
/alloc 0x\w+ to 0 = 0x0+/
# drop env
/alloc 0x\w+ to 0 = 0x0+/
*/

static const char symbol_fragment[]= "fragment1\0fragment2";
static struct userp_buffer symbol_fragment_buf= {
	.data= (uint8_t*) symbol_fragment,
	.alloc_len= sizeof(symbol_fragment)-1
};

UNIT_TEST(scope_parse_split_symbol) {
	userp_env env= userp_new_env(logging_alloc, userp_file_logger, stdout, 0);
	userp_scope scope= userp_new_scope(env, NULL);
	struct userp_bstr_part str[]= {
		{ .buf= &symbol_fragment_buf,
		  .data= symbol_fragment_buf.data,
		  .len=  symbol_fragment_buf.alloc_len
		},
		{ .buf= &symbol_data_sorted2_buf,
		  .data= symbol_data_sorted2_buf.data,
		  .len=  symbol_data_sorted2_buf.alloc_len
		},
	};
	bool ret= userp_scope_parse_symbols(scope, str, 2, 6, 0);
	printf("return: %d\n", (int)ret);
	if (ret)
		dump_scope(scope);
	else
		userp_diag_print(&env->err, stdout);
	printf("# drop scope\n");
	userp_drop_scope(scope);
	printf("# drop env\n");
	userp_drop_env(env);
}
/*OUTPUT
/alloc 0x0 to \d+ = 0x\w+/
/alloc 0x0 to \d+ = 0x\w+/
/alloc 0x0 to \d+ = 0x\w+/
/alloc 0x0 to \d+ = 0x\w+/
/alloc 0x0 to \d+ = 0x\w+/
/alloc 0x0 to 13 = 0x\w+ POINTER_IS_BUFFER_DATA/
return: 1
Scope level=0  refcnt=1 has_symbols
  Symbol Table: stack of 1 tables, 6 symbols
   local table: 7/256 binary-search
       buffers:  [0-10]/19  [0-13]/13  [4-16]/20
  Type Table: stack of 0 tables, 0 types
# drop scope
/alloc 0x\w+ to 0 = 0x0 POINTER_IS_BUFFER_DATA/
/alloc 0x\w+ to 0 = 0x0/
/alloc 0x\w+ to 0 = 0x0/
/alloc 0x\w+ to 0 = 0x0/
/alloc 0x\w+ to 0 = 0x0/
# drop env
/alloc 0x\w+ to 0 = 0x0/
*/

#endif
