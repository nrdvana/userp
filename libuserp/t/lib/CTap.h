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

bool TAP_ok(TAP_state_t *state, const char *name, bool pass, const char *expr);
bool TAP_note(TAP_state_t *state, const char *fmt, ...);

#define SUBTEST(x) void test_##x (TAP_state_t *state)
#define TAP_OK(name, expression) TAP_ok(state, name, expression, #expression )
#define TAP_NOTE(fmt...) TAP_note(state, fmt)
#define TAP_DIAG(fmt...) TAP_diag(state, fmt)
