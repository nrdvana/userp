// This file is included into scope.c

/*----------------------------------------------------------------------------

## Symbol Table

The symbol table is an array of symbols with a lazy-built by-name lookup.
This allows efficient mapping from number to name and name to number.
The symbol tables are built either from parsing an encoded symbol table of the
protocol, or one symbol at a time as the user supplies them.

### Table Parsed From Buffers

The encoded symbol table is a bunch of NUL-terminated strings packed
end-to-end.  The parse builds a vector of symbol_entry that point to the start
of each string in the buffer, while also verifying the unicode properties of
each string.

In the whole-table case, the memory holding the symbols has been allocated in
userp_buffer objects, and references to that are added to a userp_bstr, and
the scope will hold a strong reference to those buffers for its lifespan.
But, any symbol that crosses the boundary of one buffer to the next needs
copied to a buffer where it can be contiguous.  So, the symbol table parser
may allocate additional buffers to hold those symbols.

The lookup hashtree for symbols is lazy-built on the first use.

### Table Built by User

In the one-at-a-time case, the memory holding the symbols is allocated one
block at a time, and the symbol vector is allocated one element at a time.
The hashtree gets updated before each addition in order to check for
duplicates, so both the symbol_entry vector and the hashtable buckets are
allocated more aggressively to accommodate growth.

The memory holding the symbols packs them end-to-end in the same way that they
need to be encoded for the protocol.  This allows the buffers to be appended
directly to the encoder's output userp_bstr.

### HashTree Lookup Table

The lookup table is implemented using an innovative (well, I'm sure someone
somewhere has done these optimizations before somewhee) data structure I call
a "hashtree".  It is simply an optimized hashtable that refers either to
elements of the vector, or head-nodes of a Red/Black tree. It is built on 3
allocated arrays:

    symbols[]      [ 0:NULL, 1:"A", 2:"B", 3:"C", 4:"D", 5:"E", 6:"F"...]
    
    hash("A") = 3
    hash("B") = 5
    hash("C") = 13 mod 8 = 5
    hash("D") = 7
    hash("E") = 15 mod 8 = 7
    hash("F") = 23 mod 8 = 7
    
    buckets[]      [...3:{symbol=1},...5:{node=1},...7:{node=2}...]
    
    nodes[]
      0:{left=0, right=0, color=black}    // "leaf sentinel" for efficient R/B
      1:{is_pair=true,  sym=2, hash=5, sym2=3, hash2=13}
      2:{is_pair=false, sym=5, hash=15, left=3, right=4, color=black}
      3:{is_pair=false, sym=4, hash=7,  left=0, right=0, color=red}
      4:{is_pair=false, sym=6, hash=23, left=0, right=0, color=red}
      ...

In this example, the vector is holding 6 elements that didn't hash very well.
"A" landed in its own bucket.  A lookup for "A" will immediately land in
bucket[3] and refer to symbols[1], compare equal, and be done.

"B" and "C" both landed in bucket 5.  Bucket 5 refers to node 1.  Node 1 is
marked as holding a pair of elements.  A lookup for "B" will go to nodes[1],
compare with both symbols, and find a match for "B" in symbols[2].

"D", "E", and "F" all landed in bucket 7.  Bucket 7 refers to nodes[2].  Node
2 is a Red/Black tree, and the head node references symbols[5].  A lookup for
"D" will go to nodes[2], compare less than symbols[5], go ->left to nodes[3],
compare equal to symbols[4], and be done.

The HashTree provides the O(1) average lookup time of a hashtable with the
worst-case N(log N) of the R/B tree.  The thing with "pair" nodes is just an
optimization to avoid having to allocate so many nodes when most collisions
happen between only 2 nodes.

The hash buckets and tree nodes both use small integers to refer to the array
of symbols or nodes.  So, for the first 0x7F symbols, a hash bucket is 1 byte
and a tree node is 4 bytes.  It then doubles at 0x80 symbols, and again at
0x8000.  (I didn't bother adding 63-bit handling becuase these are *symbols*,
as in the names of things, not raw data)

The buckets are re-allocated every time the hashtable "appears too small"
or any time the references need to upgrade to more bits.  When the hashtable
rebuilds, the tree is reset and the same memory re-used, only reallocating
if the hash collisions exceed its size.

*/

