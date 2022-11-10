#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include "apperr.h"
#include "applim.h"
#include "card.h"
#include "siglock.h"
#include "ctab.h"

static char *backup(char *src);

char *bakfname;
int lineno, bakferr;

int loadctab(char *filename, struct card *cards, int maxn)
{
	FILE *fp;
	int ret, ncard, nline, maxnl, n;

	ret = -1;
	if (!(fp = fopen(filename, "r"))) {
		apperr = AESYS;
		goto RET;
	}
	maxnl = NLINE;
	ncard = lineno = 0;
	while (ncard < maxn) {
		n = readcard(fp, &cards[ncard], &nline, maxnl);
		maxnl -= nline;
		if (n == 0)
			break;
		if (n == -1) {
			lineno = NLINE - maxnl;
			goto CLR;
		}
		ncard++;
	}
	if (feof(fp))
		ret = ncard;
CLR:	while (n-- > 0)
		destrcard(&cards[n]);
	fclose(fp);
RET:	return ret;
}

int dumpctab(char *filename, struct card *cards, int n)
{
	FILE *fp;
	int ret, i;

	ret = -1;
	siglock(SIGLOCK_LOCK);
	if (!(bakfname = backup(filename))) {
		bakferr = apperr;
		apperr = AEBACKUP;
		goto RET;
	}
	if (!(fp = fopen(filename, "w"))) {
		apperr = AESYS;
		goto WERR;
	}
	for (i = 0; i < n; i++) {
		if (writecard(fp, &cards[i]) == -1)
			goto WERR;
		if (cards[i].sep && fputs(cards[i].sep, fp) == EOF)
			goto WERR;
	}
	if (fclose(fp) == EOF) {
		fp = NULL;
		apperr = AESYS;
		goto WERR;
	}
	unlink(bakfname);
	bakfname = NULL;
	ret = 0;
RET:	siglock(SIGLOCK_UNLOCK);
	return ret;
WERR:	if (fp)
		fclose(fp);
	goto RET;
}

static char *backup(char *src)
{
	static char dst[PATHSZ];
	char *ret, buf[8192];
	int fd[2], n;

	ret = NULL;
	strncpy(dst, "/tmp/hardv.XXXXXX", PATHSZ);
	if (dst[PATHSZ - 1]) {
		apperr = AEPATHSZ;
		goto RET;
	}
	if ((fd[1] = mkstemp(dst)) == -1) {
		apperr = AESYS;
		goto RET;
	}
	if ((fd[0] = open(src, O_RDONLY)) == -1) {
		apperr = AESYS;
		goto CL1;
	}
	while ((n = read(fd[0], buf, sizeof buf)) > 0)
		 if (write(fd[1], buf, n) != n) {
		 	apperr = AESYS;
			goto CL0;
		}
	if (n < 0) {
		apperr = AESYS;
		goto CL0;
	}
	ret = dst;
CL0:	close(fd[0]);
CL1:	close(fd[1]);
RET:	return ret;
}
