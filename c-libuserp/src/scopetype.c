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

#define READ_VQTY(dest) \
  do { \
	size_t tmp; \
    if (!userp_decode_vqty_quick(&tmp, in)) \
        goto fail_vqty; \
	(dest)= tmp; \
  } while(0)

#define READ_SYMREF(dest) \
  do { \
	size_t tmp; \
    if (!userp_decode_vqty_quick(&tmp, in)) \
        goto fail_vqty; \
    if (!(tmp & 1) && (tmp >> 1) <= scope->symbol_count) \
        (dest)= tmp >> 1; \
	else if (!((dest)= userp_scope_resolve_relative_symref(scope, tmp))) \
		goto fail_symref; \
  } while (0)

#define READ_TYPEREF(dest) \
  do { \
	size_t tmp; \
    if (!userp_decode_vqty_quick(&tmp, in)) \
        goto fail_vqty; \
    if (!(tmp & 1) && (tmp >> 1) <= scope->type_count) \
        (dest)= tmp >> 1; \
	else if (!((dest)= userp_scope_resolve_relative_typeref(scope, tmp))) \
		goto fail_typeref; \
  } while (0)

#define TYPEDEF_RECORD_ALWAYS_FIELDS       1
#define TYPEDEF_RECORD_OFTEN_FIELDS        3
#define TYPEDEF_RECORD_FIELD_PARENT_BIT      1
#define TYPEDEF_RECORD_FIELD_STATICBITS_BIT  2
#define TYPEDEF_RECORD_FIELD_FIELDS_BIT      3
#define TYPEDEF_RECORD_SELDOM_FIELDS       3
#define TYPEDEF_RECORD_FIELD_ALIGN_BIT       5
#define TYPEDEF_RECORD_FIELD_PAD_BIT         6
#define TYPEDEF_RECORD_FIELD_OTHERTYPE_BIT   7
static bool parse_record_typedef(userp_scope scope, size_t table_idx, struct userp_bit_io *in)
{
	unsigned seldom_fields[TYPEDEF_RECORD_SELDOM_FIELDS];
	struct userp_type_record tmp_rec, *dest_rec= NULL;
	size_t n, i, size, field, n_seldom,
		name, parent,
		max_fields= scope->env->record_fields_max;
	bzero(&tmp_rec, sizeof(tmp_rec));

	assert(in->accum_bits == 1 + TYPEDEF_RECORD_SELDOM_FIELDS);
	unsigned code= in->accum;
	// Bit 0 indicates presence of 'seldom' fields
	if (code & 1) {
		// How many seldom fields?
		READ_VQTY(n_seldom);
		if (n_seldom > TYPEDEF_RECORD_SELDOM_FIELDS) goto fail_seldom_count;
		// add them to a queue to be decoded
		for (i= 0; i < n_seldom; i++) {
			READ_VQTY(field);
			// check for out of bounds
			if (field > TYPEDEF_RECORD_SELDOM_FIELDS) goto fail_seldom_ref;
			field += TYPEDEF_RECORD_ALWAYS_FIELDS + TYPEDEF_RECORD_OFTEN_FIELDS;
			// check for duplicate
			if (code & (1<<field)) goto fail_seldom_ref_dup;
			code |= (1<<field);
			// queue it
			seldom_fields[i]= field;
		}
	}
	// "Always" fields:
	READ_SYMREF(name);

	// "Often" fields
	if (code & (1<<TYPEDEF_RECORD_FIELD_PARENT_BIT)) {
		READ_TYPEREF(parent);
		// locate parent, copy fields
		unimplemented("copy fields from parent");
	}

	if (code & (1<<TYPEDEF_RECORD_FIELD_STATICBITS_BIT)) {
		READ_VQTY(tmp_rec.static_bits);
	}

	if (code & (1<<TYPEDEF_RECORD_FIELD_FIELDS_BIT)) {
		READ_VQTY(n);
		// sanity check number of fields
		if (n > max_fields) goto fail_field_count;
		// Have enough info now to allocate the struct
		size= sizeof(struct userp_type_record) + (n * sizeof(struct record_field));
		dest_rec= (struct userp_type_record*) userp_bstr_append_bytes(
			&scope->typetable.typeobjects, NULL, size,
			USERP_CONTIGUOUS|USERP_ALLOC_ALIGN_SIZET
		);
		if (!dest_rec) goto fail_alloc;
		// Now parse each field
		for (i= 0; i < n; i++) {
			READ_SYMREF(dest_rec->fields[i].name);
			READ_TYPEREF(dest_rec->fields[i].type);
			READ_VQTY(dest_rec->fields[i].placement);
		}
	}
	for (i= 0; i < n_seldom; i++) {
		switch (seldom_fields[i]) {
		case TYPEDEF_RECORD_FIELD_ALIGN_BIT:
			READ_VQTY(tmp_rec.align);
			break;
		case TYPEDEF_RECORD_FIELD_PAD_BIT:
			READ_VQTY(tmp_rec.pad);
			break;
		case TYPEDEF_RECORD_FIELD_OTHERTYPE_BIT:
			READ_TYPEREF(tmp_rec.other_field_type);
			break;
		default:
			unimplemented("BUG: unhandled seldom field in record typedef");
		}
	}
	if (!dest_rec) {
		unimplemented("copy temp fields into final field struct");
	}
	memcpy(dest_rec, &tmp_rec, sizeof(tmp_rec));
	// dest_rec is completely loaded, but not completely validated at this point.
	scope->typetable.types[table_idx].name= name;
	scope->typetable.types[table_idx].parent= parent;
	scope->typetable.types[table_idx].typeclass= USERP_TYPECLASS_RECORD;
	scope->typetable.types[table_idx].typeobj= dest_rec;
	return true;

	CATCH(fail_vqty) {
		(void)0; // error message is already set, but not dispatched
	}
	CATCH(fail_symref) {
		userp_diag_set(&scope->env->err, USERP_ESYMBOL, "Invalid symbol reference");
	}
	CATCH(fail_typeref) {
		userp_diag_set(&scope->env->err, USERP_ETYPE, "Invalid type reference");
	}
	CATCH(fail_seldom_count) {
		userp_diag_set(&scope->env->err, USERP_ERECORD, "Invalid number of additional fields for record");
	}
	CATCH(fail_seldom_ref) {
		userp_diag_set(&scope->env->err, USERP_ERECORD, "Invalid field reference in record");
	}
	CATCH(fail_seldom_ref_dup) {
		userp_diag_set(&scope->env->err, USERP_ERECORD, "Invalid duplicate field reference in record");
	}
	CATCH(fail_field_count) {
		userp_diag_setf(&scope->env->err, USERP_ELIMIT,
			"Record definition of " USERP_DIAG_COUNT " fields exceeds maximum of " USERP_DIAG_COUNT2,
			(size_t) n, (size_t) max_fields);
	}
	USERP_DISPATCH_ERR(scope->env);
	CATCH(fail_alloc) {
		(void)0; // error message already dispatched
	}
	return NULL;
}	

