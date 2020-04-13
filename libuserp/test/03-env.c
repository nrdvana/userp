#include "../src/userp_env.c"

SUBTEST(builtin_alloc,?) {
	char *obj= NULL;
	userp_env_t *env= userp_env_new(NULL, NULL, NULL);
	TAP_OK( env, "constructor returned a valid pointer" );
	TAP_OK( env->alloc(NULL, (void**) &obj, 32, 0), "allocated 32 bytes" );
	obj[31]= '\x42';
	TAP_OK( env->alloc(NULL, (void**) &obj, 50, 0), "grow to 50 bytes" );
	TAP_OK( obj[31] == '\x42', "character from previous was preserved" );
	TAP_OK( env->alloc(NULL, (void**) &obj, 0, 0), "shrink to 0 bytes" );
	TAP_OK( obj == NULL, "pointer is NULL again" );
	TAP_OK( userp_env_free(env), "env freed" );
}

SUBTEST(builtin_log,?) {
	userp_env_t *env= userp_env_new(NULL, NULL, NULL);
	char err_buf[1024];
	size_t n= sizeof(err_buf);
	int wstat= 0;
	
	if (!capture_child(&wstat, NULL, 0, NULL, 0, err_buf, &n)) {
		env->diag_code= USERP_EALLOC;
		env->diag_tpl= "Testing";
		env->diag(NULL, env->diag_code, env);
		exit(2); // should not make it here
	}
	TAP_OK( WIFSIGNALED(wstat), "diag fatal error died on signal" );
	TAP_IS( err_buf, "error: Testing\n", "saw expected stderr" );
	
	n= sizeof(err_buf);
	if (!capture_child(&wstat, NULL, 0, NULL, 0, err_buf, &n)) {
		env->diag_code= USERP_WLARGEMETA;
		env->diag_tpl= "Testing warning";
		env->diag(NULL, env->diag_code, env);
		exit(0);
	}
	TAP_OK( WIFEXITED(wstat) && WEXITSTATUS(wstat) == 0, "exited cleanly" );
	TAP_IS( err_buf, "warning: Testing warning\n", "saw expected stderr" );
	
	TAP_OK( userp_env_free(env), "env freed" );
}
