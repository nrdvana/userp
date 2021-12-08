// This file contains templates for N-bit hashtable + red/black tree code

#ifndef HASHTREE_BITS
#error HASHTREE_BITS is required
#endif

#if HASHTREE_BITS == 8
#define IDBITS 7
#define WORD_TYPE uint8_t
#elif HASHTREE_BITS == 16
#define IDBITS 15
#define WORD_TYPE uint16_t
#elif HASHTREE_BITS == 32
#define IDBITS 31
#define WORD_TYPE uint32_t
#elif HASHTREE_BITS == 63
#define IDBITS 63
#define WORD_TYPE uint64_t
#else
#error Un-handled HASHTREE_BITS value
#endif

#define BIT_SUFFIX_NAME_(name, bits) name ## bits
#define BIT_SUFFIX_NAME(name, bits) BIT_SUFFIX_NAME_(name, bits)
#define NODE_TYPE BIT_SUFFIX_NAME(struct hashtree_node, IDBITS)
#define TREE_HEIGHT_LIMIT (HASHTREE_BITS*2)

NODE_TYPE {
	WORD_TYPE
		sym: IDBITS,
		is_pair: 1,
		hash,
		left,
		right: IDBITS,
		color: 1;
};

#ifndef HASHTREE_KEY_CMP

#define HASHTREE_KEY_CMP(symtable, keyhash, key, node) ( \
	(keyhash) < (node).hash? -1 \
	: (keyhash) > (node).hash? 1 \
	: strcmp( (key), (symtable)->symbols[(node).sym].name ) \
)

#define HASHTREE_NODE_CMP(symtable, node1, node2) ( \
	(node1).hash < (node2).hash? -1 \
	: (node1).hash > (node2).hash? 1 \
	: strcmp( (symtable)->symbols[(node1).sym].name, (symtable)->symbols[(node2).sym].name ) \
)

#define HASHTREE_PAIRNODE_GET_MATCH(symtable, keyhash, key, node) ( \
	0 == HASHTREE_KEY_CMP(symtable, keyhash, key, node) \
		? (symtable)->id_offset + (node).sym \
		: ((node).left == (keyhash) && 0 == strcmp((key), (symtable)->symbols[(node).right].name )) \
		? (symtable)->id_offset + (node).right \
		: 0 )

#endif

static userp_symbol BIT_SUFFIX_NAME(userp_symtable_hashtree_get, IDBITS) (
	struct userp_symtable *st,
	uint32_t hash,
	const char *name
) {
	// read the bucket for the hash
	WORD_TYPE *bucket= &((WORD_TYPE*) st->buckets)[ hash % st->bucket_alloc ];
	// If low bit is 1, there was a collision, and this is the root of a tree
	if (*bucket & 1) {
		NODE_TYPE *nodes= (NODE_TYPE*) st->nodes;
		WORD_TYPE node_idx= *bucket >> 1;
		// Oh but wait, I added one more optimization where it could be a non-tree
		// holding a pair of nodes.
		if (nodes[node_idx].is_pair)
			return HASHTREE_PAIRNODE_GET_MATCH(st, (WORD_TYPE)hash, name, nodes[node_idx]);
		// now back to your regularly scheduled R/B programming
		int cmp;
		while (node_idx) {
			cmp= HASHTREE_KEY_CMP(st, (WORD_TYPE)hash, name, nodes[node_idx]);
			if (cmp == 0) return st->id_offset + nodes[node_idx].sym;
			else if (cmp > 0) node_idx= nodes[node_idx].right;
			else node_idx= nodes[node_idx].left;
		}
	}
	// else this bucket refers to exactly one symbol, but still need to verify the string
	else {
		WORD_TYPE sym_idx= *bucket >> 1;
		if (sym_idx && strcmp(name, st->symbols[sym_idx].name) == 0)
			return st->id_offset + sym_idx;
	}
	return 0;
}

