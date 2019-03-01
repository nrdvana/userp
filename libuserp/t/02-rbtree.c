#include "config_plus.h"
#include "rbtree.h"

int compare_int32(const int32_t* a, const int32_t* b) {
	return *a - *b;
}

typedef struct userp_ident {
	const char *name;
	userp_rbnode_t name_tree_node;
} userp_ident_t;

#include "ident_rb.h"

SUBTEST(EmptyTree) {
	userp_ident_by_name_t tree;
	userp_ident_by_name_init(&tree);
	TAP_OK("tree is valid", 0 == userp_ident_by_name_check(&tree) );
}

/*
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
*/