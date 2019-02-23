/*
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

#include "config.h"
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
#define NOT_SENTINEL(node)     ((bool) (node)->color)
#define NOT_ROOTSENTINEL(node) ((bool) (node)->parent)

#define KEY_FROM_NODE(node)    ((void*)(((char*)(void*)(node))+KeyOffset))
#define DATA_FROM_NODE(node)   ((void*)(((char*)(void*)(node))+DataOffset))
#define NODE_FROM_DATA(Data)   ((RBTreeNode_t_t*)(((char*)(void*)(Data))-DataOffset))

static void Balance( RBTreeNode_t* Current );
static void RotateRight( RBTreeNode_t* node );
static void RotateLeft( RBTreeNode_t* node );
static void PruneLeaf( RBTreeNode_t* node );
static void InsertAtLeaf(RBTreeNode_t *Leaf, RBTreeNode_t *NewNode, bool LeftSide);

int rbtree_node_index( RBTreeNode_t* node, int index ) {
	int LeftCount= -1;
	bool WentLeft= true;
	while (NOT_SENTINEL(node)) {
		if (WentLeft)
			LeftCount+= GET_COUNT(node->left)+1;
		WentLeft= node->Parent->right == node;
		node= node->Parent;
	}
	return LeftCount;
}

RBTreeNode_t* rbtree_node_left_leaf( RBTreeNode_t* node ) {
	while (NOT_SENTINEL(node->left))
		node= node->left;
	return node;
}

RBTreeNode_t* RBTNode_GetRightmost( RBTreeNode_t* node ) {
	while (NOT_SENTINEL(node->right))
		node= node->right;
	return node;
}

RBTreeNode_t* RBTNode_GetPrev( RBTreeNode_t* node ) {
	// If we are not at a leaf, move to the right-most node
	//  in the tree to the left of this node.
	if (NOT_SENTINEL(node->left)) {
		RBTreeNode_t* Current= node->left;
		while (NOT_SENTINEL(Current->right))
			Current= Current->right;
		return Current;
	}
	// Else walk up the tree until we see a parent node to the left
	else {
		RBTreeNode_t* Parent= node->Parent;
		while (Parent->left == node) {
			node= Parent;
			Parent= Parent->Parent;
			// Check for RootSentinel
			if (!Parent) return NULL;
		}
		return Parent;
	}
}

RBTreeNode_t* RBTNode_GetNext( RBTreeNode_t* node ) {
	// If we are not at a leaf, move to the left-most node
	//  in the tree to the right of this node.
	if (NOT_SENTINEL(node->right)) {
		RBTreeNode_t* Current= node->right;
		while (NOT_SENTINEL(Current->left))
			Current= Current->left;
		return Current;
	}
	// Else walk up the tree until we see a parent node to the right
	else {
		RBTreeNode_t* Parent= node->Parent;
		while (Parent->right == node) {
			node= Parent;
			Parent= node->Parent;
		}
		// Check for the RootSentinel
		if (!Parent->Parent) return NULL;

		return Parent;
	}
}

/** Find the Nth node in the tree, indexed from 0, from the left to right.
 * This operates by looking at the Count of the left subtree, to descend down to the Nth element.
 */
RBTreeNode_t* RBTNode_NodeByIndex( RBTreeNode_t* node, int index ) {
	if (index >= GET_COUNT(node))
		return NULL;
	while (index != GET_COUNT(node->left)) {
		if (index < GET_COUNT(node->left))
			node= node->left;
		else {
			index-= GET_COUNT(node->left)+1;
			node= node->right;
		}
	}
	return node;
}

/** Simplistic find algorithm.
 * This is useful when you know that a tree only contains one instance of a key, or if you only care about
 * finding a random instance of that key.
 */
RBTreeNode_t* RBTNode_Find( RBTreeNode_t *node, const void* SearchKey, RBTree_CompareProc* compare, int KeyOffset) {
	int cmp;
	while (NOT_SENTINEL(node)) {
		cmp= compare( SearchKey, KEY_FROM_NODE(node) );
		if      (cmp<0) node= node->left;
		else if (cmp>0) node= node->right;
		else return node;
	}
	return NULL;
}

/** Fancy find algorithm.
 * This function not only finds a node, but can find the nearest node to the one requested, finds the number of
 * matching nodes, and gets the first and last node so the matches can be iterated.
 */
