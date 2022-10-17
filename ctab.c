#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include "apperr.h"
#include "card.h"
#include "parse.h"
#include "siglock.h"
#include "ctab.h"
#define NAMESZ 1024

static char *backup(char *src);

char *bakfname;
int cardno;

int loadctab(char *filename, struct card *cards, int maxn)
{
	FILE *fp;
	int n, nfield;

	if (!(fp = fopen(filename, "r"))) {
		apperr = AESYS;
		return -1;
	}
	lineno = cardno = n = 0;
	while (n < maxn && (nfield = readcard(fp, &cards[n])) > 0)
		n++;
	switch (nfield) {
	case -2:
		lineno = 0;
		cardno = n + 1;
	case -1:
		return -1;
	}
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
	if (!(bakfname = backup(filename)))
		return -1;
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
	unlink(bakfname);
	bakfname = NULL;
	siglock(SIGLOCK_UNLOCK);
	return 0;
}

static char *backup(char *src)
{
	static char dst[NAMESZ];
	char *ret, buf[BUFSIZ];
	ssize_t n;
	int fd[2];

	strcpy(dst, "/tmp/hardv.XXXXXX");
	ret = NULL;
	if ((fd[1] = mkstemp(dst)) == -1) {
		apperr = AESYS;
		goto RET;
	}
	if ((fd[0] = open(src, O_RDONLY)) == -1) {
		apperr = AESYS;
		goto CL1;
	}
	while ((n = read(fd[0], buf, sizeof buf)) > 0)
		if (write(fd[1], buf, n) == -1) {
			apperr = AESYS;
			goto CL0;
		}
	if (n < 0) {
		apperr = AESYS;
		goto CL0;
	}
	ret = dst;
CL0:
	close(fd[0]);
CL1:
	close(fd[1]);
RET:
	return ret;
}
