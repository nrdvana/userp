#include "../config.h"
#include "../userptiny.h"

UNIT_TEST(decode_bits) {
	uint8_t bytes[]= { 0x9F, 0x01, 0x03, 0x03, 0x80, 0x1F, 0x00, 0xF8 };
	uint16_t remaining= sizeof(bytes) * 8;
	uint8_t decodes[]= {
		0, 1, 0, 1, 2, // reads less than remain % 8
		4,             // read == remain % 8
		9, 8,          // read > 1 byte, then read a whole un-aligned byte
		15,            // read byte + remainder that lands on boundary
		4, 16, 4       // read 16 bits across 3 bytes
	};
	for (int i= 0; i < sizeof(decodes); i++) {
		uint16_t out;
		userptiny_error_t err= userptiny_decode_bits(&out, bytes+sizeof(bytes), &remaining, decodes[i]);
		if (!err)
			printf("read %d = %X\n", (int)decodes[i], (unsigned)out);
		else
			printf("read %d : %s\n", (int)decodes[i], userptiny_error_text(err));
	}
}
/*OUTPUT
read 0 = 0
read 1 = 1
read 0 = 0
read 1 = 1
read 2 = 3
read 4 = 9
read 9 = 101
read 8 = 81
read 15 = 4001
read 4 = F
read 16 = 8001
read 4 = F
*/

UNIT_TEST(decode_vqty) {
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
		err= userptiny_decode_vqty(&out, bytes+sizeof(bytes), &remaining);
		if (!err)
			printf("read = %X\n", (unsigned)out);
		else
			printf("read : %s\n", userptiny_error_text(err));
	}
	printf("remaining = %u\n", (unsigned)remaining);
	
	remaining= sizeof(bytes) * 8;
	err= userptiny_skip_vqty(5, bytes+sizeof(bytes), &remaining);
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
