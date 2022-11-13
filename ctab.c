#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include "apperr.h"
#include "applim.h"
#include "card.h"
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
			goto ERR;
		}
		ncard++;
	}
	if (feof(fp)) {
		lineno = 0;
		ret = ncard;
	}
	else {
		apperr = AENCARD;
		goto ERR;
	}
CLS:	fclose(fp);
RET:	return ret;
ERR:	while (ncard-- > 0)
		destrcard(&cards[ncard]);
	goto CLS;
}

int dumpctab(char *filename, struct card *cards, int n)
{
	sigset_t bset, oset;
	FILE *fp;
	int ret, i;

	ret = -1;
	sigemptyset(&bset);
	sigaddset(&bset, SIGHUP);
	sigaddset(&bset, SIGINT);
	sigaddset(&bset, SIGTERM);
	sigaddset(&bset, SIGQUIT);
	sigaddset(&bset, SIGTSTP);
	sigprocmask(SIG_BLOCK, &bset, &oset);
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
RET:	sigprocmask(SIG_SETMASK, &oset, NULL);
	return ret;
WERR:	if (fp)
		fclose(fp);
	goto RET;
}

char *getbname(char *src)
{
	static char bak[PATHSZ];
	int n;

	n = snprintf(bak, PATHSZ, "%s~", src);
	if (n >= PATHSZ) { apperr = AEPATHSZ; return NULL; }
	if (n < 0) { apperr = AESYS; return NULL; }
	return bak;
}

static char *backup(char *src)
{
	char *ret, *dst, buf[8192];
	int fd[2], n;

	ret = NULL;
	dst = getbname(src);
	if ((fd[1] = open(dst, O_WRONLY|O_CREAT|O_EXCL)) == -1) {
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
