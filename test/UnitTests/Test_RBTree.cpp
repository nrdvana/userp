struct SomeObject {
	char Unused[8];
	int32_t Key;
	RBTreeNode Node;
	
	SomeObject() { Key= 0; RBTNode_Init(&Node); }
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
	AssertEQ(&tree->Sentinel, tree->RootSentinel.Right);
	int unused;
	if (tree->RootSentinel.Left != &tree->Sentinel)
		CheckSubtree(tree, tree->RootSentinel.Left, &unused);
}

void CheckSubtree(RBTree* tree, RBTreeNode *Pos, int *BlackCount) {
	AssertLT(0, RBTNode_GetCount(Pos));
	AssertEQ(RBTNode_GetCount(Pos->Left) + RBTNode_GetCount(Pos->Right) + 1, RBTNode_GetCount(Pos));
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

TEST_CASE(RBTree_TestInsert_Increasing) {
	// insert in increasing order
	RBTree tree;
	const int COUNT=10000;
	SomeObject objs[COUNT];
	for (int i=0; i<COUNT; i++)
		objs[i].Key= i;
	RBTree_Init(&tree, &objs[0], &objs[0].Node, &objs[0].Key, &compare);
	for (int i=0; i<COUNT; i++) {
		RBTree_Insert(&tree, &objs[i]);
		AssertEQ(i+1, RBTree_GetCount(&tree));
		AssertEQ(&objs[0].Node, RBTree_First(&tree));
		AssertEQ(&objs[i].Node, RBTree_Last(&tree));
	}
	CheckTree(&tree);
}

TEST_CASE(RBTree_TestInsert_Decreasing) {
	// insert in decreasing order
	RBTree tree;
	const int COUNT=10000;
	SomeObject objs[COUNT];
	for (int i=0; i<COUNT; i++)
		objs[i].Key= i;
	RBTree_Init(&tree, &objs[0], &objs[0].Node, &objs[0].Key, &compare);
	for (int i=COUNT-1; i>=0; i--) {
		RBTree_Insert(&tree, &objs[i]);
		AssertEQ(COUNT-i, RBTree_GetCount(&tree));
		AssertEQ(&objs[i].Node, RBTree_First(&tree));
		AssertEQ(&objs[COUNT-1].Node, RBTree_Last(&tree));
	}
	CheckTree(&tree);
}

TEST_CASE(RBTree_TestInsert_Random) {
	// insert in random order
	RBTree tree;
	const int COUNT=10000;
	SomeObject objs[COUNT];
	int RemainingIdx[COUNT];
	for (int i=0; i<COUNT; i++) {
		objs[i].Key= i;
		RemainingIdx[i]= i;
	}
	SomeObject *min= NULL, *max= NULL;
	RBTree_Init(&tree, &objs[0], &objs[0].Node, &objs[0].Key, &compare);
	for (int remaining=COUNT; remaining>0; remaining--) {
		int idx= random()%remaining;
		SomeObject *cur= &objs[RemainingIdx[idx]];
		RemainingIdx[idx]= RemainingIdx[remaining-1];
		if (!min || min->Key > cur->Key) min= cur;
		if (!max || max->Key < cur->Key) max= cur;
		RBTree_Insert(&tree, cur);
		AssertEQ(COUNT+1-remaining, RBTree_GetCount(&tree));
		AssertEQ(&min->Node, RBTree_First(&tree));
		AssertEQ(&max->Node, RBTree_Last(&tree));
	}
	CheckTree(&tree);
}