static inline uint32_t userp_symtable_calc_hash(struct userp_symtable *st, const char *name);

// This handles both the case of limiting symbols to the 2**31 limit imposed by the hash table,
// and also guards against overflow of size_t for allocations on 32-bit systems.
#define MAX_SYMTABLE_ENTRIES (MIN(SIZE_MAX/sizeof(struct symbol_entry), (size_t)(1<<31)-1))

#define HASHTREE_BITS 8
#include "hashtree.c"
#undef HASHTREE_BITS

#define HASHTREE_BITS 16
#include "hashtree.c"
#undef HASHTREE_BITS

#define HASHTREE_BITS 32
#include "hashtree.c"
#undef HASHTREE_BITS

#define MAX_HASHTREE_BUCKET_SIZE 4
#define HASHTREE_BUCKET_SIZE(count) \
	( (((count)-1) >> 15)? 4 : (((count)-1) >> 7)? 2 : 1 )

#define HASHTREE_NODE_SIZE(count) \
	( (((count)-1) >> 15)? sizeof(struct hashtree_node31) \
    : (((count)-1) >> 7)?  sizeof(struct hashtree_node15) \
    :                      sizeof(struct hashtree_node7))

static bool scope_symtable_alloc(userp_scope scope, size_t n) {
	userp_env env= scope->env;
	struct userp_symtable *st= &scope->symtable;
	size_t i, old_n;

	assert(!scope->is_final);
	assert(n > 0);
	// On the first allocation, allocate the exact number requested.
	// After that, allocate in powers of two.
	if (st->alloc)
		n= roundup_pow2(n);
	else {
		if (n == 1) n= 64; // n=1 indicates defining symbols in a loop
		if (!scope->has_symbols) {
			// On the first allocation, also need to add this symtable to the set used by the scope.
			// When allocated, the userp_scope left room in symtable_stack for one extra
			i= scope->symtable_count++;
			scope->symtable_stack[i]= st;
			st->id_offset= !i? 0
				: scope->symtable_stack[i-1]->id_offset
				+ scope->symtable_stack[i-1]->used
				- 1; // because slot 0 of each symbol table is used as a NUL element.
			st->chardata.env= scope->env;
			st->hash_salt= scope->env->salt;
			st->used= 1; // elem 0 is always reserved
			scope->has_symbols= true;
			// All other fields were blanked during the userp_scope constructor
		}
	}
	// Symbol tables never lose symbols until destroyed, so don't bother with shrinking
	if (n <= st->alloc)
		return true; // nothing to do
	// Library implementation is capped at 31-bit references, but on a 32-bit host it's less than that
	// to prevent size_t overflow.
	if (n >= MAX_SYMTABLE_ENTRIES) {
		userp_diag_setf(&env->err, USERP_EDOINGITWRONG,
			"Can't resize symbol table larger than " USERP_DIAG_SIZE " entries",
			(size_t) MAX_SYMTABLE_ENTRIES
		);
		USERP_DISPATCH_ERR(env);
		return false;
	}

	if (!USERP_ALLOC_ARRAY(env, &st->symbols, n))
		return false;

	// If the bit size of the hashtree is changing, reset it.
	old_n= st->alloc;
	if (HASHTREE_BUCKET_SIZE(n) != HASHTREE_BUCKET_SIZE(old_n)) {
		st->processed= 0;
		st->bucket_alloc= st->bucket_alloc
			* HASHTREE_BUCKET_SIZE(old_n) / HASHTREE_BUCKET_SIZE(n);
		st->node_alloc= st->node_alloc
			* HASHTREE_NODE_SIZE(old_n) / HASHTREE_NODE_SIZE(n);
	}
	st->alloc= n;
	return true;
}

