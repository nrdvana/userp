/******************************************************************************\
*   RBTree.h                        by: TheSilverDirk / Michael Conrad
*   Created: 06/23/2000             Last Modified: 04/29/2005
*
*   2005-04-29: Hacked-up sufficiently to be compilable under C.
*
*   This is a red/black binary search tree implementation using the
*   "contained class" system.  The "inorder" and "compare" function pointers
*   allow you to do custom sorting.  These pointers MUST point to valid
*   functions before you use the tree.
\******************************************************************************/

#ifndef RBTREE_H
#define RBTREE_H

#ifdef __cplusplus
namespace ExoDataStruct {
extern "C" {
#endif

/**
 * Red/Black tree node.  Nodes point to other nodes, so pointer math is
 * needed to reach the data paired with the node.
 */
typedef struct rb_tree_node {
	struct rb_tree_node *Left, *Right, *Parent;
	unsigned int Color_and_Count;
} RBTreeNode;

typedef int RBTree_CompareProc( const void* Key_A, const void *Key_B );

extern void RBTNode_Init( RBTreeNode* Node );
extern bool RBTNode_IsSentinel( RBTreeNode *Node );
extern int  RBTNode_GetIndex( RBTreeNode* Node, int NodeIdx );
#ifdef USE_INLINES
  inline bool RBTNode_IsBlack(RBTreeNode* node)  { return !(node->Color_and_Count&1); }
  inline bool RBTNode_IsRed(RBTreeNode* node)    { return node->Color_and_Count&1; }
  inline int  RBTNode_GetCount(RBTreeNode* node) { return node->Color_and_Count>>1; }
#else
  #define RBTNode_IsBlack(node)  (!((node)->Color_and_Count&1))
  #define RBTNode_IsRed(node)    ((node)->Color_and_Count&1)
  #define RBTNode_GetCount(node) ((int) ((node)->Color_and_Count>>1))
#endif
extern RBTreeNode* RBTNode_GetPrev( RBTreeNode* Node );
extern RBTreeNode* RBTNode_GetNext( RBTreeNode* Node );
extern RBTreeNode* RBTNode_GetRightmost( RBTreeNode* SubtreeRoot );
extern RBTreeNode* RBTNode_GetLeftmost( RBTreeNode* SubtreeRoot );
extern RBTreeNode* RBTNode_NodeByIndex( RBTreeNode* SubtreeRoot, int index );
extern RBTreeNode* RBTNode_Find( RBTreeNode *Node, const void* SearchKey, RBTree_CompareProc* compare, int KeyOffset );
extern int RBTNode_FindAll( RBTreeNode *Node, const void* SearchKey, RBTree_CompareProc* compare, int KeyOffset, RBTreeNode** Result_First, RBTreeNode** Result_Last, RBTreeNode** Result_Nearest );
extern void RBTNode_Prune( RBTreeNode* Node );

typedef struct RBTree_t {
	RBTreeNode Sentinel, RootSentinel; // the left child of the RootSentinel is the root node
	int KeyOffset, DataOffset;
	RBTree_CompareProc *compare;
} RBTree;

extern void RBTree_Init( RBTree* Tree, const void *SampleDataPtr, const RBTreeNode *SampleNodePtr, const void *SampleKeyPtr, RBTree_CompareProc* compare);
typedef void RBTree_DeleteProc(void* Passback, void* Obj);
extern void RBTree_Clear( RBTree* Tree, RBTree_DeleteProc* DelProc, void* Passback );
extern void RBTree_InsertWithHint(RBTree *Tree, void *DataItem, RBTreeNode *InsHint);
#ifdef USE_INLINES
  inline int RBTree_GetCount(RBTree *Tree)
	{ return RBTNode_GetCount(&Tree->RootSentinel); }
  inline RBTreeNode* RBTree_Find( RBTree *Tree, void *SearchKey )
	{ return RBTNode_Find(Tree->RootSentinel.Left, SearchKey, Tree->compare, Tree->KeyOffset); }
  inline int RBTree_FindAll( RBTree *Tree, void *SearchKey, RBTreeNode** Result_First, RBTreeNode** Result_Last, RBTreeNode** Result_Nearest )
	{ return RBTNode_FindAll(Tree->RootSentinel.Left, SearchKey, Tree->compare, Tree->KeyOffset, Result_First, Result_Last, Result_Nearest); }
  inline RBTreeNode* RBTree_First( RBTree *Tree )
	{ return RBTree_GetCount(Tree)? RBTNode_GetLeftmost((Tree)->RootSentinel.Left) : NULL; }
  inline RBTreeNode* RBTree_Last( RBTree *Tree )
	{ return RBTree_GetCount(Tree)? RBTNode_GetRightmost((Tree)->RootSentinel.Left) : NULL; }
  inline void RBTree_Insert( RBTree *Tree, void *DataItem )
	{ RBTree_InsertWithHint(Tree, DataItem, 0); }
  inline void* RBTree_DataFromNode( RBTree *Tree, RBTreeNode *Node)
	{ return (void*) (((char*)(void*)Node)+Tree->DataOffset); }
  inline void* RBTree_KeyFromNode( RBTree *Tree, RBTreeNode *Node)
	{ return (void*) (((char*)(void*)Node)+Tree->KeyOffset); }
#else
  #define RBTree_FIND( Tree, SearchKey )  (RBTNode_Find((Tree)->RootSentinel.Left, SearchKey, (Tree)->compare, (Tree)->KeyOffset))
  #define RBTree_FINDALL( Tree, SearchKey, Result_First, Result_Last, Result_Nearest ) (RBTNode_FindAll((Tree)->RootSentinel.Left, SearchKey, (Tree)->compare, (Tree)->KeyOffset, Result_First, Result_Last, Result_Nearest))
  #define RBTree_FIRST( Tree )            (RBTree_GetCount(Tree)? RBTNode_GetLeftmost((Tree)->RootSentinel.Left) : NULL)
  #define RBTree_LAST( Tree )             (RBTree_GetCount(Tree)? RBTNode_GetRightmost((Tree)->RootSentinel.Left) : NULL)
  #define RBTree_Insert( Tree, DataItem ) (RBTree_InsertWithHint(Tree, DataItem, 0))
  #define RBTree_DataFromNode(Tree, Node) ((void*) (((char*)(void*)(Node))+(Tree)->DataOffset))
  #define RBTree_KeyFromNode(Tree, Node)  ((void*) (((char*)(void*)(Node))+(Tree)->KeyOffset))
  #define RBTree_GetCount(Tree)           (RBTNode_GetCount(&(Tree)->RootSentinel))
#endif

/******************************************************************************\
*   Open RBTree Container
\******************************************************************************/
/*
class ECannotAddNode {};
class ECannotRemoveNode {};

class RBTree {
public:
	class Node: public RBTreeNode {
	public:
		Node()  { RBTreeNode_Init(this); }
		~Node() { if (RBTreeNode::Color != RBTreeNode::Unassigned) RBTree_Prune(this); }

		// The average user shouldn't need these, but they might come in handy.
		void* Left()   const { return RBTreeNode::Left->Object;   }
		void* Right()  const { return RBTreeNode::Right->Object;  }
		void* Parent() const { return RBTreeNode::Parent->Object; }
		int   Color()        { return RBTreeNode::Color; }

		// These let you use your nodes as a sequence.
		void* Next()   const { return RBTree_GetNext(this)->Object; }
		void* Prev()   const { return RBTree_GetPrev(this)->Object; }

		bool IsSentinel() { return RBTreeNode_IsSentinel(this); }

		friend RBTree;
	};

private:
	RBTreeNode RootSentinel; // the left child of the sentinel is the root node
public:
	RBTree_inorder_func *inorder;
	RBTree_compare_func *compare;

	RBTree() { RBTree_InitRootSentinel(RootSentinel); }
	~RBTree() { Clear(); }

	void Clear() { RBTree_Clear(RootSentinel); }
	bool IsEmpty() const { return RBTreeNode_IsSentinel(RootSentinel.Left); }

	void* GetRoot()  const { return RootSentinel.Left->Object; }
	void* GetFirst() const { return RBTree_GetLeftmost(RootSentinel.Left)->Object; }
	void* GetLast()  const { return RBTree_GetRightmost(RootSentinel.Left)->Object; }

	void Add( Node* NewNode ) { if (!RBTree_Add(RootSentinel, NewNode, inorder)) throw ECannotAddNode(); }

	void* Find( const void *SearchKey ) const { return RBTree_Find(RootSentinel, SearchKey, compare)->Object; }

	static void Remove( Node* Node ) { if (!RBTree_Prune(Node)) throw ECannotRemoveNode(); }
};
*/

/******************************************************************************\
*   Type-Safe Contained Red/Black Tree Class                                   *
\******************************************************************************/
/*
template <class T>
class TypedRBTree: public RBTree {
public:
	typedef RBTree Inherited;

	class Node: public RBTree::Node {
	public:
		typedef RBTree::Node Inherited;

		// The average user shouldn't need these, but they might come in handy.
		T* Left()   const { return (T*) Inherited::Left();   }
		T* Right()  const { return (T*) Inherited::Right();  }
		T* Parent() const { return (T*) Inherited::Parent(); }
		int Color() const { return Inherited::Color(); }

		// These let you use your nodes as a sequence.
		T* Next()   const { return (T*) Inherited::Next(); }
		T* Prev()   const { return (T*) Inherited::Prev(); }

		friend TypedRBTree<T>;
	};

public:
	T* GetRoot()  const { return (T*) Inherited::GetRoot();  }
	T* GetFirst() const { return (T*) Inherited::GetFirst(); }
	T* GetLast()  const { return (T*) Inherited::GetLast();  }

	void Add( Node* NewNode ) { Inherited::Add(NewNode); }

	T* Find( const void *Val ) const { return (T*) Inherited::Find(Val); }

	static void Remove( Node* Node ) { Inherited::Remove(Node); }
};

}
*/

#ifdef __cplusplus
} // namespace
} // extern "C"
#endif

#endif

