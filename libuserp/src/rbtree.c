/*
Credits:

  Intrest in red/black trees was inspired by Dr. John Franco, and his animated
    red/black tree java applet.
  http://www.ececs.uc.edu/~franco/C321/html/RedBlack/redblack.html

  The node insertion code was written in a joint effort with Anthony Deeter,
    as a class project.

  The red/black deletion algorithm was derived from the deletion patterns in
   "Fundamentals of Sequential and Parallel Algorithms",
    by Dr. Kenneth A. Berman and Dr. Jerome L. Paul

  I also got the sentinel node idea from this book.

*/

#include "config_plus.h"
#include "rbtree.h"

#define RED 1
#define BLACK 0
#define SET_COLOR_BLACK(node)  ((node)->color= BLACK)
#define SET_COLOR_RED(node)    ((node)->color= RED)
#define COPY_COLOR(dest,src)   ((dest)->color= (src)->color)
#define IS_BLACK(node)         (!(node)->color)
#define IS_RED(node)           ((node)->color)
#define ADD_COUNT(node,val)    ((node)->count+= val)
#define GET_COUNT(node)        ((node)->count)
#define IS_SENTINEL(node)      (!(bool) (node)->color)
#define IS_ROOTSENTINEL(node)  (!(bool) (node)->parent)
#define NOT_SENTINEL(node)     ((bool) (node)->color)
#define NOT_ROOTSENTINEL(node) ((bool) (node)->parent)

#define KEY_FROM_NODE(node)    ((void*)(((char*)(void*)(node))+node_key_ofs))
#define DATA_FROM_NODE(node)   ((void*)(((char*)(void*)(node))+node_obj_ofs))
#define NODE_FROM_DATA(obj)    ((RBTreeNode_t_t*)(((char*)(void*)(obj))-node_obj_ofs))

static void Balance( RBTreeNode_t* current );
static void RotateRight( RBTreeNode_t* node );
static void RotateLeft( RBTreeNode_t* node );
static void PruneLeaf( RBTreeNode_t* node );
static void InsertAtLeaf(RBTreeNode_t *leaf, RBTreeNode_t *new_node, bool on_left);

int rbtree_node_index( RBTreeNode_t* node ) {
	int left_count= node->left->count;
	RBTreeNode_t *prev= node;
	node= node->parent;
	while (NOT_SENTINEL(node)) {
		if (node->right == prev)
			left_count += GET_COUNT(node->left)+1;
		prev= node;
		node= node->parent;
	}
	return left_count;
}

RBTreeNode_t* rbtree_node_left_leaf( RBTreeNode_t* node ) {
	while (NOT_SENTINEL(node->left))
		node= node->left;
	return node;
}

RBTreeNode_t* rbtree_node_right_leaf( RBTreeNode_t* node ) {
	while (NOT_SENTINEL(node->right))
		node= node->right;
	return node;
}

RBTreeNode_t* rbtree_node_prev( RBTreeNode_t* node ) {
	// If we are not at a leaf, move to the right-most node
	//  in the tree to the left of this node.
	if (NOT_SENTINEL(node->left)) {
		node= node->left;
		while (NOT_SENTINEL(node->right))
			node= node->right;
		return node;
	}
	// Else walk up the tree until we see a parent node to the left
	else {
		RBTreeNode_t* parent= node->parent;
		while (parent->left == node) {
			node= parent;
			parent= parent->parent;
			// Check for root_sentinel
			if (!parent) return NULL;
		}
		return parent;
	}
}

RBTreeNode_t* rbtree_node_next( RBTreeNode_t* node ) {
	// If we are not at a leaf, move to the left-most node
	//  in the tree to the right of this node.
	if (NOT_SENTINEL(node->right)) {
		node= node->right;
		while (NOT_SENTINEL(node->left))
			node= node->left;
		return node;
	}
	// Else walk up the tree until we see a parent node to the right
	else {
		RBTreeNode_t* parent= node->parent;
		while (parent->right == node) {
			node= parent;
			parent= node->parent;
		}
		// Check for the root_sentinel
		if (!parent->parent) return NULL;
		return parent;
	}
}

