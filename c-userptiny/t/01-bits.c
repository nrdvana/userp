#include "../config.h"
#include "../userptiny.h"

UNIT_TEST(decode_bits) {
	uint8_t bytes[]= { 0x9F, 0x01, 0x03, 0x03, 0x80, 0x1F, 0x00, 0xF8 };
	uint8_t decodes[]= {
		1, 1, 2,       // reads less than remain % 8
		4,             // read == remain % 8
		9, 8,          // read > 1 byte, then read a whole un-aligned byte
		15,            // read byte + remainder that lands on boundary
		4, 16, 4       // read 16 bits across 3 bytes
	};
	uint16_t remaining= sizeof(bytes) * 8;
	for (int i= 0; i < sizeof(decodes); i++) {
		uint16_t out;
		userptiny_error_t err= userptiny_decode_bits_u16(&out, bytes+sizeof(bytes), &remaining, decodes[i]);
		if (!err)
			printf("u16 %d = %X\n", (int)decodes[i], (unsigned)out);
		else
			printf("u16 %d : %s\n", (int)decodes[i], userptiny_error_text(err));
	}
	// Now do it again as signed integers
	remaining= sizeof(bytes) * 8;
	for (int i= 0; i < sizeof(decodes); i++) {
		int16_t out;
		userptiny_error_t err= userptiny_decode_bits_s16(&out, bytes+sizeof(bytes), &remaining, decodes[i]);
		if (!err)
			printf("s16 %d = %d\n", (int)decodes[i], (int)out);
		else
			printf("s16 %d : %s\n", (int)decodes[i], userptiny_error_text(err));
	}
}
/*OUTPUT
u16 1 = 1
u16 1 = 1
u16 2 = 3
u16 4 = 9
u16 9 = 101
u16 8 = 81
u16 15 = 4001
u16 4 = F
u16 16 = 8001
u16 4 = F
s16 1 = -1
s16 1 = -1
s16 2 = -1
s16 4 = -7
s16 9 = -255
s16 8 = -127
s16 15 = -16383
s16 4 = -1
s16 16 = -32767
s16 4 = -1
*/

UNIT_TEST(decode_vint) {
	uint8_t bytes[]= {
		0x82, // one byte
		0x05, 0x80, // two bytes
		0x0B, 0x00, 0x04, 0x00, // four bytes that fits in 16 bits
		0xFB, 0xFF, 0x07, 0x00, // also fits
		0x03, 0x00, 0x08, 0x00 // does not fit
	};
	userptiny_error_t err;
	uint16_t remaining= sizeof(bytes) * 8;
	for (int i= 0; i < 5; i++) {
		uint16_t out;
		err= userptiny_decode_vint_u16(&out, bytes+sizeof(bytes), &remaining);
		if (!err)
			printf("read = %X\n", (unsigned)out);
		else
			printf("read : %s\n", userptiny_error_text(err));
	}
	printf("remaining = %u\n", (unsigned)remaining);
	
	remaining= sizeof(bytes) * 8;
	err= userptiny_skip_vint(5, bytes+sizeof(bytes), &remaining);
	printf("skip: err=%s remaining=%u\n", err? userptiny_error_text(err) : "", (unsigned) remaining);
}
/*OUTPUT
read = 41
read = 2001
read = 8001
read = FFFF
read : integer overflow
remaining = 32
skip: err= remaining=0
*/
