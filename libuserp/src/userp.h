#ifndef USERP_H
#define USERP_H

typedef struct userp_env userp_env_t, *userp_env_p;
typedef struct userp_context userp_t, *userp_p;
typedef struct userp_scope userp_scope_t, *userp_scope_p;
typedef struct userp_type userp_type_t, *userp_type_p;
typedef struct userp_enc userp_enc_t, *userp_enc_p;
typedef struct userp_dec userp_dec_t, *userp_dec_p;

typedef bool userp_alloc_fn(void *callback_data, void **pointer, size_t new_size, int flags);
typedef void userp_diag_fn(void *callback_data, int diag_code, userp_env_t *env);
extern userp_env_p userp_env_new(userp_alloc_fn alloc_callback, userp_diag_fn diag_callback, void *callback_data);

#define USERP_IS_ERR(code)  (!(code>>14))
#define USERP_IS_WARN(code) ((code>>13) == 2)
#define USERP_IS_DEBUG(code) ((code>>13) == 3)

#define USERP_ERR_MAJOR(code) (code & 0x3F00)
#define USERP_ERR_OS          0x0100
#define USERP_ERR_ALLOC       0x0101
#define USERP_ERR_EOF         0x0200
#define USERP_ERR_INVAL       0x0300
/* TODO */

extern int userp_env_get_diag_code(userp_env_t *env);
extern const char *userp_diag_code_name(int code);
extern int userp_env_get_diag(userp_env_t *env, char *buf, size_t buflen);
extern int userp_env_print_diag(userp_env_t *env, FILE *fh);

#define USERP_BITALIGN_MAX 13
// number of bits needed to store an alignment number
#define USERP_BITALIGN_MAX_LOG2 4

#endif
