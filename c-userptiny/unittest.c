#include "config.h"
#include "userptiny.h"

int main() {
	char scope_state[sizeof(struct userptiny_scope) + 64];
	struct userptiny_scope *scope= userptiny_scope_init(scope_state, sizeof(scope_state), NULL);
	char dec_state[sizeof(struct userptiny_dec) + 64];
	struct userptiny_dec *dec= userptiny_dec_init(dec_state, sizeof(dec_state), scope);

	return 0;
}
