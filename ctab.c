#include <stdlib.h>
#include <stdio.h>
#include "card.h"
#include "parse.h"
#include "siglock.h"
#include "ctab.h"

int loadctab(char *filename, struct card *cards, int maxn)
{
	FILE *fp;
	int n;

	n = 0;
	if (!(fp = fopen(filename, "r"))) {
		perror(filename);
		exit(1);
	}
	while (n < maxn && readcard(fp, &cards[n], filename) > 0)
		n++;
	if (!feof(fp)) {
		fprintf(stderr, "an input file can't contain more than "
			"%d cards\n", maxn);
		exit(1);
	}
	return n;
}

void dumpctab(char *filename, struct card *cards, int n)
{
	FILE *fp;
	int i;

	siglock(SIGLOCK_LOCK);
	if (!(fp = fopen(filename, "w"))) {
		perror(filename);
		exit(1);
	}
	for (i = 0; i < n; i++) {
		if (i && fputc('\n', fp) == EOF) {
			perror(filename);
			exit(1);
		}
		writecard(fp, &cards[i], filename);
	}
	fclose(fp);
	siglock(SIGLOCK_UNLOCK);
}
