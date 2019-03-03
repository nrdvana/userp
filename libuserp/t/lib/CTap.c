#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <time.h>

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
	printf("%*s%sok %d - %s\n", parent_state->indent, "", state.pass? "" : "not ", ++parent_state->test_id, test->name);
	if (!state.pass) parent_state->pass= 0;
}

bool TAP_ok(TAP_state_t *state, bool pass, const char *expr, const char *name, ...) {
	va_list args;
	va_start(args, name);
	printf("%*s%sok %d - ", state->indent, "", pass? "" : "not ", ++state->test_id);
	vprintf(name, args);
	printf("\n");
	va_end(args);
	if (!pass) {
		state->pass= false;
		printf("%*s# failed expression: %s\n", state->indent, "", expr);
	}
	return pass;
}

bool TAP_note(TAP_state_t *state, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printf("%*s# ", state->indent, "");
	vprintf(fmt, args);
	printf("\n");
	va_end(args);
	return true;
}

bool TAP_diag(TAP_state_t *state, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "%*s# ", state->indent, "");
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
	return true;
}

static int TAP_seed= 0;
void TAP_reseed(TAP_state_t *state) {
	char *srand_val;
	if (!TAP_seed) {
		if ((srand_val= getenv("srand")))
			TAP_seed= atoi(srand_val);
		else
			TAP_seed= (unsigned) time(NULL);

		if (!TAP_seed) ++TAP_seed;
	}
	TAP_note(state, "srand = %d", TAP_seed);
	srand((unsigned) TAP_seed);
}

int TAP_main(int argc, char **argv) {
	int i, t;
	TAP_state_t state= { .test_id=0, .indent=0, .pass=1 };
	//printf("TAP version 13\n");
	TAP_reseed(&state);
	printf("%d..%d\n", 1, argc > 1? argc-1 : TAP_subtest_count);
	if (argc > 1) {
		for (i= 1; i < argc; i++) {
			for (t= 0; t < TAP_subtest_count; t++)
				if (0 == strcmp(argv[i], TAP_subtests[t].name))
					break;
			if (t == TAP_subtest_count) {
				printf("# Subtest: %s\n", argv[i]);
				printf("not ok %d - no such test %s\n", i, argv[i]);
			}
			else
				TAP_run_subtest(TAP_subtests+t, &state);
		}
	}
	else {
		for (t= 0; t < TAP_subtest_count; t++)
			TAP_run_subtest(TAP_subtests+t, &state);
	}
	return 0;
}