/** Find the Nth node in the tree, indexed from 0, from the left to right.
 * This operates by looking at the count of the left subtree, to descend down to the Nth element.
 */
RBTreeNode_t* rbtree_node_child_at_index( RBTreeNode_t* node, size_t index ) {
	if (index >= GET_COUNT(node))
		return NULL;
	while (index != GET_COUNT(node->left)) {
		if (index < GET_COUNT(node->left))
			node= node->left;
		else {
			index -= GET_COUNT(node->left)+1;
			node= node->right;
		}
	}
	return node;
}

/** Fancy find algorithm.
 * This function not only finds a node, but can find the nearest node to the one requested, finds the number of
 * matching nodes, and gets the first and last node so the matches can be iterated.
 */
bool rbtree_node_search(
	RBTreeNode_t *node, const void* key, RBTreeCmpFn *compare, int node_key_ofs,
	RBTreeNode_t **result_first, RBTreeNode_t **result_last, RBTreeNode_t **result_nearest,
	int *result_count
) {
	RBTreeNode_t *nearest= NULL, *first, *last, *test;
	int count, cmp;
	
	while (1) {
		if (IS_SENTINEL(node)) {
			if (result_nearest) *result_nearest= NULL;
			if (result_first) *result_first= NULL;
			if (result_last) *result_last= NULL;
			if (result_count) *result_count= 0;
			return false;
		}
		nearest= node;
		cmp= compare( key, KEY_FROM_NODE(node) );
		if      (cmp<0) node= node->left;
		else if (cmp>0) node= node->right;
		else break;
	}
	// no matches
	if (result_nearest) *result_nearest= nearest;
	// we've found the head of the tree the matches will be found in
	count= 1;
	if (result_first || result_count) {
		// Search the left tree for the first match
		first= node;
		test= first->left;
		while (NOT_SENTINEL(test)) {
			cmp= compare( key, KEY_FROM_NODE(test) );
			if (cmp == 0) {
				first= test;
				count+= 1 + GET_COUNT(test->right);
				test= test->left;
			}
			else /* cmp > 0 */
				test= test->right;
		}
		if (result_first) *result_first= first;
	}
	if (result_last || result_count) {
		// Search the right tree for the last match
		last= node;
		test= last->right;
		while (NOT_SENTINEL(test)) {
			cmp= compare( key, KEY_FROM_NODE(test) );
			if (cmp == 0) {
				last= test;
				count+= 1 + GET_COUNT(test->left);
				test= test->right;
			}
			else /* cmp < 0 */
				test= test->left;
		}
		if (result_last) *result_last= last;
		if (result_count) *result_count= count;
	}
	return true;
}

/* Insert a new object into the tree.  The initial node is called 'hint' because if the new node
 * isn't a child of hint, this will backtrack up the tree to find the actual insertion point.
 */
