#include <sys/types.h>
#include <unistd.h>


int main() {
	int x= 3;
	while (x && fork()==0) x--;
	int r;
	while (1) {
		waitpid(-1, &r, 0);
	}
	return 0;
}
