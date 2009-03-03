#include <stdint.h>
#include "BufferTracker.h"

int main() {
	userp_buftracker_t* bt= userp_buftracker_Create();
	uint8_t *buf= userp_buftracker_RegisterNew(bt, 120);
	userp_buftracker_AcquireRef(bt, buf);
	userp_buftracker_AcquireRef(bt, buf+30);
	userp_buftracker_ReleaseRef(bt, buf);
	userp_buftracker_ReleaseRef(bt, buf+30);
	userp_buftracker_Destroy(bt), bt= 0;
	return 0;
}