void rbtree_node_insert( RBTreeNode_t *hint, RBTreeNode_t *node, RBTreeCmpFn *compare, int node_key_ofs) {
	// Can't insert node if it is already in the tree
	if (GET_COUNT(node))
		return;
	// check for first node scenario
	if (IS_ROOTSENTINEL(hint) && IS_SENTINEL(hint->left)) {
		hint->left= node;
		node->parent= hint;
		node->left= hint->right; // tree's leaf sentinel
		node->right= hint->right;
		node->count= 1;
		node->color= BLACK;
		return;
	}
	// Can't add to a normal sentinel
	if (IS_SENTINEL(hint))
		return;
	// else traverse hint until leaf
	void *key= KEY_FROM_NODE(node);
	int cmp;
	bool leftmost= true, rightmost= true;
	RBTreeNode_t *pos= hint, *next, *parent;
	while (1) {
		cmp= compare( key, KEY_FROM_NODE(pos) );
		if (cmp < 0) {
			rightmost= false;
			next= pos->left;
		} else {
			leftmost= false;
			next= pos->right;
		}
		if (IS_SENTINEL(next))
			break;
		pos= next;
	}
	// If the original hint was not the root of the tree, and cmp indicate the same direction 
	// as leftmost or rightmost, then backtrack and see if we're completely in the wrong spot.
	if (NOT_ROOTSENTINEL(hint->parent) && (cmp < 0? leftmost : rightmost)) {
		// we are the leftmost child of hint, so if there is a parent to the left,
		// key needs to not compare less else we have to start over.
		parent= hint->parent;
		while (1) {
			if ((cmp < 0? parent->right : parent->left) == hint) {
				if ((cmp < 0) == (compare(key, KEY_FROM_NODE(parent)) < 0)) {
					// Whoops.  Hint was wrong.  Should start over from root.
					while (NOT_ROOTSENTINEL(parent->parent))
						parent= parent->parent;
					rbtree_node_insert(parent, node, compare, node_key_ofs);
					return;
				}
				else break; // we're fine afterall
			}
			else if (IS_ROOTSENTINEL(parent->parent))
				break; // we're fine afterall
			parent= parent->parent;
		}
	}
	if (cmp < 0)
		pos->left= node;
	else
		pos->right= node;
	node->parent= pos;
	// next is pointing to the leaf-sentinel for this tree after exiting loop above
	node->left= next;
	node->right= next;
	node->count= 1;
	node->color= RED;
	Balance(pos);
	for (parent= pos; NOT_ROOTSENTINEL(parent); parent= parent->parent)
		ADD_COUNT(parent, 1);
	// We've iterated to the root sentinel- so node->left is the head of the tree.
	// Set the tree's root to black
	SET_COLOR_BLACK(parent->left);
}

void RotateRight( RBTreeNode_t* node ) {
	RBTreeNode_t* NewHead= node->left;
	RBTreeNode_t* parent= node->parent;

	if (parent->right == node) parent->right= NewHead;
	else parent->left= NewHead;
	NewHead->parent= parent;

	ADD_COUNT(node, -1 - GET_COUNT(NewHead->left));
	ADD_COUNT(NewHead, 1 + GET_COUNT(node->right));
	node->left= NewHead->right;
	NewHead->right->parent= node;

	NewHead->right= node;
	node->parent= NewHead;
}

void RotateLeft( RBTreeNode_t* node ) {
	RBTreeNode_t* NewHead= node->right;
	RBTreeNode_t* parent= node->parent;

	if (parent->right == node) parent->right= NewHead;
	else parent->left= NewHead;
	NewHead->parent= parent;

	ADD_COUNT(node, -1 - GET_COUNT(NewHead->right));
	ADD_COUNT(NewHead, 1 + GET_COUNT(node->left));
	node->right= NewHead->left;
	NewHead->left->parent= node;

	NewHead->left= node;
	node->parent= NewHead;
}

/** Re-balance a tree which has just had one element added.
 * current is the parent node of the node just added.  The child is red.
 *
 * node counts are *not* updated by this method.
 */
void Balance( RBTreeNode_t* current ) {
	// if current is a black node, no rotations needed
	while (IS_RED(current)) {
		// current is red, the imbalanced child is red, and parent is black.

		RBTreeNode_t *parent= current->parent;

		// if the current is on the right of the parent, the parent is to the left
		if (parent->right == current) {
			// if the sibling is also red, we can pull down the color black from the parent
			if (IS_RED(parent->left)) {
				SET_COLOR_BLACK(parent->left);
				SET_COLOR_BLACK(current);
				SET_COLOR_RED(parent);
				// jump twice up the tree. if current reaches the HeadSentinel (black node), the loop will stop
				current= parent->parent;
				continue;
			}
			// if the imbalance (red node) is on the left, and the parent is on the left,
			//  a "prep-slide" is needed. (see diagram)
			if (IS_RED(current->left))
				RotateRight( current );

			// Now we can do our left rotation to balance the tree.
			RotateLeft( parent );
			SET_COLOR_RED(parent);
			SET_COLOR_BLACK(parent->parent);
			return;
		}
		// else the parent is to the right
		else {
			// if the sibling is also red, we can pull down the color black from the parent
			if (IS_RED(parent->right)) {
				SET_COLOR_BLACK(parent->right);
				SET_COLOR_BLACK(current);
				SET_COLOR_RED(parent);
				// jump twice up the tree. if current reaches the HeadSentinel (black node), the loop will stop
				current= parent->parent;
				continue;
			}
			// if the imbalance (red node) is on the right, and the parent is on the right,
			//  a "prep-slide" is needed. (see diagram)
			if (IS_RED(current->right))
				RotateLeft( current );

			// Now we can do our right rotation to balance the tree.
			RotateRight( parent );
			SET_COLOR_RED(parent);
			SET_COLOR_BLACK(parent->parent);
			return;
		}
	}
	// note that we should now set the root node to be black.
	// but the caller does this anyway.
	return;
}

