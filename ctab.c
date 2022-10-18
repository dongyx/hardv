#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include "apperr.h"
#include "applim.h"
#include "card.h"
#include "parse.h"
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
	ECHK(!(fp = fopen(filename, "r")), AESYS, goto RET);
	maxnl = NLINE;
	ncard = lineno = 0;
	while (ncard < maxn) {
		n = readcard(fp, &cards[ncard], &nline, maxnl);
		maxnl -= nline;
		if (n == 0)
			break;
		if (n == -1) {
			lineno = NLINE - maxnl;
			goto CLS;
		}
		ncard++;
	}
	if (feof(fp))
		ret = ncard;
CLS:	fclose(fp);
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
	ECHK(!(bakfname = backup(filename)), apperr, goto RET);
	ECHK(!(fp = fopen(filename, "w")), AESYS, goto ULK);
	for (i = 0; i < n; i++) {
		ECHK(i && fputc('\n', fp) == EOF, AESYS, goto WERR);
		ECHK(writecard(fp, &cards[i]) == -1, apperr, goto WERR);
	}
	ECHK(fclose(fp) == EOF, AESYS, goto RET);
ULK:	unlink(bakfname);
	bakfname = NULL;
	ret = 0;
RET:	siglock(SIGLOCK_UNLOCK);
	return ret;
WERR:	fclose(fp);
	goto RET;
}

static char *backup(char *src)
{
	static char dst[PATHSZ];
	char *ret, buf[BUFSIZ];
	int fd[2], n;

	ret = NULL;
	strncpy(dst, "/tmp/hardv.XXXXXX", PATHSZ);
	ECHK(dst[PATHSZ - 1], AEPATHSZ, goto RET);
	ECHK((fd[1] = mkstemp(dst)) == -1, AESYS, goto RET);
	ECHK((fd[0] = open(src, O_RDONLY)) == -1, AESYS, goto CL1); 
	while ((n = read(fd[0], buf, sizeof buf)) > 0)
		 ECHK(write(fd[1], buf, n) != n, AESYS, goto CL0); 
	ECHK(n < 0, AESYS, goto CL0);
	ret = dst;
CL0:	close(fd[0]);
CL1:	close(fd[1]);
RET:	return ret;
}
