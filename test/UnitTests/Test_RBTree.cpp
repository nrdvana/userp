struct SomeObject {
	char Unused[8];
	int32_t Key;
	RBTreeNode Node;
	
	SomeObject(int KeyVal): Key(KeyVal) { RBTNode_Init(&Node); }
};

void CheckSubtree(RBTree *tree, RBTreeNode *Pos, int *BlackCount);
void CheckTree(RBTree *tree) {
	AssertEQ(false, (bool)RBTNode_IsRed(&tree->RootSentinel));
	AssertEQ(false, (bool)RBTNode_IsRed(tree->RootSentinel.Left));
	AssertEQ(false, (bool)RBTNode_IsRed(&tree->Sentinel));
	AssertEQ(0, RBTNode_GetCount(&tree->Sentinel));
	AssertEQ(&tree->Sentinel, tree->Sentinel.Left);
	AssertEQ(&tree->Sentinel, tree->Sentinel.Right);
	AssertEQ(&tree->Sentinel, tree->Sentinel.Parent);
	AssertEQ(&tree->Sentinel, tree->RootSentinel.Right);
	int unused;
	if (tree->RootSentinel.Left != &tree->Sentinel)
		CheckSubtree(tree, tree->RootSentinel.Left, &unused);
}

void CheckSubtree(RBTree* tree, RBTreeNode *Pos, int *BlackCount) {
	AssertGT(0, RBTNode_GetCount(Pos));
	AssertEQ(RBTNode_GetCount(Pos->Left) + RBTNode_GetCount(Pos->Right), RBTNode_GetCount(Pos));
	int LeftBlackNodes= 0, RightBlackNodes= 0;
	if (Pos->Left != &tree->Sentinel)
		CheckSubtree(tree, Pos->Left, &LeftBlackNodes);
	if (Pos->Right != &tree->Sentinel)
		CheckSubtree(tree, Pos->Right, &RightBlackNodes);
	AssertEQ(LeftBlackNodes, RightBlackNodes);
	*BlackCount= LeftBlackNodes + (RBTNode_IsBlack(Pos)? 1 : 0);
}

int compare(const void* a, const void* b) {
	int32_t aval= *(const int*)a, bval= *(const int*)b;
	if (aval<bval) return -1;
	if (aval>bval) return 1;
	return 0;
}

TEST_CASE(RBTree_TestEmptyTree) {
	RBTree tree;
	SomeObject tmp(5);
	RBTree_Init(&tree, &tmp, &tmp.Node, &tmp.Key, &compare);
	AssertEQ(-12, tree.DataOffset);
	AssertEQ(-4, tree.KeyOffset);
	AssertEQ(0, RBTree_GetCount(&tree));
	RBTree_Clear(&tree, NULL, NULL);
	AssertEQ(0, RBTree_GetCount(&tree));
	AssertEQ((RBTreeNode*)NULL, RBTree_First(&tree));
	AssertEQ((RBTreeNode*)NULL, RBTree_Last(&tree));
	CheckTree(&tree);
}

TEST_CASE(RBTree_TestInsert) {
}
