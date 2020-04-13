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

#define USERP_IS_FATAL(code) ((code>>13) == 3)
#define USERP_IS_ERROR(code) ((code>>13) == 2)
#define USERP_IS_WARN(code)  ((code>>13) == 1)
#define USERP_IS_DEBUG(code) ((code>>13) == 0)

#define USERP_EALLOC       0x6001 /* failure to alloc or realloc or free */
#define USERP_EINVAL       0x6002 /* invalid argument given to API */
#define USERP_EEOF         0x4002 /* unexpected end of input */
#define USERP_ELIMIT       0x4003 /* decoded data exceeds a constraint */
#define USERP_WLARGEMETA   0x2001 /* encoded or decoded metadata is suspiciously large */

#define USERP_WARN_EOF
/* TODO */

extern int userp_env_get_diag_code(userp_env_t *env);
extern const char *userp_diag_code_name(int code);
extern int userp_env_get_diag(userp_env_t *env, char *buf, size_t buflen);
extern int userp_env_print_diag(userp_env_t *env, FILE *fh);

#define USERP_BITALIGN_MAX 13
// number of bits needed to store an alignment number
#define USERP_BITALIGN_MAX_LOG2 4

#endif
