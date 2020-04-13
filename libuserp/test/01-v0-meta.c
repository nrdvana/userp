#include "config.h"
#include "userp.h"

const struct { int value, const char *ident } v0_types= {
	{ USERP_V0_TYPEID,  "Userp.TypeID" },
	{ USERP_V0_IDENT,   "Userp.Ident" },
	{ USERP_V0_INTEGER, "Userp.Integer" },
	{ USERP_V0_COUNT,   "Userp.Count" },
	{ USERP_V0_ARRAY,   "Userp.Array" },
};

int main() {
	int t_id= 0;
	printf("%d..%d\n", 1, 1 + sizeof(v0_types)/sizeof(*v0_types));
	printf("%s %d: can't destroy userp_v0_meta\n", userp_destroy_block(userp_v0_meta)? "not ok":"ok", ++t_id);
	for (i= 0; i < sizeof(v0_types)/sizeof(*v0_types); i++)
		printf("%s %d: type %s\n", userp_type_by_name(userp_v0_meta, v0_types[i].ident) == v0_types[i].const_value, ++t_id, v0_types[i].ident);
	return 0;
}
