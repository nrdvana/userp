#define UNIT_TEST_CONCAT_NAME_(prefix,name) prefix##name
#define UNIT_TEST(name) void UNIT_TEST_CONCAT_NAME_(test_,name)(int argc, char **argv)
#define TRACE(env, x...) do{ if(env->log_trace){ userp_diag_set(env->diag,x); userp_env_emit_diag(env); } }while(0)

#include "config.h"
#include "t/01-bits.c"

typedef void test_entrypoint(int argc, char **argv);
struct tests {
	const char *name;
	test_entrypoint *entrypoint;
} tests[]= {
	{ "decode_bits", &test_decode_bits },
	{ NULL, NULL }
};

int main(int argc, char **argv) {
	int i;
	if (argc < 2) {
		fprintf(stderr, "Usage: ./test [COMMON_OPTS] TEST_NAME [TEST_ARGS]\n");
		return 2;
	}
	for (i= 0; tests[i].name != NULL; i++)
		if (strcmp(tests[i].name, argv[1]) == 0) {
			tests[i].entrypoint(argc-2, argv+2);
			return 0;
		}
	fprintf(stderr, "No such test %s\nAvailable tests are:\n", argv[1]);
	for (i= 0; tests[i].name != NULL; i++)
		fprintf(stderr, "  %s\n", tests[i].name);
	return 2;
}
