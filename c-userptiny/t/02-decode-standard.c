#include "../config.h"
#include "../userptiny.h"

UNIT_TEST(decode_integer) {
	uint8_t bytes[]= { 0x00 };
	struct userptiny_dec dec;
	uint8_t state[64];
	userptiny_error_t err;

	if ((err= userptiny_dec_init(&dec, &userptiny_v1_scope, state, sizeof(state)))) {
		printf("userptiny_dec_init : %s\n", userptiny_error_text(err));
		return;
	}
	if ((err= userptiny_dec_set_input(&dec, USERPTINY_V1_TYPE_INTEGER, bytes, sizeof(bytes)))) {
		printf("userptiny_dec_set_input : %s\n", userptiny_error_text(err));
		return;
	}
	uint16_t out;
	while (!(err= userptiny_dec_int(&dec, &out)))
		printf("int = %X\n", (unsigned)out);
	printf("int : %s\n", userptiny_error_text(err));
}
/*OUTPUT
int = 0
int : invalid api call
*/
