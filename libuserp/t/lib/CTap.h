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

bool TAP_ok(TAP_state_t *state, bool pass, const char *expr, const char *name, ...);
bool TAP_diag(TAP_state_t *state, const char *fmt, ...);
bool TAP_note(TAP_state_t *state, const char *fmt, ...);
void TAP_reseed(TAP_state_t *state);

#define SUBTEST(x) void test_##x (TAP_state_t *state)
#define TAP_OK(expression, name...) TAP_ok(state, expression, #expression, name)
#define TAP_NOTE(fmt...) TAP_note(state, fmt)
#define TAP_DIAG(fmt...) TAP_diag(state, fmt)
#define TAP_RESEED() TAP_reseed(state)