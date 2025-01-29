#include <ctype.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "hardv.h"

static void
shuf(size_t *a, size_t n)
{
	size_t i, j, s;

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

static void
chtz(char *tz)
{
	if (tz)
		setenv("TZ", tz, 1);
	else
		unsetenv("TZ");
	tzset();
}

static char *
normv(char *dst, char *src)
{
	char *p;

	p = dst;
	while (*++src)
		if (*src != '\t' || src[-1] != '\n')
			*p++ = *src;
	while (p != dst && p[-1] == '\n') --p;
	*p = '\0';
	return dst;
}

static int
issameday(time_t a, time_t b)
{
	struct tm ta, tb;

	memcpy(&ta, localtime(&a), sizeof ta);
	memcpy(&tb, localtime(&b), sizeof tb);
	return ta.tm_year==tb.tm_year&&ta.tm_mon==tb.tm_mon&&ta.tm_mday==tb.tm_mday;
}

static int
isnow(struct card *card)
{
	time_t should;

	should = elapsecs(getv(card, NEXT));
	if (opt.exact)
		return now >= should;
	else
		return now >= should || issameday(now, should);
}

static char *
chosemod(struct card *card)
{
	static char buf[MAXN];
	char *mod;

	if (!(mod=getv(card,MOD)))
		return "exec " LIBEXECDIR "/hardv/stdq";
	normv(buf, mod);
	return buf;
	return mod;
}

static void
envprepare(struct card *card, int isfirst)
{
	char buf[MAXN+8];
	struct field *f;

	for (f=card->field; f; f=f->next) {
		strcpy(buf, "HARDV_F_");
		strcat(buf, f->key);
		if (setenv(buf,f->val,1) == -1) syserr();
	}
	if (setenv("HARDV_Q",normv(buf,getv(card,Q)),1) == -1) syserr();
	if (setenv("HARDV_A",normv(buf,getv(card,A)),1) == -1) syserr();
	sprintf(buf, "%ld", now);
	if (setenv("HARDV_NOW",buf,1) == -1) syserr();
	sprintf(buf, "%ld", elapsecs(getv(card,PREV)));
	if (setenv("HARDV_PREV",buf,1) == -1) syserr();
	sprintf(buf, "%ld", elapsecs(getv(card,NEXT)));
	if (setenv("HARDV_NEXT",buf,1) == -1) syserr();
	if (setenv("HARDV_FIRST",isfirst?"1":"",1) == -1) syserr();
}

static void
setv(struct card *card, char *k, char *v)
{
	struct field *p;

	for (p=card->field;p;p=p->next)
		if (!strcmp(p->key, k)) {
			free(p->val);
			if (!(p->val=strdup(v))) syserr();
			return;
		}
	if (!(p=calloc(1,sizeof *p))) syserr();
	if (!(p->key=strdup(k))) syserr();
	if (!(p->val=strdup(v))) syserr();
	p->next = card->field;
	card->field = p;
}

static void
settime(struct card *card, char *k, time_t v)
{
	char buf[MAXN];
	struct tm *t;

	t = localtime(&v);
	strftime(buf, sizeof buf, "\t%Y-%m-%d %H:%M:%S %z\n", t);
	setv(card, k, buf);
}

static void
sety(struct card *card)
{
	time_t prev, next, diff;

	prev = getv(card,PREV)?elapsecs(getv(card,PREV)):now;
	next = getv(card,NEXT)?elapsecs(getv(card,NEXT)):now;
	diff = next - prev;
	if (diff < 86400) diff = 86400;
	settime(card, PREV, now);
	settime(card, NEXT, now+2*diff);
	ctabdump(card->file);
}

static void
setn(struct card *card)
{
	settime(card, PREV, now);
	settime(card, NEXT, now+86400);
	ctabdump(card->file);
}

static void
quiz(struct card *card)
{
	static int isfirst = 1;
	int st, pid;
	char *mod;

	mod = chosemod(card);
	if ((pid=fork()) == -1) syserr();
	if (pid == 0) {
		envprepare(card, isfirst);
		execl("/bin/sh", "/bin/sh", "-c", mod, NULL);
		err("shell failed: %s", mod);
	}
	while (wait(&st) == -1)
		if (errno != EINTR)
			syserr();
	if (WIFEXITED(st)) {
		if (WEXITSTATUS(st) == 0) sety(card);
		if (WEXITSTATUS(st) == 1) setn(card);
	}
	isfirst = 0;
}

void
learn(void)
{
	size_t *plan, nq, i;

	if (!(plan=calloc(ncards,sizeof *plan))) syserr();
	for (nq=i=0; i<ncards; i++)
		if (ctab[i].field && isnow(&ctab[i]))
			plan[nq++] = i;
	if (opt.rand) shuf(plan, nq);
	if (opt.maxn>0 && nq>opt.maxn) nq = opt.maxn;
	for (i=0; i<nq; i++) quiz(&ctab[plan[i]]);
}