int RBTNode_FindAll( RBTreeNode_t *node, const void* SearchKey, RBTree_CompareProc* compare,
	int KeyOffset, RBTreeNode_t** Result_First, RBTreeNode_t** Result_Last, RBTreeNode_t** Result_Nearest )
{
	RBTreeNode_t *Nearest= NULL, *First, *Last, *Test;
	int Count, cmp;
	
	while (NOT_SENTINEL(node)) {
		Nearest= node;
		cmp= compare( SearchKey, KEY_FROM_NODE(node) );
		if      (cmp<0) node= node->left;
		else if (cmp>0) node= node->right;
		else break;
	}
	if (Result_Nearest) *Result_Nearest= Nearest;
	// no matches
	if (IS_SENTINEL(node)) {
		if (Result_First) *Result_First= NULL;
		if (Result_Last) *Result_Last= NULL;
		return 0;
	}
	// we've found the head of the tree the matches will be found in
	First= Last= node;
	Count= 1;
	// We could play the recursion game, but we don't really need to.  this is a lot more efficient
	// Search the left tree for the first match
	Test= First->left;
	while (NOT_SENTINEL(Test)) {
		cmp= compare( SearchKey, KEY_FROM_NODE(Test) );
		if (cmp == 0) {
			First= Test;
			Count+= 1 + GET_COUNT(Test->right);
			Test= Test->left;
		}
		else /* cmp > 0 */
			Test= Test->right;
	}
	// Search the right tree for the last match
	Test= Last->right;
	while (NOT_SENTINEL(Test)) {
		cmp= compare( SearchKey, KEY_FROM_NODE(Test) );
		if (cmp == 0) {
			Last= Test;
			Count+= 1 + GET_COUNT(Test->left);
			Test= Test->right;
		}
		else /* cmp < 0 */
			Test= Test->left;
	}
	if (Result_First) *Result_First= First;
	if (Result_Last) *Result_Last= Last;
	return Count;
}

/** Insert a new object into the tree.
 * InsHint is optional, and should specify a node adjacent to where this item should be added.
 * If InsHint is not an adjacent node, the default insertion algorithm will be used.
 */
void RBTree_InsertWithHint(RBTree *Tree, void *DataItem, RBTreeNode_t *InsHint) {
	int KeyOffset= Tree->KeyOffset, DataOffset= Tree->DataOffset; // put these into local vars for purpose of macros
	RBTreeNode_t *NewNode= NODE_FROM_DATA(DataItem), *Current, *Next, *Prev;
	void* NewNodeKey= KEY_FROM_NODE(NewNode);
	int cmp;
	
	if (NewNode->Color_and_Count != 0) {
		printf("Attempt to use node which is already in tree!");
		abort();
	}
	
	NewNode->Color_and_Count= COLOR_and_COUNT(RED,1);
	NewNode->left=  &Tree->Sentinel;
	NewNode->right= &Tree->Sentinel;
	
	Current= Tree->RootSentinel.Left;
	
	if (InsHint) {
		cmp= Tree->compare(NewNodeKey, KEY_FROM_NODE(InsHint));
		if (cmp == 0) {
			// if it matches, then either side is fine for insertion.  But we might not be at a leaf.
			if (IS_SENTINEL(InsHint->left))
				InsertAtLeaf(InsHint, NewNode, true);
			else if (IS_SENTINEL(InsHint->right))
				InsertAtLeaf(InsHint, NewNode, false);
			else
				InsertAtLeaf(RBTNode_GetNext(InsHint), NewNode, true);
			return;
		}
		else if (cmp < 0) {
			Prev= RBTNode_GetPrev(InsHint);
			if (Prev == NULL || Tree->compare(KEY_FROM_NODE(Prev), NewNodeKey) <= 0) {
				// either the hint or its Prev must be a leaf node
				InsertAtLeaf(IS_SENTINEL(InsHint->left)? InsHint : Prev, NewNode, IS_SENTINEL(InsHint->left));
				return;
			}
		}
		else if (cmp > 0) {
			Next= RBTNode_GetNext(InsHint);
			if (Next == NULL || Tree->compare(KEY_FROM_NODE(Next), NewNodeKey) >= 0) {
				// either the hint or its Next must be a leaf node
				InsertAtLeaf(IS_SENTINEL(InsHint->right)? InsHint : Next, NewNode, IS_SENTINEL(InsHint->right));
				return;
			}
		}
		// if the hint was not valid, we fall through to the default algorithm
	}
	
	if (IS_SENTINEL(Current)) {
		Tree->RootSentinel.Left= NewNode;
		NewNode->Parent= &Tree->RootSentinel;
	}
	else {
		do {
			ADD_COUNT(Current, 1);
			// if the new node comes before the current node, go left
			if (Tree->compare( KEY_FROM_NODE(NewNode), KEY_FROM_NODE(Current) ) < 0) {
				if (IS_SENTINEL(Current->left)) {
					Current->left= NewNode;
					break;
				}
				else Current= Current->left;
			}
			// else go right
			else {
				if (IS_SENTINEL(Current->right)) {
					Current->right= NewNode;
					break;
				}
				else Current= Current->right;
			}
		} while (1);
		NewNode->Parent= Current;
		Balance( Current );
	}
	SET_COLOR_BLACK(Tree->RootSentinel.Left);
}

