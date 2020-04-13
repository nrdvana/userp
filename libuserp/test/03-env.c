#include "../src/userp_env.c"

SUBTEST(builtin_alloc) {
	char *obj= NULL;
	userp_env_t *env= userp_env_new(NULL, NULL, NULL);
	TAP_OK( env, "constructor returned a valid pointer" );
	//TAP_OK( env->alloc((void**) &obj, 
}
