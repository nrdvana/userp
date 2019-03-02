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
	if (!root) return false;
	userp_ident_t *next= userp_ident_by_name_right(root);
	if (next) dump_node(state, next, lev+1);
	TAP_NOTE("%*s(%c) %s", lev*4, "", root->name_tree_node.color? 'R':'B', root->name);
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
	TAP_OK(0 == userp_ident_by_name_check(&tree), "tree is valid");
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
		TAP_OK(userp_ident_by_name_insert(&tree, idents+i), "insert");
		TAP_OK(0 == userp_ident_by_name_check(&tree), "tree is valid")
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
		TAP_OK(0 == userp_ident_by_name_check(&tree), "tree is valid");
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
		TAP_OK(0 == userp_ident_by_name_check(&tree), "tree is valid");
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
	TAP_RESEED();
	
	userp_ident_by_name_init(&tree);
	for (i=0; i < NODE_COUNT; i++) {
		snprintf(buffer, sizeof(buffer), "%d", rand());
		idents[i].name= strdup(buffer);
		TAP_OK(userp_ident_by_name_insert(&tree, idents+i), "insert");
		TAP_OK(0 == userp_ident_by_name_check(&tree), "tree is valid");
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
	TAP_RESEED();
	
	userp_ident_by_name_init(&tree);
	for (i=0; i < NODE_COUNT; i++) {
		snprintf(buffer, sizeof(buffer), "%d", rand());
		idents[i].name= strdup(buffer);
		userp_ident_by_name_insert(&tree, idents+i);
	}
	TAP_OK(0 == userp_ident_by_name_check(&tree), "tree is valid");
	
	TAP_OK((pos= userp_ident_by_name_first(&tree)), "has first");
	TAP_OK(userp_ident_by_name_next(pos), "has next");
	TAP_OK(!userp_ident_by_name_prev(pos), "no prev");
	for (i=0, prev=NULL; pos; i++) {
		TAP_OK(userp_ident_by_name_elem_index(pos) == i, "elem_index");
		TAP_OK(userp_ident_by_name_elem_at(&tree, i) == pos, "elem_at");
		if (prev)
			TAP_OK(strcmp(prev->name, pos->name) <= 0, "sort correct")
				|| dump_tree(state, &tree);
		prev= pos;
		pos= userp_ident_by_name_next(pos);
	}
	TAP_OK(i == NODE_COUNT, "correct count");
	
	for (i= 0; i < NODE_COUNT; i++)
		free(idents[i].name);
	free(idents);
}

SUBTEST(Find) {
	int i, c, count;
	char buffer[32];
	userp_ident_t *idents= calloc(NODE_COUNT, sizeof(userp_ident_t));
	userp_ident_t *first, *last, *nearest, *cur, *next;
	userp_ident_by_name_t tree;
	TAP_RESEED();
	
	userp_ident_by_name_init(&tree);
	/* Insert all the nodes at random */
	for (i=0; i < NODE_COUNT; i++) {
		snprintf(buffer, sizeof(buffer), "%d", rand());
		idents[i].name= strdup(buffer);
		userp_ident_by_name_insert(&tree, idents+i);
	}
	TAP_OK(0 == userp_ident_by_name_check(&tree), "tree is valid");
	/* Then for each node, make sure it can be found */
	for (i=0; i < NODE_COUNT; i++) {
		first= last= nearest= NULL;
		count= userp_ident_by_name_find_all(&tree, idents[i].name, &first, &last);
		TAP_OK(count > 0 && first && last, "found (%d)", count);
		for (cur= first; cur && cur != last; cur= userp_ident_by_name_next(cur)) {
			if (cur == &idents[i]) break;
		}
		TAP_OK(cur == &idents[i], "first..last includes node %d (%p)", i, cur);
		/* now, if next node has value not adjacent, search for nearby value */
		next= userp_ident_by_name_next(cur);
		c= 0;
		if (next)
			for (c= 0; cur->name[c] && cur->name[c] == next->name[c]; c++);
		if (cur->name[c]) {
			/* compute "nearby value" by appending character to cur->name */
			snprintf(buffer, sizeof(buffer), "%s0", cur->name);
			/* then search for nearest node to this nearby value, which should give back 'cur' */
			count= userp_ident_by_name_find_all(&tree, buffer, &nearest, NULL);
			TAP_OK(count == 0 && nearest == cur, "nearest to %s is current node", buffer)
				|| TAP_DIAG("count=%d, nearest=%p, cur=%p, nearest->name=%s, cur->name=%s", count, nearest, cur, nearest->name, cur->name);
		}
		else {
			TAP_OK(1, "(can't compare nearest due to following node being too close)");
		}
	}
	
	for (i= 0; i < NODE_COUNT; i++)
		free(idents[i].name);
	free(idents);
}

SUBTEST(Remove, NODE_COUNT*3+1) {
	int i, rm_i;
	char buffer[20];
	userp_ident_t *idents= calloc(NODE_COUNT, sizeof(userp_ident_t));
	userp_ident_by_name_t tree;
	TAP_RESEED();
	
	userp_ident_by_name_init(&tree);
	for (i=0; i < NODE_COUNT; i++) {
		/* Add a node */
		snprintf(buffer, sizeof(buffer), "%d", rand());
		idents[i].name= strdup(buffer);
		TAP_OK(userp_ident_by_name_insert(&tree, &idents[i]), "add %s", idents[i].name);
		/* Then remove a node at pseudorandom */
		rm_i= rand() % (i+1);
		userp_ident_by_name_remove(&idents[rm_i]);
		TAP_OK(0 == userp_ident_by_name_check(&tree), "remove %s & check tree", idents[rm_i].name)
			|| dump_tree(state, &tree);
		/* Then re-add that node with a different key */
		free(idents[rm_i].name);
		snprintf(buffer, sizeof(buffer), "%d", rand());
		idents[rm_i].name= strdup(buffer);
		TAP_OK(userp_ident_by_name_insert(&tree, &idents[rm_i]), "re-add node as %s", idents[rm_i].name);
	}
	TAP_OK(0 == userp_ident_by_name_check(&tree), "tree is valid");
	
	for (i= 0; i < NODE_COUNT; i++)
		free(idents[i].name);
	free(idents);
}