/** Small helper routine for RBTree_Insert */
void InsertAtLeaf(RBTreeNode_t *Leaf, RBTreeNode_t *NewNode, bool LeftSide) {
	RBTreeNode_t *node;
	if (LeftSide)
		Leaf->left= NewNode;
	else
		Leaf->right= NewNode;
	NewNode->Parent= Leaf;
	Balance(Leaf);
	for (node= Leaf; node->Parent != NULL; node= node->Parent)
		ADD_COUNT(node, 1);
	// We've iterated to the root sentinel- so node->left is the head of the tree.
	// Set the tree's root to black
	SET_COLOR_BLACK(node->left);
}

/**
 * Throughout the rest of the code, we use offsets from the RBTreeNode_t to reach the key and the containing object.
 * We calculate those offsets here, based on the sample object/key/node we are given.
 */
void RBTree_Init( RBTree* Tree, const void *SampleDataPtr, const RBTreeNode_t *SampleNodePtr,
	const void *SampleKeyPtr, RBTree_CompareProc* compare)
{
	Tree->RootSentinel.Color_and_Count= COLOR_and_COUNT(BLACK, 0); // zero nodes, black color
	Tree->RootSentinel.Left= &Tree->Sentinel;
	Tree->RootSentinel.Right= &Tree->Sentinel;
	Tree->RootSentinel.Parent= 0; // this uniquely marks this as the root sentinel
	Tree->Sentinel.Color_and_Count= COLOR_and_COUNT(BLACK, 0); // zero nodes, black color
	Tree->Sentinel.Left= &Tree->Sentinel;
	Tree->Sentinel.Right= &Tree->Sentinel;
	Tree->Sentinel.Parent= &Tree->Sentinel;
	Tree->DataOffset= (int)(((char*)SampleDataPtr) - ((char*)SampleNodePtr));
	Tree->KeyOffset= (int)(((char*)SampleKeyPtr) - ((char*)SampleNodePtr));
	Tree->compare= compare;
}

/** Mark all nodes as being not-in-tree, and possibly delete the objects that contain them.
 * DeleteProc is optional.  If given, it will be called on the 'Data' object which contains the RBTreeNode_t.
 */
void RBTree_Clear( RBTree* Tree, RBTree_DeleteProc* DelProc, void* Passback ) {
	RBTreeNode_t *Cur, *Next;
	int DataOffset= Tree->DataOffset; // copy to local var for macro
	int Direction;
	// make sure we actually have something to do
	if (Tree->RootSentinel.Left == &Tree->Sentinel)
		return;
	Cur= Tree->RootSentinel.Left;
	Direction= 0; // came from above
	while (Cur != &Tree->RootSentinel)
		switch (Direction) {
		case 0: // came from above, go down-left
			if (Cur->left != &Tree->Sentinel) {
				Cur= Cur->left;
				continue;
			}
		case 1: // came up from the left, go down-right
			if (Cur->right != &Tree->Sentinel) {
				Cur= Cur->right;
				Direction= 0;
				continue;
			}
		case 2: // came up from the right, kill the current node and proceed up
			Next= Cur->Parent;
			Cur->Color_and_Count= COLOR_and_COUNT(BLACK, 0);
			if (DelProc) DelProc(Passback, DATA_FROM_NODE(Cur));
			Direction= Next->right == Cur? 2 : 1;
			Cur= Next;
		}
	
	Tree->RootSentinel.Left= &Tree->Sentinel;
	Tree->RootSentinel.Color_and_Count= COLOR_and_COUNT(BLACK, 0);
}

