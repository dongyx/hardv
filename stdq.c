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

static int
pindent(char *s)
{
	char *sp;

	for (sp = s; *sp ; sp++) {
		if (sp == s || sp[-1] == '\n')
			putchar('\t');
		putchar(*sp);
	}
	return sp - s;
}

int
main(int argc, char **argv)
{
	char *act;

	progname = argv[0];
	setvbuf(stdin, NULL, _IONBF, 0);
	if (strcmp(getenv("HARDV_FIRST"), "1"))
		putchar('\n');
	puts("Q:\n");
	pindent(getenv("HARDV_Q"));
	puts("\n");
	do {
		puts("Press <ENTER> to check the answer.");
		fflush(stdout);
		act = getact();
	} while (strcmp(act, ""));
	puts("A:\n");
	pindent(getenv("HARDV_A"));
	puts("\n");
	do {
		puts("Do you recall? (y/n/s)");
		fflush(stdout);
		act = getact();
	} while (strcmp(act, "y")&&strcmp(act, "n")&&strcmp(act, "s"));
	switch (*act) {
	case 'y': return 0;
	case 'n': return 1;
	}
	return 2;
}
