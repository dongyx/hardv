#include <stdlib.h>
#include <stdio.h>
#include "apperr.h"
#include "card.h"
#include "parse.h"
#include "siglock.h"
#include "ctab.h"

int loadctab(char *filename, struct card *cards, int maxn)
{
	FILE *fp;
	int n, nfield;

	if (!(fp = fopen(filename, "r"))) {
		apperr = AESYS;
		return -1;
	}
	n = 0;
	while (n < maxn && (nfield = readcard(fp, &cards[n])) > 0)
		n++;
	if (nfield == -1)
		return -1;
	if (!feof(fp)) {
		apperr = AENCARD;
		return -1;
	}
	return n;
}

int dumpctab(char *filename, struct card *cards, int n)
{
	FILE *fp;
	int i;

	siglock(SIGLOCK_LOCK);
	if (!(fp = fopen(filename, "w"))) {
		apperr = AESYS;
		return -1;
	}
	for (i = 0; i < n; i++) {
		if (i && fputc('\n', fp) == EOF) {
			apperr = AESYS;
			return -1;
		}
		if (writecard(fp, &cards[i]) == -1)
			return -1;
	}
	if (fclose(fp) == -1) {
		apperr = AESYS;
		return -1;
	}
	siglock(SIGLOCK_UNLOCK);
	return 0;
}