void RotateRight( RBTreeNode_t* node ) {
	RBTreeNode_t* NewHead= node->left;
	RBTreeNode_t* Parent= node->Parent;

	if (Parent->right == node) Parent->right= NewHead;
	else Parent->left= NewHead;
	NewHead->Parent= Parent;

	ADD_COUNT(node, -1 - GET_COUNT(NewHead->left));
	ADD_COUNT(NewHead, 1 + GET_COUNT(node->right));
	node->left= NewHead->right;
	NewHead->right->Parent= node;

	NewHead->right= node;
	node->Parent= NewHead;
}

void RotateLeft( RBTreeNode_t* node ) {
	RBTreeNode_t* NewHead= node->right;
	RBTreeNode_t* Parent= node->Parent;

	if (Parent->right == node) Parent->right= NewHead;
	else Parent->left= NewHead;
	NewHead->Parent= Parent;

	ADD_COUNT(node, -1 - GET_COUNT(NewHead->right));
	ADD_COUNT(NewHead, 1 + GET_COUNT(node->left));
	node->right= NewHead->left;
	NewHead->left->Parent= node;

	NewHead->left= node;
	node->Parent= NewHead;
}

/** Re-balance a tree which has just had one element added.
 * Current is the parent node of the node just added.  The child is red.
 *
 * node counts are *not* updated by this method.
 */
