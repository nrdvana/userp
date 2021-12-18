#define USERPTINY_EOVERFLOW 1
#define USERPTINY_EOVERRUN  2

struct userptiny_scope {
	struct userptiny_scope *parent;
	uint8_t *symbols;
	uint8_t sym_base, sym_count;
	uint16_t type_base;
	uint8_t type_count, type_alloc;
	uint8_t *type_table[];
};

struct userptiny_scope* userptiny_scope_init(char *obj_buf, uint16_t size, struct userptiny_scope *parent);

struct userptiny_enc {
	struct userptiny_scope *scope;
	uint8_t *out;
	uint16_t out_pos;
	uint16_t out_len;
	uint8_t in_bitpos,
		state_pos,
		state_alloc,
		error;
	uint8_t state_stack[];
};

struct userptiny_dec {
	struct userptiny_scope *scope;
	uint8_t *in;
	uint16_t in_pos;
	uint16_t in_len;
	uint8_t in_bitpos,
		state_pos,
		state_alloc,
		error;
	uint8_t state_stack[];
};

struct userptiny_dec* userptiny_dec_init(char *obj, uint16_t size, struct userptiny_scope *scope);
uint16_t userptiny_dec_bits(struct userptiny_dec *dec, uint8_t bits);
uint16_t userptiny_dec_vqty(struct userptiny_dec *dec);