//#define TYPEDEF_ANY_SELBASE     0
//#define TYPEDEF_ANY_SELBITS     1
//#define TYPEDEF_SYMREF_SELBASE  (TYPEDEF_ANY_SELBASE+(1<<TYPEDEF_ANY_SELBITS))
//#define TYPEDEF_SYMREF_SELBITS  1
//#define TYPEDEF_TYPEREF_SELBASE (TYPEDEF_SYMREF_SELBASE+(1<<TYPEDEF_SYMREF_SELBITS))
//#define TYPEDEF_TYPEREF_SELBITS 1
//#define TYPEDEF_INTEGER_SELBASE (TYPEDEF_TYPEREF_SELBASE+(1<<TYPEDEF_TYPEREF_SELBITS))
//#define TYPEDEF_INTEGER_SELBITS 6
//#define TYPEDEF_CHOICE_SELBASE  (TYPEDEF_INTEGER_SELBASE+(1<<TYPEDEF_INTEGER_SELBITS))
//#define TYPEDEF_CHOICE_SELBITS  3
//#define TYPEDEF_ARRAY_SELBASE   (TYPEDEF_CHOICE_SELBASE+(1<<TYPEDEF_CHOICE_SELBITS))
//#define TYPEDEF_ARRAY_SELBITS   6
//#define TYPEDEF_RECORD_SELBASE  (TYPEDEF_ARRAY_OFS+(1<<TYPEDEF_ARRAY_SELBITS))
//#define TYPEDEF_RECORD_SELBITS  (1+TYPEDEF_RECORD_OFTEN_FIELDS)
//bool parse_typedef(userp_scope scope, struct userp_bit_io *in) {
//	if (in->pos >
//	unsigned code= *pos++;
//	if (code >= TYPEDEF_RECORD_OFS) {
//		code -= TYPEDEF_RECORD_OFS;
//		return parse_record_typedef(scope, in);
//	} else if (code >= TYPEDEF_ARRAY_OFS) {
//		unimplemented("parse array");
//	} else if (code >= TYPEDEF_CHOICE_OFS) {
//		unimplemented("parse choice");
//	} else if (code >= TYPEDEF_INTEGER_OFS) {
//		code -= TYPEDEF_INTEGER_OFS;
//		// Fields: name, bit0? n_seldom, bit1? parent, bit2? align, bit3? bits, bit4? bswap, bit5? min
//		unimplemented("parse integer");
//	} else if (code >= TYPEDEF_TYPEREF_OFS) {
//		unimplemented("parse typeref");
//	} else if (code >= TYPEDEF_SYMREF_OFS) {
//		unimplemented("parse symref");
//	} else /* if (*pos >= TYPEDEF_ANY_OFS) */ {
//		unimplemented("parse typeany");
//	}
//}
