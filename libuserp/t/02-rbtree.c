#include "config_plus.h"
#include "rbtree.h"
#include <stdlib.h>
#include <string.h>

int compare_int32(const int32_t* a, const int32_t* b) {
	return *a - *b;
}
int compare_charstar(const char **a, const char **b) {
	return strcmp(*a, *b);
}

typedef struct userp_ident {
	char *name;
	userp_rbnode_t name_tree_node;
} userp_ident_t;

#include "ident_rb.h"

bool dump_node(TAP_state_t *state, userp_ident_t *root, int lev) {
	userp_ident_t *next= userp_ident_by_name_right(root);
	if (next) dump_node(state, next, lev+1);
	TAP_NOTE("%*s(%c) %s\n", lev*4, "", root->name_tree_node.color? 'R':'B', root->name);
	next= userp_ident_by_name_left(root);
	if (next) dump_node(state, next, lev+1);
	return true;
}
bool dump_tree(TAP_state_t *state, userp_ident_by_name_t *tree) {
	dump_node(state, userp_ident_by_name_root(tree), 0);
}

SUBTEST(EmptyTree) {
	userp_ident_by_name_t tree;
	userp_ident_by_name_init(&tree);
	TAP_OK("tree is valid", 0 == userp_ident_by_name_check(&tree));
}

#define NODE_COUNT 1000
SUBTEST(AddNodesRight, NODE_COUNT*2) {
	int i;
	char buffer[20];
	userp_ident_t *idents= calloc(NODE_COUNT, sizeof(userp_ident_t));
	userp_ident_by_name_t tree;
	
	userp_ident_by_name_init(&tree);
	for (i=0; i < NODE_COUNT; i++) {
		snprintf(buffer, sizeof(buffer), "%d", i);
		idents[i].name= strdup(buffer);
		TAP_OK("insert", userp_ident_by_name_insert(&tree, idents+i));
		TAP_OK("tree is valid", 0 == userp_ident_by_name_check(&tree))
			|| dump_tree(state, &tree);
	}
	for (i= 0; i < NODE_COUNT; i++)
		free(idents[i].name);
	free(idents);
}

SUBTEST(AddNodesLeft, NODE_COUNT) {
	int i;
	char buffer[20];
	userp_ident_t *idents= calloc(NODE_COUNT, sizeof(userp_ident_t));
	userp_ident_by_name_t tree;
	
	userp_ident_by_name_init(&tree);
	for (i=0; i < NODE_COUNT; i++) {
		snprintf(buffer, sizeof(buffer), "%d", NODE_COUNT-i);
		idents[i].name= strdup(buffer);
		userp_ident_by_name_insert(&tree, idents+i);
		TAP_OK("tree is valid", 0 == userp_ident_by_name_check(&tree));
	}
	for (i= 0; i < NODE_COUNT; i++)
		free(idents[i].name);
	free(idents);
}

SUBTEST(AddNodesMiddle, NODE_COUNT) {
	int i;
	char buffer[20];
	userp_ident_t *idents= calloc(NODE_COUNT, sizeof(userp_ident_t));
	userp_ident_by_name_t tree;
	
	userp_ident_by_name_init(&tree);
	for (i=0; i < NODE_COUNT; i++) {
		snprintf(buffer, sizeof(buffer), "%d", i&1? NODE_COUNT*2-i : i);
		idents[i].name= strdup(buffer);
		userp_ident_by_name_insert(&tree, idents+i);
		TAP_OK("tree is valid", 0 == userp_ident_by_name_check(&tree));
	}
	for (i= 0; i < NODE_COUNT; i++)
		free(idents[i].name);
	free(idents);
}

SUBTEST(AddNodesRandom, NODE_COUNT*2) {
	int i;
	char buffer[20];
	userp_ident_t *idents= calloc(NODE_COUNT, sizeof(userp_ident_t));
	userp_ident_by_name_t tree;
	int r= rand();
	printf("# srand = %d\n", r);
	srand(r);
	
	userp_ident_by_name_init(&tree);
	for (i=0; i < NODE_COUNT; i++) {
		snprintf(buffer, sizeof(buffer), "%d", rand());
		idents[i].name= strdup(buffer);
		TAP_OK("insert", userp_ident_by_name_insert(&tree, idents+i));
		TAP_OK("tree is valid", 0 == userp_ident_by_name_check(&tree));
	}
	for (i= 0; i < NODE_COUNT; i++)
		free(idents[i].name);
	free(idents);
}

SUBTEST(NodeIndex, NODE_COUNT*2+NODE_COUNT-1+5) {
	int i;
	char buffer[20];
	userp_ident_t *idents= calloc(NODE_COUNT, sizeof(userp_ident_t));
	userp_ident_t *pos, *prev;
	userp_ident_by_name_t tree;
	int r= rand();
	printf("# srand = %d\n", r);
	srand(r);
	
	userp_ident_by_name_init(&tree);
	for (i=0; i < NODE_COUNT; i++) {
		snprintf(buffer, sizeof(buffer), "%d", rand());
		idents[i].name= strdup(buffer);
		userp_ident_by_name_insert(&tree, idents+i);
	}
	TAP_OK("tree is valid", 0 == userp_ident_by_name_check(&tree));
	
	TAP_OK("has first", (pos= userp_ident_by_name_first(&tree)));
	TAP_OK("has next", userp_ident_by_name_next(pos));
	TAP_OK("no prev", !userp_ident_by_name_prev(pos));
	for (i=0, prev=NULL; pos; i++) {
		TAP_OK("elem_index", userp_ident_by_name_elem_index(pos) == i);
		TAP_OK("elem_at", userp_ident_by_name_elem_at(&tree, i) == pos);
		if (prev)
			TAP_OK("sort correct", strcmp(prev->name, pos->name) <= 0)
				|| dump_tree(state, &tree);
		prev= pos;
		pos= userp_ident_by_name_next(pos);
	}
	TAP_OK("correct count", i == NODE_COUNT);
	
	for (i= 0; i < NODE_COUNT; i++)
		free(idents[i].name);
	free(idents);
}

