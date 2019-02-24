#ifndef RBTREE_H
#define RBTREE_H

/**
 * Red/Black tree node.  Nodes point to other nodes, so pointer math is
 * needed to reach the data paired with the node.
 */
typedef struct RBTreeNode_s {
	struct RBTreeNode_s *left, *right, *parent;
	size_t color: 1;
#if SIZE_MAX == 0xFFFF
	size_t count: 15;
#elif SIZE_MAX == 0xFFFFFFFF
	size_t count: 31;
#else
	size_t count: 63;
#endif
} RBTreeNode_t;

static inline bool rbtree_node_is_sentinel(RBTreeNode_t *node) { return !node->count; }
static inline bool rbtree_node_is_rootsentinel(RBTreeNode_t *node) { return !node->parent; }

typedef int RBTreeCmpFn( const void *Key_A, const void *Key_B );

// Returns index of a node within the tree, as if the tree were a sorted array.
// In other words, returns the number of nodes with a key less than this node.
extern int rbtree_node_index( RBTreeNode_t* node );
// Returns previous node in conceptual sorted list of nodes, or NULL
extern RBTreeNode_t* rbtree_node_prev( RBTreeNode_t* node );
// Returns next node in conceptual sorted list of nodes, or NULL
extern RBTreeNode_t* rbtree_node_next( RBTreeNode_t* node );
// Returns right-most child of this node, or NULL
extern RBTreeNode_t* rbtree_node_right_leaf( RBTreeNode_t* node );
// Returns left-most child of this node, or NULL
extern RBTreeNode_t* rbtree_node_left_leaf( RBTreeNode_t* node );
// Returns Nth child of a node, or NULL
extern RBTreeNode_t* rbtree_node_child_at_index( RBTreeNode_t* node, size_t index );
// Add a node to a tree
extern void rbtree_node_insert( RBTreeNode_t *hint, RBTreeNode_t *node, RBTreeCmpFn *compare, int node_key_ofs);
// Find a child with the given key.  Returns count of nodes that matched.
extern bool rbtree_node_search( RBTreeNode_t *node, const void *key, RBTreeCmpFn *compare, int node_key_ofs,
	RBTreeNode_t **result_first, RBTreeNode_t **result_last, RBTreeNode_t **result_nearest, int *result_count );
// Remove a node form the tree
extern void rbtree_node_prune( RBTreeNode_t *node );

typedef struct RBTree_s {
	RBTreeNode_t root_sentinel; // the left child of the Root Sentinel is the root node
	RBTreeNode_t sentinel;
	RBTreeCmpFn *compare;       // comparison function for keys
	int node_key_ofs;           // adder to get from node address to key address
	int node_obj_ofs;           // adder to get from node address to object address
} RBTree_t;

/* Initialize the tree.  Use macro RBTREE_INIT to automatically resolve offsets to member fields. */
static inline void rbtree_init( RBTree_t *tree, int key_ofs, int obj_ofs, RBTreeCmpFn *compare ) {
	tree->root_sentinel.count= 0;
	tree->root_sentinel.parent= NULL;
	tree->root_sentinel.left= &tree->sentinel;
	tree->root_sentinel.right= &tree->sentinel;
	tree->sentinel.count= 0;
	tree->sentinel.left= &tree->sentinel;
	tree->sentinel.right= &tree->sentinel;
	tree->sentinel.parent= &tree->sentinel;
	tree->compare= compare;
	tree->node_key_ofs= key_ofs;
	tree->node_obj_ofs= obj_ofs;
}

/* RBTREE_INIT( tree, object_type, node_field, key_field, compare_function )
 *
 * Example:
 * struct MyThing { const char *name, RBTreeNode_t *nametree };
 * struct MyCollection { RBTree_t things; }
 * ...
 *  collection= (MyCollection*) malloc(sizeof(MyCollection));
 *  RBTREE_INIT(&collection->things, MyThing, nametree, name, &strcmp);
 */
#define RBTREE_OFS_TO(type, field) ( ((intptr_t)&((((type)*)(void*)10000)->field)) - 10000 )
#define RBTREE_INIT( tree, object_type, node_field, key_field, compare ) rbtree_init( tree, RBTREE_OFS_TO(object_type, node_field) - RBTREE_OFS_TO(object_type, key_field), -RBTREE_OFS_TO(object_type, node_field), compare )

/* rbtree_destroy( tree, elem_destructor, opaque )
 *
 * Efficiently destroy a large tree by iterating bottom to top and running a destructor
 * on each node.  This avoids all the node removal operations.
 */