static inline uint32_t userp_symtable_calc_hash(struct userp_symtable *st, const char *name) {
	// This is mostly MurmurHash32 by Austin Appleby, but I fudged the
	// implementation a bit since I don't have the key length in advance.
	uint32_t hash= 0, accum= 0;
	const char *pos= name;
	if (*pos) while (1) {
		accum= (accum << 7) ^ *pos++;
		if (*pos) accum= (accum << 7) ^ *pos++;
		if (*pos) accum= (accum << 7) ^ *pos++;
		if (*pos) accum= (accum << 7) ^ *pos++;
		accum *= 0xcc9e2d51;
		accum = (accum << 15) | (accum >> 17);
		accum *= 0x1b873593;
		hash ^= accum;
		if (!*pos) break;
		hash= (hash << 13) | (hash >> 19);
		hash= hash * 5 + 0xe6546b64;
	}
	hash ^= pos - name;
	hash ^= hash >> 16;
	hash *= 0x85ebca6b;
	hash ^= hash >> 13;
	hash *= 0xc2b2ae35;
	hash ^= hash >> 16;
	return hash? hash : 1;
}

userp_symbol userp_symtable_hashtree_get(struct userp_symtable *st, uint32_t hash, const char *name) {
	return ((st->alloc-1) >> 15)? userp_symtable_hashtree_get31(st, hash, name)
	     : ((st->alloc-1) >>  7)? userp_symtable_hashtree_get15(st, hash, name)
	     :                        userp_symtable_hashtree_get7 (st, hash, name);
}

bool userp_symtable_hashtree_insert(struct userp_symtable *st, size_t sym_ofs) {
	return ((st->alloc-1) >> 15)? userp_symtable_hashtree_insert31(st, sym_ofs)
	     : ((st->alloc-1) >>  7)? userp_symtable_hashtree_insert15(st, sym_ofs)
	     :                        userp_symtable_hashtree_insert7 (st, sym_ofs);
}

// This handles both the case of limiting tables to 2x the max number of symbols,
// and also guards against overflow of size_t for allocations on 32-bit systems.
#define MAX_HASH_BUCKETS (MIN((SIZE_MAX/MAX_HASHTREE_BUCKET_SIZE), MAX_SYMTABLE_ENTRIES*2))

