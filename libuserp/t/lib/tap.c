#include <stdio.h>
#include <assert.h>

typedef struct TAP_state_s {
	int test_id;
	int indent;
	int pass;
} TAP_state_t;

typedef struct TAP_subtest_s {
	void(*fn)(TAP_state_t*);
	const char *name;
	int plan_count;
} TAP_subtest_t;

int TAP_subtest_count= 0;
TAP_subtest_t *TAP_tests= NULL;

int TAP_register_subtest( void(*fn)(TAP_state_t*), const char *test_name, int plan_count ) {
	assert(TAP_tests= (TAP_subtest_t*) realloc(TAP_tests, sizeof(TAP_subtest_t)*(TAP_subtest_count+1)));
	TAP_tests[TAP_subtest_count].fn= fn;
	TAP_tests[TAP_subtest_count].name= test_name;
	TAP_tests[TAP_subtest_count].plan_count= plan_count;
	return TAP_subtest_count++;
}

void TAP_run_subtest(TAP_subtest_t *test, TAP_state_t *parent_state) {
	TAP_state_t state= { .test_id=0, .indent= parent_state->indent+4, .pass=1 };
	printf("%*s# Subtest: %s\n", parent_state->indent, "", test->name);
	if (test->plan_count > 0)
		printf("%*s%d..%d\n", state.indent, "", 1, test->plan_count);
	int TAP_test_idx= 0;
	test->fn(&state);
	if (test->plan_count > 0) {
		if (state.test_id != test->plan_count) state.pass= 0;
	}
	else {
		printf("%*s%d..%d\n", state.indent, "", 1, state.test_id);
	}
	printf("%*sok %d - %s\n", parent_state->indent, state.pass? "" : "not ", ++parent_state->test_id, test->name);
	if (!state.pass) parent_state->pass= 0;
}

int TAP_main(int argc, char **argv) {
	int i, t;
	TAP_state_t state= { .test_id=0, .indent=0, .pass=1 };
	printf("TAP version 13\n");
	printf("%d..%d\n", 1, argc > 1? argc-1 : TAP_subtest_count);
	if (argc > 1) {
		for (i= 1; i < argc; i++) {
			for (t= 0; t < TAP_subtest_count; t++)
				if (0 == strcmp(argv[i], TAP_tests[t].name))
					break;
			if (t == TAP_subtest_count) {
				printf("# Subtest: %s\n", argv[i]);
				printf("not ok %d - no such test %s\n", i, argv[i]);
			}
			else
				TAP_run_subtest(TAP_tests+t, &state);
		}
	}
	else {
		for (t= 0; t < TAP_subtest_count; t++)
			TAP_run_subtest(TAP_tests+t, &state);
	}
}

#define TAP_MAIN \
	int main(int argc, char **argv) { return TAP_main(argc, argv); }

#define SUBTEST_PLAN(x, plan) \
	void test_##x(TAP_state_t *TAP_state); \
	int test_decl_##x = TAP_register_subtest( &test_##x , #x , plan ); \
	void test_##x(TAP_state_t *TAP_state)

#define SUBTEST(x) SUBTEST_PLAN(x, -1)