typedef void RBTreeElemDestructor( void *obj, void *opaque );
extern void rbtree_clear( RBTree_t *tree, RBTreeElemDestructor *del_fn, void *opaque );

/** rbtree_count( tree )
 *
 * Return the number of objects in the tree.
 */
static inline size_t rbtree_count( RBTree_t *tree ) { return tree->root_sentinel.left->count; }

/** rbtree_insert( tree, object )
 *
 * Insert an object into the tree.  The object must have a RBTreeNode at -tree->node_obj_ofs
 * or your program will crash.
 */
static inline void rbtree_insert( RBTree_t *tree, void *obj ) {
	rbtree_node_insert(tree->root_sentinel.left, (RBTreeNode_t*) (((char*)obj) - tree->node_obj_ofs), tree->compare, tree->node_key_ofs);
}

/** rbtree_remove( tree, object )
 *
 * Remove an object from the tree.  This does not delete the object.
 */
static inline void rbtree_remove( RBTree_t *tree, void *obj ) {
	rbtree_node_prune((RBTreeNode_t*) (((char*)obj) - tree->node_obj_ofs));
}

/** rbtree_find( tree, key )
 *
 * Search for an object with a key that matches 'key'.  Returns the object, or NULL if none match.
 */
static inline void *rbtree_find( RBTree_t *tree, const void *key ) {
	RBTreeNode_t *nearest= NULL;
	return rbtree_node_search(tree->root_sentinel.left, key, tree->compare, tree->node_key_ofs, NULL, NULL, &nearest, NULL)?
		((char*)nearest) + tree->node_obj_ofs: NULL;
}

/** rbtree_find_first( tree, key )
 *
 * In a tree with duplicate values, search for the lowest-indexed match to the key.
 * This allows you to iterate forward voer them.
 */
static inline void *rbtree_find_first( RBTree_t *tree, const void *key ) {
	RBTreeNode_t *first= NULL;
	return rbtree_node_search(tree->root_sentinel.left, key, tree->compare, tree->node_key_ofs, &first, NULL, NULL, NULL)?
		((char*)first) + tree->node_obj_ofs: NULL;
}

/** rbtree_find_all( tree, key, &first, &last )
 *
 * Find the first match, last match, and count of matches.
 */
static inline int rbtree_find_all( RBTree_t *tree, const void *key, void **first, void **last) {
	int count;
	rbtree_node_search(tree->root_sentinel.left, key, tree->compare, tree->node_key_ofs, (RBTreeNode_t**)first, (RBTreeNode_t**)last, NULL, &count);
	if (first && *first) { *first= ((char*)*first) + tree->node_obj_ofs; }
	if (last && *last) { *last= ((char*)*last) + tree->node_obj_ofs; }
	return count;
}

/** rbtree_elem_at( tree, index )
 *
 * Get the Nth element in the sorted list of the tree's elements.
 */
static inline void *rbtree_elem_at( RBTree_t *tree, size_t index ) {
	RBTreeNode_t *node= rbtree_node_child_at_index(tree->root_sentinel.left, index);
	return node? ((char*)node) + tree->node_obj_ofs : NULL;
}

/** rbtree_first( tree )
 *
 * Return the first element of the tree, in sort order.
 */
static inline void *rbtree_first( RBTree_t *tree ) {
	RBTreeNode_t *node= rbtree_node_left_leaf(tree->root_sentinel.left);
	return node? ((char*)node) + tree->node_obj_ofs : NULL;
}

/** rbtree_last( tree )
 *
 * Return the last element of the tree, in sort order.
 */
static inline void *rbtree_last( RBTree_t *tree ) {
	RBTreeNode_t *node= rbtree_node_right_leaf(tree->root_sentinel.left);
	return node? ((char*)node) + tree->node_obj_ofs : NULL;
}

/** rbtree_prev( tree )
 *
 * Return the previous element of the tree, in sort order.
 */
static inline void *rbtree_prev( RBTree_t *tree, void *obj ) {
	RBTreeNode_t *node= rbtree_node_prev( (RBTreeNode_t*) (((char*)obj) - tree->node_obj_ofs) );
	return node? ((char*)node) + tree->node_obj_ofs : NULL;
}

/** rbtree_next( tree )
 *
 * Return the next element of the tree, in sort order.
 */
static inline void *rbtree_next( RBTree_t *tree, void *obj ) {
	RBTreeNode_t *node= rbtree_node_next( (RBTreeNode_t*) (((char*)obj) - tree->node_obj_ofs) );
	return node? ((char*)node) + tree->node_obj_ofs : NULL;
}

#endif
