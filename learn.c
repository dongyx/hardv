#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "hardv.h"

static void
shuf(int *a, int n)
{
	int i, j, s;

	for (i=0; i<n; i++) {
		j = i + rand()%(n-i);
		s = a[i];
		a[i] = a[j];
		a[j] = s;
	}
}

static char *
getv(struct card *card, char *key)
{
	struct field *p;

	for (p=card->field; p; p=p->next)
		if (!strcmp(p->key, key))
			return p->val;
	return NULL;
}

static long
elapsecs(char *buf)
{
	char *zs;
	long z, off;
	struct tm tm;

	if (!(zs=strptime(buf, "%Y-%m-%d %H:%M:%S", &tm)))
		err("invalid time: %s", buf);
	z = atol(zs);
	off = labs(z)/100*3600+labs(z)%100*60;
	off *= z>=0?1:-1;
	return (long)timegm(&tm) + off;
}

static int
issameday(long a, long b)
{
	struct tm ta, tb;
	time_t ca, cb;

	ca = (time_t)a;
	cb = (time_t)b;
	memcpy(&ta, localtime(&ca), sizeof ta);
	memcpy(&tb, localtime(&cb), sizeof tb);
	return ta.tm_year==tb.tm_year&&ta.tm_mon==tb.tm_mon&&ta.tm_mday==tb.tm_mday;
}

static int
isnow(struct card *card)
{
	long should;

	should = elapsecs(getv(card, NEXT));
	if (opt.exact)
		return now >= should;
	else
		return now >= should || issameday(now, should);
}

static char *
chosemod(struct card *card)
{
	return getv(card, MOD);
}

static void
envprepare(struct card *card)
{
}

static void
sety(struct card *card)
{
}

static void
setn(struct card *card)
{
}

static void
quiz(struct card *card)
{
	int st, pid;
	char *mod;

	mod = chosemod(card);
	if ((pid=fork()) == -1) syserr();
	if (pid == 0) {
		envprepare(card);
		execl("/bin/sh", "-c", mod);
		syserr();
	}
	while (wait(&st) == -1)
		if (errno != EINTR)
			syserr();
	if (WIFEXITED(st)) {
		if (WEXITSTATUS(st) == 0) sety(card);
		if (WEXITSTATUS(st) == 1) setn(card);
	}
}

void
learn(void)
{
	int plan[MAXN], nq, i;

	for (nq=i=0; i<ncards; i++)
		if (ctab[i].field && isnow(&ctab[i]))
			plan[nq++] = i;
	if (opt.rand) shuf(plan, nq);
	if (opt.maxn>0 && nq>opt.maxn) nq = opt.maxn;
	for (i=0; i<nq; i++) quiz(&ctab[plan[i]]);
}