void Balance( RBTreeNode_t* Current ) {
	// if Current is a black node, no rotations needed
	while (IS_RED(Current)) {
		// Current is red, the imbalanced child is red, and parent is black.

		RBTreeNode_t *Parent= Current->Parent;

		// if the Current is on the right of the parent, the parent is to the left
		if (Parent->right == Current) {
			// if the sibling is also red, we can pull down the color black from the parent
			if (IS_RED(Parent->left)) {
				SET_COLOR_BLACK(Parent->left);
				SET_COLOR_BLACK(Current);
				SET_COLOR_RED(Parent);
				// jump twice up the tree. if Current reaches the HeadSentinel (black node), the loop will stop
				Current= Parent->Parent;
				continue;
			}
			// if the imbalance (red node) is on the left, and the parent is on the left,
			//  a "prep-slide" is needed. (see diagram)
			if (IS_RED(Current->left))
				RotateRight( Current );

			// Now we can do our left rotation to balance the tree.
			RotateLeft( Parent );
			SET_COLOR_RED(Parent);
			SET_COLOR_BLACK(Parent->Parent);
			return;
		}
		// else the parent is to the right
		else {
			// if the sibling is also red, we can pull down the color black from the parent
			if (IS_RED(Parent->right)) {
				SET_COLOR_BLACK(Parent->right);
				SET_COLOR_BLACK(Current);
				SET_COLOR_RED(Parent);
				// jump twice up the tree. if Current reaches the HeadSentinel (black node), the loop will stop
				Current= Parent->Parent;
				continue;
			}
			// if the imbalance (red node) is on the right, and the parent is on the right,
			//  a "prep-slide" is needed. (see diagram)
			if (IS_RED(Current->right))
				RotateLeft( Current );

			// Now we can do our right rotation to balance the tree.
			RotateRight( Parent );
			SET_COLOR_RED(Parent);
			SET_COLOR_BLACK(Parent->Parent);
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
void RBTNode_Prune( RBTreeNode_t* Current ) {
	RBTreeNode_t *Temp, *Successor;
	if (GET_COUNT(Current) == 0)
		return;

	// If this is a leaf node (or almost a leaf) we can just prune it
	if (IS_SENTINEL(Current->left) || IS_SENTINEL(Current->right))
		PruneLeaf(Current);

	// Otherwise we need a successor.  We are guaranteed to have one because
	//  the current node has 2 children.
	else {
		// pick from the largest subtree
		Successor= (GET_COUNT(Current->left) > GET_COUNT(Current->right))? RBTNode_GetPrev( Current ) : RBTNode_GetNext( Current );
		PruneLeaf( Successor );

		// now exchange the successor for the current node
		Temp= Current->right;
		Successor->right= Temp;
		Temp->Parent= Successor;

		Temp= Current->left;
		Successor->left= Temp;
		Temp->Parent= Successor;

		Temp= Current->Parent;
		Successor->Parent= Temp;
		if (Temp->left == Current) Temp->left= Successor; else Temp->right= Successor;
		Successor->Color_and_Count= Current->Color_and_Count;
	}
	Current->left= Current->right= Current->Parent= NULL;
	Current->Color_and_Count= COLOR_and_COUNT(BLACK, 0);
}

/** PruneLeaf performs pruning of nodes with at most one child node.
 * This is the real heart of node deletion.
 * The first operation is to decrease the node count from node to RootSentinel.
 */
void PruneLeaf( RBTreeNode_t* node ) {
	RBTreeNode_t *Parent= node->Parent, *Current, *Sibling, *Sentinel;
	bool LeftSide= (Parent->left == node);
	Sentinel= IS_SENTINEL(node->left)? node->left : node->right;
	
	// first, decrement the count from here to RootSentinel
	for (Current= node; Current != NULL; Current= Current->Parent)
		ADD_COUNT(Current, -1);

	// if the node is red and has at most one child, then it has no child.
	// Prune it.
	if (IS_RED(node)) {
		if (LeftSide) Parent->left= Sentinel;
		else Parent->right= Sentinel;
		return;
	}

	// node is black here.  If it has a child, the child will be red.
	if (node->left != Sentinel) {
		// swap with child
		SET_COLOR_BLACK(node->left);
		node->left->Parent= Parent;
		if (LeftSide) Parent->left= node->left;
		else Parent->right= node->left;
		return;
	}
	if (node->right != Sentinel) {
		// swap with child
		SET_COLOR_BLACK(node->right);
		node->right->Parent= Parent;
		if (LeftSide) Parent->left= node->right;
		else Parent->right= node->right;
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
	if (LeftSide) Parent->left= Sentinel; else Parent->right= Sentinel;

	Sibling= (LeftSide)? Parent->right : Parent->left;
	Current= node;

	// Loop until the current node is red, or until we get to the root node.
	// (The root node's parent is the RootSentinel, which will have a NULL parent.)
	while (IS_BLACK(Current) && Parent->Parent != 0) {
		// If the sibling is red, we are unable to reduce the number of black
		//  nodes in the sibling tree, and we can't increase the number of black
		//  nodes in our tree..  Thus we must do a rotation from the sibling
		//  tree to our tree to give us some extra (red) nodes to play with.
		// This is Case 1 from the text
		if (IS_RED(Sibling)) {
			SET_COLOR_RED(Parent);
			SET_COLOR_BLACK(Sibling);
			if (LeftSide) {
				RotateLeft(Parent);
				Sibling= Parent->right;
			}
			else {
				RotateRight(Parent);
				Sibling= Parent->left;
			}
			continue;
		}
		// Sibling will be black here

		// If the sibling is black and both children are black, we have to
		//  reduce the black node count in the sibling's tree to match ours.
		// This is Case 2a from the text.
		if (IS_BLACK(Sibling->right) && IS_BLACK(Sibling->left)) {
			SET_COLOR_RED(Sibling);
			// Now we move one level up the tree to continue fixing the
			// other branches.
			Current= Parent;
			Parent= Current->Parent;
			LeftSide= (Parent->left == Current);
			Sibling= (LeftSide)? Parent->right : Parent->left;
			continue;
		}
		// Sibling will be black with 1 or 2 red children here

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
		if (LeftSide) {
			if (IS_BLACK(Sibling->right)) { // Case 3 from the text
				RotateRight( Sibling );
				Sibling= Parent->right;
			}
			// now Case 4 from the text
			SET_COLOR_BLACK(Sibling->right);
			COPY_COLOR(Sibling, Parent); //Sibling->Color= Parent->Color;
			SET_COLOR_BLACK(Parent);

			Current= Parent;
			Parent= Current->Parent;
			RotateLeft( Current );
			return;
		}
		else {
			if (IS_BLACK(Sibling->left)) { // Case 3 from the text
				RotateLeft( Sibling );
				Sibling= Parent->left;
			}
			// now Case 4 from the text
			SET_COLOR_BLACK(Sibling->left);
			COPY_COLOR(Sibling, Parent);
			SET_COLOR_BLACK(Parent);

			Current= Parent;
			Parent= Current->Parent;
			RotateRight( Current );
			return;
		}
	}

	// Now, make the current node black (to fulfill Case 2b)
	// Case 4 will have exited directly out of the function.
	// If we stopped because we reached the top of the tree,
	//   the head is black anyway so don't worry about it.
	SET_COLOR_BLACK(Current);
}
