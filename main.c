#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include "hardv.h"

char *progname = "hardv";
struct option opt;
time_t now;

static void
help(void)
{
	puts("Usage:");
	printf("\t%s [options] FILE...\n", progname);
	printf("\t%s --help|--version\n", progname);
	puts("Options:");
	puts("	-e	enable exact quiz time");
	puts("	-r	randomize the quiz order");
	puts("	-n N	quiz at most N cards");
	puts("	--help	print the brief help");
	puts("	--version");
	puts("		print the version information");
	exit(0);
}

static void
version(void)
{
	puts("HardV 5.0.0-alpha.2 <https://github.com/dongyx/hardv>");
	puts("Copyright (c) 2022 DONG Yuxuan <https://www.dyx.name>");
	exit(0);
}

int
main(int argc, char **argv)
{
	int ch;

	if (argc > 0) progname = argv[0];
	if (getenv("HARDV_NOW"))
		now = elapsecs(getenv("HARDV_NOW"));
	else
		now = time(NULL);
	srand(time(NULL));
	if (argc>1 && !strcmp(argv[1], "--help")) help();
	if (argc>1 && !strcmp(argv[1], "--version")) version();
	while ((ch=getopt(argc,argv,"ern:")) != -1)
		switch (ch) {
		case 'e':
			opt.exact = 1;
			break;
		case 'r':
			opt.rand = 1;
			break;
		case 'n':
			opt.maxn = atoi(optarg);
			break;
		default:
			exit(-1);
		}
	argv+=optind, argc-=optind;
	if (!argc) err("file operand expected");
	while (*argv) ctabload(*argv++);
	learn();
	return 0;
}