static bool BIT_SUFFIX_NAME(userp_symtable_rb_insert, IDBITS) (
	struct userp_symtable *st,
	WORD_TYPE *root,
	WORD_TYPE node
) {
	// element 0 is the "sentinel" leaf, and 'node' is the element being added
	NODE_TYPE *nodes= (NODE_TYPE*) st->nodes;
	int cmp;
	WORD_TYPE stack[TREE_HEIGHT_LIMIT], pos, parent, new_head, i;

	assert(nodes != NULL);
	assert(nodes[0].left == 0);
	assert(nodes[0].right == 0);
	assert(nodes[0].color == 0);

	assert(root != NULL && *root != 0);
	assert(nodes[*root].color == 0);

	nodes[node].left= 0;
	nodes[node].right= 0;
	nodes[node].color= 1;
	
	// leaf-sentinel will double as the head-sentinel
	stack[i= 0]= 0;
	pos= *root;
	while (pos && i < TREE_HEIGHT_LIMIT) {
		stack[++i]= pos;
		cmp= HASHTREE_NODE_CMP(st, nodes[node], nodes[pos]);
		pos= cmp < 0? nodes[pos].left : nodes[pos].right;
	}
	if (i >= TREE_HEIGHT_LIMIT) {
		assert(i < TREE_HEIGHT_LIMIT);
		return false;
	}
	pos= stack[i--];
	if (cmp < 0)
		nodes[pos].left= node;
	else
		nodes[pos].right= node;
	// balance the tree.  Pos points at the parent node of the new node.
	// if current is a black node, no rotations needed
	while (i > 0 && nodes[pos].color) {
		// current is red, the imbalanced child is red, and parent is black.
		parent= stack[i];
		assert(nodes[parent].color == 0);
		// if the current is on the right of the parent, the parent is to the left
		if (nodes[parent].right == pos) {
			// if the sibling is also red, we can pull down the color black from the parent
			if (nodes[nodes[parent].left].color) {
				nodes[nodes[parent].left].color= 0;
				nodes[pos].color= 0;
				nodes[parent].color= 1;
				// jump twice up the tree
				pos= stack[--i];
				i--;
				break;
			}
			// if the imbalance (red node) is on the left, and the parent is on the left,
			//  rotate the inner tree to the right
			if (nodes[nodes[pos].left].color) {
				// rotate right
				new_head= nodes[pos].left;
				if (nodes[parent].right == pos) nodes[parent].right= new_head;
				else nodes[parent].left= new_head;
				nodes[pos].left= nodes[new_head].right;
				nodes[new_head].right= pos;
				pos= new_head;
			}

			// Now we can do the left rotation to balance the tree.
			new_head= nodes[parent].right;
			pos= parent;
			parent= stack[--i];
			if (!parent) *root= new_head;
			else if (nodes[parent].right == pos) nodes[parent].right= new_head;
			else nodes[parent].left= new_head;
			nodes[pos].right= nodes[new_head].left;
			nodes[new_head].left= pos;
			nodes[pos].color= 1;
			nodes[new_head].color= 0;
			break; // balanced
		}
		// else the parent is to the right
		else {
			// if the sibling is also red, we can pull down the color black from the parent
			if (nodes[nodes[parent].right].color) {
				nodes[nodes[parent].right].color= 0;
				nodes[pos].color= 0;
				nodes[parent].color= 1;
				// jump twice up the tree
				pos= stack[--i];
				i--;
				break;
			}
			// if the imbalance (red node) is on the right, and the parent is on the right,
			//  rotate the inner tree to the left
			if (nodes[nodes[pos].right].color) {
				// rotate left
				new_head= nodes[pos].right;
				if (nodes[parent].right == pos) nodes[parent].right= new_head;
				else nodes[parent].left= new_head;
				nodes[pos].right= nodes[new_head].left;
				nodes[new_head].left= pos;
				pos= new_head;
			}

			// Now we can do the right rotation to balance the tree.
			new_head= nodes[parent].left;
			pos= parent;
			parent= stack[--i];
			if (!parent) *root= new_head;
			else if (nodes[parent].right == pos) nodes[parent].right= new_head;
			else nodes[parent].left= new_head;
			nodes[pos].left= nodes[new_head].right;
			nodes[new_head].right= pos;
			nodes[pos].color= 1;
			nodes[new_head].color= 0;
			break; // balanced
		}
	}
	// ensure the root node is black
	nodes[*root].color= 0;
	return true;
}

