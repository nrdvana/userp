#include <stdbool.h>

typedef struct TAP_state {
	int test_id;
	int indent;
	int pass;
} TAP_state_t;

typedef struct TAP_subtest {
	void(*fn)(TAP_state_t*);
	const char *name;
	int plan_count;
} TAP_subtest_t;

extern int TAP_subtest_count;
extern TAP_subtest_t TAP_subtests[];

void TAP_ok(TAP_state_t *state, const char *name, bool pass, const char *expr);

#define SUBTEST(x) void x (TAP_state_t *state)
#define TAP_OK(name, expression) TAP_ok(state, name, expression, #expression )