/** Prune a node from anywhere in the tree.
 * If the node is a leaf, it can be removed easily.  Otherwise we must swap the node for a leaf node
 * with an adjacent key value, and then remove from the position of that leaf.
 *
 * This function *does* update node counts.
 */
void rbtree_node_prune( RBTreeNode_t* current ) {
	RBTreeNode_t *temp, *successor;
	if (GET_COUNT(current) == 0)
		return;

	// If this is a leaf node (or almost a leaf) we can just prune it
	if (IS_SENTINEL(current->left) || IS_SENTINEL(current->right))
		PruneLeaf(current);

	// Otherwise we need a successor.  We are guaranteed to have one because
	//  the current node has 2 children.
	else {
		// pick from the largest subtree
		successor= (GET_COUNT(current->left) > GET_COUNT(current->right))? rbtree_node_prev( current ) : rbtree_node_next( current );
		PruneLeaf( successor );

		// now exchange the successor for the current node
		temp= current->right;
		successor->right= temp;
		temp->parent= successor;

		temp= current->left;
		successor->left= temp;
		temp->parent= successor;

		temp= current->parent;
		successor->parent= temp;
		if (temp->left == current) temp->left= successor; else temp->right= successor;
		successor->color= current->color;
		successor->count= current->count;
	}
	current->left= current->right= current->parent= NULL;
	current->color= BLACK;
	current->count= 0;
}

/** PruneLeaf performs pruning of nodes with at most one child node.
 * This is the real heart of node deletion.
 * The first operation is to decrease the node count from node to root_sentinel.
 */