bool userp_scope_symtable_hashtree_populate(struct userp_symtable *st, userp_env env) {
	size_t orig_bucket_alloc= st->bucket_alloc;
	// Need at least 1.5x as many buckets as symbols
	if (st->bucket_alloc < st->alloc + (st->alloc>>1)) {
		// The allocated size of the symbol vector is a good hint about the ideal size for
		// the hashtable, since the user might have requested something specific.
		// But don't go smaller than 256;
		assert(st->alloc <= MAX_SYMTABLE_ENTRIES);
		// For first allocation, only reserve 2x.  For later allocations, jump by 4x.
		// Guaranteed to be within size_t because of MAX_SYMTABLE_ENTRIES
		size_t new_buckets= st->alloc << (st->bucket_alloc? 2 : 1);
		// don't allocate tiny hash tables, just adds collisions for insignificant savings
		if (new_buckets < 0x200)
			new_buckets= 0x200;
		else if (new_buckets > MAX_HASH_BUCKETS)
			new_buckets= MAX_HASH_BUCKETS;
		// The current hash algorithm gets a lot better if the bucket count is odd
		if (!(new_buckets & 1)) --new_buckets;
		assert(new_buckets > st->bucket_alloc);
		size_t size= new_buckets * HASHTREE_BUCKET_SIZE(st->alloc);
		if (env->log_trace) {
			userp_diag_setf(&env->msg, USERP_MSG_SYMTABLE_HASHTREE_ALLOC,
				"userp_scope: alloc symtable hashtree size=" USERP_DIAG_SIZE
					" buckets=" USERP_DIAG_COUNT
					" for " USERP_DIAG_POS " symbols",
				size, new_buckets, st->used
			);
			USERP_DISPATCH_MSG(env);
		}
		// don't use "realloc" because there is no reason to copy the old content of the buffer
		userp_alloc(env, &st->buckets, 0, 0);
		if (!userp_alloc(env, &st->buckets, size, 0)) {
			st->bucket_alloc= 0;
			st->processed= 0;
			st->node_used= 0;
			return false;
		}
		st->bucket_alloc= new_buckets;
		st->processed= 0;
	}
	
	// initial setup of hash table and nodes.  Processed counts symbol[0] (NULL), so use that
	// as a flag whether the tables are cleared or not.
	if (!st->processed) {
		if (env->log_trace && orig_bucket_alloc > 0) {
			userp_diag_setf(&env->msg, USERP_MSG_SYMTABLE_HASHTREE_REBUILD,
				"userp_scope: rebuild hashtree "
					"(" USERP_DIAG_COUNT "/" USERP_DIAG_SIZE "+" USERP_DIAG_COUNT2 ")"
					" at " USERP_DIAG_POS " symbols",
				st->bucket_used, st->bucket_alloc, st->node_used, st->used
			);
			USERP_DISPATCH_MSG(env);
		}
		// clear the entire hash table
		bzero(st->buckets, st->bucket_alloc * HASHTREE_BUCKET_SIZE(st->alloc));
		st->bucket_used= 0;
		// node zero is the sentinel node
		if (st->node_alloc) {
			bzero(st->nodes, HASHTREE_NODE_SIZE(st->alloc));
			st->node_used= 1;
		} else {
			assert(st->node_used == 0);
		}
		st->processed= 1;
	}
	size_t batch= st->used - st->processed;
	while (st->processed < st->used) {
		// Now try adding the symbol entry
		if (userp_symtable_hashtree_insert(st, st->processed))
			++st->processed;
		// Did it fail because it was out of tree nodes?
		else if (st->node_used+3 >= st->node_alloc) {
			// Allocate in powers of 2, min 32 nodes
			size_t alloc= roundup_pow2(st->node_used + 17);
			if (env->log_debug) {
				userp_diag_setf(&env->msg, USERP_MSG_SYMTABLE_HASHTREE_EXTEND,
					"userp_scope: symtable hashtree "
					"(" USERP_DIAG_COUNT "/" USERP_DIAG_SIZE "+" USERP_DIAG_COUNT2 ")"
					" collisions require more nodes, realloc " USERP_DIAG_SIZE2 " more",
					st->bucket_used, st->bucket_alloc, st->node_used, alloc - st->node_alloc
				);
				USERP_DISPATCH_MSG(env);
			}
			if (!userp_alloc(env, &st->nodes, alloc * HASHTREE_NODE_SIZE(st->alloc), USERP_HINT_DYNAMIC))
				return false;
			if (!st->node_alloc) {
				// first node is a sentinel set to zeroes
				bzero(st->nodes, HASHTREE_NODE_SIZE(st->alloc));
				st->node_used= 1;
			}
			st->node_alloc= alloc;
		}
		// The only other way it can fail is a corrupt tree
		else {
			userp_diag_set(&env->err, USERP_EBADSTATE, "userp_scope: symbol table hashtree is corrupt");
			USERP_DISPATCH_ERR(env);
			return false;
		}
	}
	if (env->log_trace && batch > 1) {
		// show any time more than one node gets added in a batch
		userp_diag_setf(&env->msg, USERP_MSG_SYMTABLE_HASHTREE_UPDATE,
			"userp_scope: added symbols " USERP_DIAG_POS ".." USERP_DIAG_POS2 " to hashtree "
			"(" USERP_DIAG_COUNT "/" USERP_DIAG_SIZE "+" USERP_DIAG_COUNT2 ")",
			st->processed-batch, st->used,
			st->bucket_used, st->bucket_alloc, st->node_used
		);
		USERP_DISPATCH_MSG(env);
	}
	return true;
}

