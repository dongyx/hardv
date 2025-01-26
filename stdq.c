#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include "hardv.h"

char *progname = "stdq";

static char *
getact(void)
{
	static char buf[128];

	while (!fgets(buf, sizeof buf, stdin)) {
		if (feof(stdin)) exit(2);
		if (errno != EINTR) syserr();
	}
	if (buf[strlen(buf)-1] != '\n')
		err("line too long or not terminated");
	buf[strlen(buf)-1] = '\0';
	return buf;
}

int
main(int argc, char **argv)
{
	char *act;

	progname = argv[0];
	putchar('Q');
	fputs(getenv("HARDV_F_Q"), stdout);
	do {
		puts("press <ENTER> to check the answer");
		fflush(stdout);
		act = getact();
	} while (strcmp(act, ""));
	putchar('A');
	fputs(getenv("HARDV_F_A"), stdout);
	do {
		puts("recall? ([y]es/[n]o/[s]kip)");
		fflush(stdout);
		act = getact();
	} while (strcmp(act, "y")&&strcmp(act, "n")&&strcmp(act, "s"));
	switch (*act) {
	case 'y': return 0;
	case 'n': return 1;
	case 's': return 2;
	}
	return 2;
}
