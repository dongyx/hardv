#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include "apperr.h"
#include "applim.h"
#include "learn.h"
#include "ctab.h"

static void help(FILE *fp);
static void pversion(FILE *fp);
static void report(char *fname);
static int safechk(char *fname);

int main(int argc, char **argv)
{
	struct learnopt opt;
	char *envnow;
	time_t now;
	int ch;

	srand(time(NULL));
	if ((envnow = getenv("HARDV_NOW"))) {
		if (parsetm(envnow, &now) == -1) {
			aeprint(envnow);
			return 1;
		}
	} else
		now = time(NULL);
	memset(&opt, 0, sizeof opt);
	opt.maxn = -1;
	while ((ch = getopt(argc, argv, "hvern:")) != -1)
		switch (ch) {
		case 'e':
			opt.exact = 1;
			break;
		case 'r':
			opt.rand = 1;
			break;
		case 'n':
			if ((opt.maxn = atoi(optarg)) <= 0) {
				help(stderr);
				return 1;
			}
			break;
		case 'h':
			help(stdout);
			return 0;
		case 'v':
			pversion(stdout);
			return 0;
		case '?':
		default:
			help(stderr);
			return 1;
		}
	argv += optind;
	argc -= optind;
	if (argc <= 0) {
		help(stderr);
		return 1;
	}
	while (*argv) {
		if (safechk(*argv) == -1 ||
			learn(*argv, now, &opt) == -1) {
			report(*argv);
			return 1;
		}
		argv++;
	}
	return 0;
}

static void help(FILE *fp)
{
	fputs("usage:\n", fp);
	fputs("\thardv [options] file ...\n", fp);
	fputs("\thardv -h|-v\n", fp);
	fputs("\n", fp);
	fputs("options\n", fp);
	fputs("\n", fp);
	fputs("-e	enable exact quiz time\n", fp);
	fputs("-r	randomize the quiz order within a file\n", fp);
	fputs("-n <n>	quiz at most <n> cards\n", fp);
	fputs("-h	print this help information\n", fp);
	fputs("-v	print version and building arguments\n", fp);
}

static void pversion(FILE *fp)
{
	fprintf(fp, "hardv %s\n", VERSION);
	fprintf(fp, "%s\n", COPYRT);
	fputc('\n', fp);
	fprintf(fp, "NLINE:	%d\n", NLINE);
	fprintf(fp, "LINESZ:	%d\n", LINESZ);
	fprintf(fp, "NCARD:	%d\n", NCARD);
	fprintf(fp, "NFIELD:	%d\n", NFIELD);
	fprintf(fp, "KEYSZ:	%d\n", KEYSZ);
	fprintf(fp, "VALSZ:	%d\n", VALSZ);
	fprintf(fp, "PATHSZ:	%d\n", PATHSZ);
}

static void report(char *fname)
{
	if (apperr == AEEXBF) {
		fprintf(stderr, "%s: %s\n",
			aestr(apperr), getbname(fname));
		fputs("\nThis may be caused by a previous crash "
			"or simultaneous quiz/editing.\n"
			"You should check the backup file "
			"and recover the data.\n", stderr);
		return;
	}
	if (apperr == AEBACKUP) {
		aeprint(fname);
		fprintf(stderr, "%s: %s\n", getbname(fname),
			aestr(bakferr));
		return;
	}
	if (lineno > 0)
		fprintf(stderr, "%s, line %d: %s\n", fname, lineno,
			aestr(apperr));
	else
		aeprint(fname);
	if (bakfname)
		fprintf(stderr,"\nAn error occurred while saving %s. "
			"A backup file is created at %s, you should "
			"check it and recover the data.\n", fname,
			bakfname);
}

static int safechk(char *fname)
{
	char *bak;

	bak = getbname(fname);
	if (access(bak, F_OK) == 0) {
		apperr = AEEXBF;
		return -1;
	}
	return 0;
}