static bool BIT_SUFFIX_NAME(userp_symtable_hashtree_insert, IDBITS) (
	struct userp_symtable *st,
	int sym_ofs
) {
	uint32_t hash= st->symbols[sym_ofs].hash;
	
	// calculate the official hash on the symbol entry if it wasn't done yet
	if (!hash) {
		hash= userp_symtable_calc_hash(st, st->symbols[sym_ofs].name);
		st->symbols[sym_ofs].hash= hash;
	}

	// check the bucket for the hash.
	WORD_TYPE *bucket= &((WORD_TYPE*) st->buckets)[hash % st->bucket_alloc ];
	if (!*bucket) {
		// If zero, we're lucky, and can just write to it
		*bucket= sym_ofs << 1;
		st->bucket_used++;
		return true;
	}
	if (!(*bucket & 1)) {
		// The number is a symbol ID, not a node ID.
		WORD_TYPE sym2= *bucket >> 1;
		// We need to allocate one node, and create a pair.
		// Check if there is room for 1-3 more tree elements.
		if (st->node_used >= st->node_alloc) {
			return false;  // parent needs to reallocate
		}
		NODE_TYPE *nodes= (NODE_TYPE*) st->nodes;
		WORD_TYPE node= st->node_used++;
		nodes[node].sym= sym_ofs;
		nodes[node].hash= hash;
		nodes[node].is_pair= 1;
		nodes[node].right= sym2;
		nodes[node].left= st->symbols[sym2].hash;
		nodes[node].color= 0;
		*bucket= (node<<1) | 1;
	}
	else {
		// The number in the bucket is a node ID.
		WORD_TYPE node= *bucket >> 1;
		NODE_TYPE *nodes= (NODE_TYPE*) st->nodes;
		bool is_pair= nodes[node].is_pair;
		// The number in the bucket is a node ID, but is 'is_pair', then it isn't a tree and is
		// just two symbol refs stuffed into the same node.  This means we have to add 2 nodes.
		// Check if there are enough spare nodes
		if (st->node_used + (is_pair? 2 : 1) > st->node_alloc)
			return false;  // parent needs to reallocate

		if (is_pair) {
			// need to allocate a node for the second symbol that was wedged into this pair.
			// then add it as a proper child node to the existing node.
			WORD_TYPE node2= st->node_used++;
			nodes[node2].sym= nodes[node].right;
			nodes[node2].hash= nodes[node].left;
			nodes[node2].color= 1;
			nodes[node2].is_pair= 0;
			nodes[node2].left= 0;
			nodes[node2].right= 0;
			if (HASHTREE_NODE_CMP(st, nodes[node2], nodes[node]) < 0) {
				nodes[node].left= node2;
				nodes[node].right= 0;
			} else {
				nodes[node].right= node2;
				nodes[node].left= 0;
			}
			nodes[node].color= 0;
			nodes[node].is_pair= 0;
		}
		// Now the node is definitely a real tree.  Allocate yet another node for the
		// current symbol, then insert it with the normal R/B algorithm.
		WORD_TYPE node2= st->node_used++;
		nodes[node2].sym= sym_ofs;
		nodes[node2].hash= hash;
		nodes[node2].left= 0;
		nodes[node2].right= 0;
		nodes[node2].is_pair= 0;
		nodes[node2].color= 0;
		WORD_TYPE prev_head= node;
		bool success= BIT_SUFFIX_NAME(userp_symtable_rb_insert, IDBITS)(st, &node, node2);
		if (!success) { // can fail if tree is corrupt
			st->node_used--;
			return false;
		}
		if (node != prev_head)
			*bucket= (node << 1) | 1;
	}
	return true;
}

static bool BIT_SUFFIX_NAME(userp_symtable_hashtree_walk, IDBITS) (
	struct userp_symtable *st,
	WORD_TYPE root,
	const char *key,
	bool (*walk_cb)(void *context, int sym_ofs),
	void *context
) {
	NODE_TYPE *nodes= (NODE_TYPE*) st->nodes;
	WORD_TYPE node, stack[TREE_HEIGHT_LIMIT];
	int parent_pos, cmp;

	assert(nodes);
	assert(nodes[0].left == 0);
	assert(nodes[0].right == 0);
	assert(nodes[0].color == 0);

	stack[0]= 0;
	parent_pos= 0;
	node= root;
	// seek initial node, if given
	if (key) {
		while (node && parent_pos < TREE_HEIGHT_LIMIT) {
			cmp= strcmp(key, st->symbols[node].name);
			if (cmp == 0) break;
			stack[++parent_pos]= node;
			node= (cmp > 0)? nodes[node].right : nodes[node].left;
		}
		if (node) // ran out of stack, tree is corrupt
			return false;
		node= stack[parent_pos--];
		goto emit_node;
	}
	// Now begin iterating
	{
	seek_left:
		while (nodes[node].left) {
			if (parent_pos+1 >= TREE_HEIGHT_LIMIT) return false; // tree corrupt
			stack[++parent_pos]= node;
			node= nodes[node].left;
		}
	emit_node:
		if (!walk_cb(context, node))
			return true; // user requested end, which is success
	//step_right:
		// start over from right subtree, if any
		if (nodes[node].right) {
			if (parent_pos+1 >= TREE_HEIGHT_LIMIT) return false; // tree corrupt
			stack[++parent_pos]= node;
			node= nodes[node].right;
			goto seek_left;
		}
		// else walk upward while this node was to the right of the parent
		while (nodes[stack[parent_pos]].right == node)
			node= stack[parent_pos--];
		// then up one more parent
		node= stack[parent_pos--];
		if (node) goto emit_node;
	}
	return true;
}

#undef IDBITS
#undef WORD_TYPE
#undef NODE_TYPE
#undef BIT_SUFFIX_NAME
#undef BIT_SUFFIX_NAME_
#undef TREE_HEIGHT_LIMIT