userp_symbol userp_scope_get_symbol(userp_scope scope, const char *name, int flags) {
	int i, pos, len;
	struct userp_symtable *st;
	userp_symbol ret;
	struct symbol_entry *sym;
	userp_env env= scope->env;
	uint32_t hash= userp_symtable_calc_hash(&scope->symtable, name);

	// search self and parent scopes for symbol (but flags can request local-only)
	if (!(flags & USERP_GET_LOCAL) || scope->has_symbols) {
		for (i= scope->symtable_count - 1; i >= 0; --i) {
			st= scope->symtable_stack[i];
			// Does this symtable have a current hashtable?  If not, build it.
			if (st->processed < st->used)
				if (!userp_scope_symtable_hashtree_populate(st, scope->env))
					return false;
			// Now use the hashtable
			ret= userp_symtable_hashtree_get(st, hash, name);
			if (ret)
				return ret;
			// User can request only searching immediate scope
			if (flags & USERP_GET_LOCAL)
				break;
		}
	}
	// Not found.  Does it need added?
	if (!(flags & USERP_CREATE))
		return 0;
	// if scope is finalized, emit an error
	if (scope->is_final) {
		userp_diag_set(&env->err, USERP_ESCOPEFINAL, "Can't add symbol to a finalized scope");
		USERP_DISPATCH_ERR(env);
		return 0;
	}
	// Grow the symbols array if needed
	if (scope->symtable.used >= scope->symtable.alloc)
		if (!scope_symtable_alloc(scope, scope->symtable.alloc+1))
			return 0;
	pos= scope->symtable.used++;
	// Copy the name into scope storage
	len= strlen(name);
	if (!(name= (char*) userp_bstr_append_bytes(&scope->symtable.chardata, (const uint8_t*) name, len+1, USERP_CONTIGUOUS)))
		return 0;
	// add symbol to the vector
	sym= scope->symtable.symbols + pos;
	sym->name= name; // name was replaced with the local pointer, above
	sym->hash= hash;
	sym->type_ref= 0;
	sym->canonical= 0;
	scope->symtable.used= pos+1;
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
		*dest_lim;        // one-beyond-end of the symbol buffer
	//    *dest_last_sorted;// last element which compared greater than the previous
	//bool sorted;          // whether elements found so far are in correct order
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
		//if (parse->sorted) {
		//	if (parse->prev && strcmp((char*)parse->prev, (char*)parse->start) >= 0) {
		//		parse->dest_last_sorted= parse->dest_pos - 1;
		//		parse->sorted= false;
		//	}
		//	parse->prev= parse->start;
		//}
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
	size_t n, i, segment,
		orig_sym_used, orig_sym_partcnt, syms_added,
		pos_ofs;//, lastsort_ofs;
	uint8_t *p1, *p2;

	// If scope is finalized, emit an error
	if (scope->is_final) {
		userp_diag_set(&env->err, USERP_ESCOPEFINAL, "Can't add symbol to a finalized scope");
		USERP_DISPATCH_ERR(env);
		return false;
	}
	// If no input, return
	if (!part_count || (part_count == 1 && !parts[0].len)) {
		if (!sym_count)
			return true;
		userp_diag_setf(&env->err, USERP_EOVERRUN, "Can't parse " USERP_DIAG_COUNT " symbols from empty buffer",
			(size_t) sym_count);
		USERP_DISPATCH_ERR(env);
		return false;
	}
	// ensure symbol vector has sym_count slots available (if sym_count provided)
	// scope_symtable_alloc needs to be called regardless, if symtable not initialized yet.
	n= (scope->symtable.used? scope->symtable.used : 1) + (sym_count? sym_count : 1);
	if (n > scope->symtable.alloc)
		if (!scope_symtable_alloc(scope, n))
			return false;
	// Initialize chardata (if not already) and ensure space for input.part_count*2 - 1 new parts.
	// There can be at most one part added for each input part and one for each boundary between input parts.
	n= scope->symtable.chardata.part_count + part_count*2 - 1;
	if (n > scope->symtable.chardata.part_alloc)
		if (!userp_bstr_partalloc(&scope->symtable.chardata, n))
			return false;
	// Record the original status of the symbol table, to be able to revert changes
	orig_sym_used= scope->symtable.used;
	orig_sym_partcnt= scope->symtable.chardata.part_count;
	// Set up the parser's view of the situation
	bzero(&parse, sizeof(parse));
	parse.dest_pos= scope->symtable.symbols + scope->symtable.used;
	parse.dest_lim= scope->symtable.symbols
		+ (sym_count? scope->symtable.used+sym_count : scope->symtable.alloc);
	//parse.sorted= !scope->symtable.tree;
	//parse.dest_last_sorted= parse.sorted? parse.dest_pos : NULL;
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
			//lastsort_ofs= parse.dest_last_sorted - scope->symtable.symbols;
			// Perform re-alloc
			if (!scope_symtable_alloc(scope, scope->symtable.alloc+1 /* gets rounded up */)) {
				success= false;
				break;
			}
			// Repair parse pointers
			parse.dest_pos= scope->symtable.symbols + pos_ofs;
			parse.dest_lim= scope->symtable.symbols + scope->symtable.alloc;
			//if (parse.dest_last_sorted)
			//	parse.dest_last_sorted= scope->symtable.symbols + lastsort_ofs;
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
		USERP_DISPATCH_ERR(env);
		goto failure;
	}		
	// The stream protocol allows buffers to have extra characters at the end
	// of the symbol table.  Maybe there should be a flag to detect that when
	// it isn't wanted?
	return true;

	CATCH(failure) {
		CATCH(parse_failure) {
			memcpy(&env->err, &parse.diag, sizeof(parse.diag));
			USERP_DISPATCH_ERR(env);
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

#ifdef UNIT_TEST

/*
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
*/

UNIT_TEST(scope_symbol_sorted) {
	char buf[32];
	int i;
	userp_env env= userp_new_env(NULL, userp_file_logger, stdout, 0);
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
Scope level=0  refcnt=1 has_symbols
 *Symbol Table: stack of 1 tables, 1000 symbols
 *local table: 1000/1024 partially indexed \(\d+ vector bytes\)
 *hashtree: \d+/\d+\+\d+ \(\d+ table bytes, \d+ node bytes\)
 *buffers:  \[0-4095\]/4096  \[0-4905\]/8192
 *Type Table: stack of 0 tables, 0 types
*/

UNIT_TEST(scope_symbol_treesort) {
	char buf[32];
	int i;
	userp_symbol sym, sym2;

	userp_env env= userp_new_env(NULL, userp_file_logger, stdout, 0);
	env->log_warn= env->log_debug= env->log_trace= 1;
	userp_scope scope= userp_new_scope(env, NULL);
	// Insert two nodes out of order
	printf("# two out-of-order nodes\n");
	buf[0]= 'b'; buf[1]= '\0';
	if (!userp_scope_get_symbol(scope, buf, USERP_CREATE))
		printf("Failed to add %s\n", buf);
	dump_scope(scope);
	buf[0]= 'a';
	if (!userp_scope_get_symbol(scope, buf, USERP_CREATE))
		printf("Failed to add %s\n", buf);
	dump_scope(scope);
	// Now start over
	printf("# reset\n");
	userp_drop_scope(scope);
	scope= userp_new_scope(env, NULL);
	printf("# 12K nodes increasing\n");
	// Insert some nodes in order
	for (i= 20000; i < (1<<15)-1; i++) {
		snprintf(buf, sizeof(buf), "%05d", i);
		sym= userp_scope_get_symbol(scope, buf, USERP_CREATE);
		if (!sym) {
			printf("Failed to add %s\n", buf);
			abort();
		}
		if ((sym2= userp_scope_get_symbol(scope, buf, USERP_CREATE)) != sym) {
			printf("didn't get back same symbol: get('%s') = %d, should be %d\n", buf, sym2, sym);
			abort();
		}
	}
	
	// Let it allocate right up to the limit of the 15-bit tree implementation.
	scope_symtable_alloc(scope, 1<<15);

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
		if (!(sym= userp_scope_get_symbol(scope, buf, USERP_CREATE))) {
			printf("Failed to add %s\n", buf);
			abort();
		}
		if ((sym2= userp_scope_get_symbol(scope, buf, USERP_CREATE)) != sym) {
			printf("didn't get back same symbol: get('%s') = %d, should be %d\n", buf, sym2, sym);
			abort();
		}
	}
	dump_scope(scope);
	assert(scope->symtable.used == 32768);
	
	printf("# insert one more node forcing it to upgrade to 31-bit tree\n");
	snprintf(buf, sizeof(buf), "%05d", 1<<15);
	sym= userp_scope_get_symbol(scope, buf, USERP_CREATE);
	if (!sym) {
		printf("Failed to add %s\n", buf);
		abort();
	}
	if ((sym2= userp_scope_get_symbol(scope, buf, USERP_CREATE)) != sym) {
		printf("didn't get back same symbol: get('%s') = %d, should be %d\n", buf, sym2, sym);
		abort();
	}
	dump_scope(scope);

	userp_drop_scope(scope);
	userp_drop_env(env);
}
/*OUTPUT
debug: userp_scope: create 1 .*
# two out-of-order nodes
Scope level=0  refcnt=1 has_symbols
  Symbol Table: stack of 1 tables, 1 symbols
   local table: 1/\d+ not indexed \(\d+ vector bytes\)
       buffers:  \[0-2\]/4096
    Type Table: stack of 0 tables, 0 types
debug: userp_scope: alloc symtable hashtree size=\d+ buckets=\d+ for 2 symbols
Scope level=0  refcnt=1 has_symbols
  Symbol Table: stack of 1 tables, 2 symbols
 *local table: 2/\d+ partially indexed \(\d+ vector bytes\)
 *hashtree: 1/\d+\+0 \(\d+ table bytes, \d+ node bytes\)
 *buffers:  \[0-4\]/4096
 *Type Table: stack of 0 tables, 0 types
# reset
debug: userp_scope: destroy 1 .*
debug: userp_scope: create 2 .*
# 12K nodes increasing
(debug:.*\n)*
# 10K nodes decreasing
(debug:.*\n)*
# 10K nodes into the middle of the tree
(debug:.*\n)*
Scope level=0  refcnt=1 has_symbols
  Symbol Table: stack of 1 tables, 32767 symbols
 *local table: 32767/\d+ indexed \(\d+ vector bytes\)
 *hashtree:.*
 *buffers:.*
 *Type Table: stack of 0 tables, 0 types
# insert one more node forcing it to upgrade to 31-bit tree
debug: userp_scope: alloc symtable hashtree .*
debug: userp_scope: rebuild hashtree .*? at 32769 symbols
debug: userp_scope: added symbols 1..32769 to hashtree .*
Scope level=0  refcnt=1 has_symbols
  Symbol Table: stack of 1 tables, 32768 symbols
 *local table: 32768/\d+ indexed \(\d+ vector bytes\)
 *hashtree:.*
 *buffers:.*
 *Type Table: stack of 0 tables, 0 types
debug: userp_scope: destroy 2 .*
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
	env->log_warn= env->log_debug= env->log_trace= 1;
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
(alloc 0x0+ to .*\n){2}
debug: userp_scope: create 1 .*
(alloc 0x0+ to .*\n){3}
return: 1
Scope level=0  refcnt=1 has_symbols
 *Symbol Table: stack of 1 tables, 5 symbols
 *local table: 5/.*
 *buffers:  \[0-20\]/20
 *Type Table: stack of 0 tables, 0 types
# drop buffer
# drop scope
debug: userp_scope: destroy 1 .*
(alloc 0x\w+ to 0 = 0x0+\n){4}
# drop env
alloc 0x\w+ to 0 = 0x0+
*/


/* This test parses a buffer of two segments each having a perfectly aligned
 * list of symbols.  It should translate to two succesful calls to to
 * parse_symbols(), each returning true.
 */
UNIT_TEST(scope_parse_split_symtable) {
	userp_env env= userp_new_env(NULL, userp_file_logger, stdout, 0);
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
return: 1
Scope level=0  refcnt=1 has_symbols
 *Symbol Table: stack of 1 tables, 10 symbols
 *local table: 10/.*
 *buffers:  \[0-20\]/20  \[0-20\]/20
 *Type Table: stack of 0 tables, 0 types
# drop scope
# drop env
*/

static const char symbol_fragment[]= "fragment1\0fragment2";
static struct userp_buffer symbol_fragment_buf= {
	.data= (uint8_t*) symbol_fragment,
	.alloc_len= sizeof(symbol_fragment)-1
};

UNIT_TEST(scope_parse_split_symbol) {
	userp_env env= userp_new_env(NULL, userp_file_logger, stdout, 0);
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
return: 1
Scope level=0  refcnt=1 has_symbols
 *Symbol Table: stack of 1 tables, 6 symbols
 *local table: 6/.*
 *buffers:  \[0-10\]/19  \[0-13\]/13  \[4-16\]/20
 *Type Table: stack of 0 tables, 0 types
# drop scope
# drop env
*/

#endif
