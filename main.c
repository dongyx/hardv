#include <stdlib.h>
#include <time.h>
#include "hardv.h"

char *progname = "hardv";
struct option opt;
time_t now;

int
main(int argc, char **argv)
{
	if (getenv("HARDV_NOW"))
		now = elapsecs(getenv("HARDV_NOW"));
	else
		time(&now);
	argv++;
	while (*argv)
		ctabload(*argv++);
	learn();
	return 0;
}
