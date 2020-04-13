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

bool TAP_strcmp(TAP_state_t *state, const char *actual, const char *expected, const char *fmt, ...) {
	va_list args;
	int cmp= strcmp(actual, expected);
	va_start(args, fmt);
	printf("%*s%sok %d - ", state->indent, "", cmp == 0? "" : "not ", ++state->test_id);
	vprintf(fmt, args);
	printf("\n");
	va_end(args);
	if (cmp) {
		state->pass= false;
		printf("%*s# expected: %s\n", state->indent, "", expected);
		printf("%*s# but got : %s\n", state->indent, "", actual);
	}
	return cmp == 0;
}

bool die(const char *msg) {
	perror(msg);
	abort();
}

bool capture_child(
	int *wstat_out,
	const char *stdin_buf, size_t stdin_len,
	char *stdout_buf, size_t *stdout_len,
	char *stderr_buf, size_t *stderr_len
) {
	int in_pipe[2], out_pipe[2], err_pipe[2], devnull, got, wstat;
	size_t n;
	pid_t child_pid= -1;
	if ((devnull= open("/dev/null", O_RDWR)) < 0)
		die("open(/dev/null)");
	
	if (stdin_buf && stdin_len) {
		if (0 != pipe(in_pipe)) die("pipe");
	} else {
		in_pipe[0]= in_pipe[1]= devnull;
	}
	if (stdout_buf && stdout_len && *stdout_len) {
		if (0 != pipe(out_pipe)) die("pipe");
	} else {
		out_pipe[0]= out_pipe[1]= devnull;
	}
	if (stderr_buf && stderr_len && *stderr_len) {
		if (0 != pipe(err_pipe)) die("pipe");
	} else {
		err_pipe[0]= err_pipe[1]= devnull;
	}
	fflush(stdout);
	fflush(stderr);
	if ((child_pid= fork()) < 0) die("fork");
	if (!child_pid) {
		// child
		dup2(in_pipe[0], 0); close(in_pipe[1]);
		dup2(out_pipe[1], 1); close(out_pipe[0]);
		dup2(err_pipe[1], 2); close(err_pipe[0]);
		// return "false" to parent, who will execute child's code
		return false;
	}
	else {
		// This should be a select() loop, but that's a lot of hassle for test cases that probably
		// don't write much.  Just do it the sloppy potentially blocking way.
		if (in_pipe[1] != devnull) {
			if (write(in_pipe[1], stdin_buf, stdin_len) < stdin_len)
				die("write(child stdin)");
			close(in_pipe[0]); // closed after writing to avoid potential EPIPE
		}
		if (out_pipe[0] != devnull) {
			close(out_pipe[1]);
			for (n= 0; n < *stdout_len; n+= got) {
				got= read(out_pipe[0], stdout_buf+n, *stdout_len-n);
				if (got <= 0) {
					if (got < 0) perror("read(child stdout)");
					break;
				}
			}
			stdout_buf[ n < *stdout_len ? n : *stdout_len-1 ]= '\0';
			*stdout_len= n;
		}
		if (err_pipe[0] != devnull) {
			close(err_pipe[1]);
			for (n= 0; n < *stderr_len; n+= got) {
				got= read(err_pipe[0], stderr_buf+n, *stderr_len-n);
				if (got <= 0) {
					if (got < 0) perror("read(child stdout)");
					break;
				}
			}
			stderr_buf[ n < *stderr_len ? n : *stderr_len-1 ]= '\0';
			*stderr_len= n;
		}
		if (waitpid(child_pid, &wstat, 0) < 0) die("waitpid");
		if (wstat_out) *wstat_out= wstat;
		close(out_pipe[0]);
		close(err_pipe[0]);
		return true;
	}
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