void PruneLeaf( RBTreeNode_t* node ) {
	RBTreeNode_t *parent= node->parent, *current, *sibling, *sentinel;
	bool leftside= (parent->left == node);
	sentinel= IS_SENTINEL(node->left)? node->left : node->right;
	
	// first, decrement the count from here to root_sentinel
	for (current= node; current != NULL; current= current->parent)
		ADD_COUNT(current, -1);

	// if the node is red and has at most one child, then it has no child.
	// Prune it.
	if (IS_RED(node)) {
		if (leftside) parent->left= sentinel;
		else parent->right= sentinel;
		return;
	}

	// node is black here.  If it has a child, the child will be red.
	if (node->left != sentinel) {
		// swap with child
		SET_COLOR_BLACK(node->left);
		node->left->parent= parent;
		if (leftside) parent->left= node->left;
		else parent->right= node->left;
		return;
	}
	if (node->right != sentinel) {
		// swap with child
		SET_COLOR_BLACK(node->right);
		node->right->parent= parent;
		if (leftside) parent->left= node->right;
		else parent->right= node->right;
		return;
	}

/*	Now, we have determined that node is a black leaf node with no children.
	The tree must have the same number of black nodes along any path from root
	to leaf.  We want to remove a black node, disrupting the number of black
	nodes along the path from the root to the current leaf.  To correct this,
	we must either shorten all other paths, or add a black node to the current
	path.  Then we can freely remove our black leaf.

	While we are pointing to it, we will go ahead and delete the leaf and
	replace it with the sentinel (which is also black, so it won't affect
	the algorithm).
*/
	if (leftside) parent->left= sentinel; else parent->right= sentinel;

	sibling= (leftside)? parent->right : parent->left;
	current= node;

	// Loop until the current node is red, or until we get to the root node.
	// (The root node's parent is the root_sentinel, which will have a NULL parent.)
	while (IS_BLACK(current) && parent->parent != 0) {
		// If the sibling is red, we are unable to reduce the number of black
		//  nodes in the sibling tree, and we can't increase the number of black
		//  nodes in our tree..  Thus we must do a rotation from the sibling
		//  tree to our tree to give us some extra (red) nodes to play with.
		// This is Case 1 from the text
		if (IS_RED(sibling)) {
			SET_COLOR_RED(parent);
			SET_COLOR_BLACK(sibling);
			if (leftside) {
				RotateLeft(parent);
				sibling= parent->right;
			}
			else {
				RotateRight(parent);
				sibling= parent->left;
			}
			continue;
		}
		// sibling will be black here

		// If the sibling is black and both children are black, we have to
		//  reduce the black node count in the sibling's tree to match ours.
		// This is Case 2a from the text.
		if (IS_BLACK(sibling->right) && IS_BLACK(sibling->left)) {
			SET_COLOR_RED(sibling);
			// Now we move one level up the tree to continue fixing the
			// other branches.
			current= parent;
			parent= current->parent;
			leftside= (parent->left == current);
			sibling= (leftside)? parent->right : parent->left;
			continue;
		}
		// sibling will be black with 1 or 2 red children here

		// << Case 2b is handled by the while loop. >>

		// If one of the sibling's children are red, we again can't make the
		//  sibling red to balance the tree at the parent, so we have to do a
		//  rotation.  If the "near" nephew is red and the "far" nephew is
		//  black, we need to rotate that tree rightward before rotating the
		//  parent leftward.
		// After doing a rotation and rearranging a few colors, the effect is
		//  that we maintain the same number of black nodes per path on the far
		//  side of the parent, and we gain a black node on the current side,
		//  so we are done.
		// This is Case 4 from the text. (Case 3 is the double rotation)
		if (leftside) {
			if (IS_BLACK(sibling->right)) { // Case 3 from the text
				RotateRight( sibling );
				sibling= parent->right;
			}
			// now Case 4 from the text
			SET_COLOR_BLACK(sibling->right);
			COPY_COLOR(sibling, parent); //sibling->Color= parent->Color;
			SET_COLOR_BLACK(parent);

			current= parent;
			parent= current->parent;
			RotateLeft( current );
			return;
		}
		else {
			if (IS_BLACK(sibling->left)) { // Case 3 from the text
				RotateLeft( sibling );
				sibling= parent->left;
			}
			// now Case 4 from the text
			SET_COLOR_BLACK(sibling->left);
			COPY_COLOR(sibling, parent);
			SET_COLOR_BLACK(parent);

			current= parent;
			parent= current->parent;
			RotateRight( current );
			return;
		}
	}

	// Now, make the current node black (to fulfill Case 2b)
	// Case 4 will have exited directly out of the function.
	// If we stopped because we reached the top of the tree,
	//   the head is black anyway so don't worry about it.
	SET_COLOR_BLACK(current);
}

/** Mark all nodes as being not-in-tree, and possibly delete the objects that contain them.
 * DeleteProc is optional.  If given, it will be called on the 'Data' object which contains the RBTreeNode_t.
 */
void rbtree_clear( RBTree_t *tree, RBTreeElemDestructor *del_fn, void *opaque ) {
	RBTreeNode_t *current, *next;
	int node_key_ofs= tree->node_key_ofs; // copy to local var for macro
	int node_obj_ofs= tree->node_obj_ofs;
	int direction;
	// make sure we actually have something to do
	if (tree->root_sentinel.left == &tree->sentinel)
		return;
	current= tree->root_sentinel.left;
	direction= 0; // came from above
	while (current != &tree->root_sentinel)
		switch (direction) {
		case 0: // came from above, go down-left
			if (current->left != &tree->sentinel) {
				current= current->left;
				continue;
			}
		case 1: // came up from the left, go down-right
			if (current->right != &tree->sentinel) {
				current= current->right;
				direction= 0;
				continue;
			}
		case 2: // came up from the right, kill the current node and proceed up
			next= current->parent;
			current->count= 0;
			direction= next->right == current? 2 : 1;
			if (del_fn) del_fn(DATA_FROM_NODE(current), opaque);
			current= next;
		}
	
	tree->root_sentinel.left= &tree->sentinel;
	tree->root_sentinel.color= BLACK;
	tree->root_sentinel.count= 0;
}
