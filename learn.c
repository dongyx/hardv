#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include "card.h"
#include "ctab.h"
#include "apperr.h"
#include "applim.h"
#include "learn.h"
#define SHELL "/bin/sh"
#define DAY 60*60*24

#define CAVEAT \
	"* DO NOT EDIT INPUT FILES OR RUN ANOTHER INSTANCE OF HARDV " \
	"ON ANY OF THE SAME\n" \
	"* FILES BEFORE THIS INSTANCE EXITS.\n"

static struct learnopt *learnopt;
static char *curfile;
static struct card cardtab[NCARD];
static int ncard;

static int isnow(struct card *card, time_t now);
static int exemod(struct card *card, time_t now);
static int recall(struct card *card, time_t now);
static int sety(struct card *card, time_t now);
static int setn(struct card *card, time_t now);
static void preset(struct card *card, time_t now,
	time_t *prev, time_t *next, time_t *diff);
int pindent(char *s);

int learn(char *filename, int now, struct learnopt *opt)
{
	static int plan[NCARD];
	struct card *card;
	int i, j, swp, ret, stop, np;

	ret = -1;
	curfile = filename;
	learnopt = opt;
	lineno = 0;
	if ((ncard = loadctab(curfile, cardtab, NCARD)) == -1)
		goto CLR;
	np = 0;
	for (i = 0; i < ncard; i++) {
		card = &cardtab[i];
		if (card->field && isnow(card, now))
			plan[np++] = i;
	}
	if (opt->rand)
		for (i = 0; i < np; i++) {
			j = i + rand() % (np - i);
			swp = plan[i];
			plan[i] = plan[j];
			plan[j] = swp;
		}
	stop = 0;
	for (i = 0; !stop && i < np && opt->maxn; i++) {
		card = &cardtab[plan[i]];
		if (!opt->any)
			puts(CAVEAT);
		if (getmod(card)) {
			if (exemod(card, now) == -1)
				goto CLR;
		} else switch(recall(card, now)) {
			case -1:	goto CLR;
			case  1:	stop = 1;
		}
		opt->any = 1;
		if (opt->maxn > 0)
			opt->maxn--;
	}
	ret = 0;
CLR:	for (i = 0; i < ncard; i++)
		destrcard(&cardtab[i]);
	return ret;
}

static int isnow(struct card *card, time_t now)
{
	struct tm today, theday;
	time_t next;

	next = getnext(card);
	if (next <= 0)
		next = now;
	if (learnopt->exact)
		return now >= next;
	memcpy(&today, localtime(&now), sizeof today);
	memcpy(&theday, localtime(&next), sizeof theday);
	if (today.tm_year > theday.tm_year)
		return 1;
	if (today.tm_year == theday.tm_year
		&& today.tm_mon > theday.tm_mon)
		return 1;
	if (today.tm_year == theday.tm_year
		&& today.tm_mon == theday.tm_mon
		&& today.tm_mday >= theday.tm_mday)
		return 1;
	return 0;
}

static int exemod(struct card *card, time_t now)
{
	const char fpref[] = "HARDV_F_";
	char sprev[64], snext[64], snow[64], first[2];
	char kbuf[KEYSZ + sizeof(fpref) - 1];
	char mod[VALSZ], q[VALSZ], a[VALSZ];
	struct field *f;
	pid_t pid;
	int stat;

	if ((pid = fork()) == -1) {
		apperr = AESYS;
		return -1;
	}
	/* child */
	if (pid == 0) {
		strcpy(kbuf, fpref);
		normval(getmod(card), mod, VALSZ);
		normval(getques(card), q, VALSZ);
		normval(getansw(card), a, VALSZ);
		sprintf(snext, "%ld", (long)getnext(card));
		sprintf(sprev, "%ld", (long)getprev(card));
		sprintf(snow, "%ld", (long)now);
		first[0] = first[1] = '\0';
		if (!learnopt->any)
			first[0] = '1';
		if (	setenv("HARDV_Q",	q,	1) == -1 ||
			setenv("HARDV_A",	a,	1) == -1 ||
			setenv("HARDV_NEXT",	snext,	1) == -1 ||
			setenv("HARDV_PREV",	sprev,	1) == -1 ||
			setenv("HARDV_NOW",	snow,	1) == -1 ||
			setenv("HARDV_FIRST",	first,	1) == -1
		) {
			perror("setenv[1]");
			exit(2);
		}
		for (f = card->field; f != NULL; f = f->next) {
			strcpy(&kbuf[sizeof(fpref) - 1], f->key);
			if (setenv(kbuf, f->val, 1) == -1) {
				perror("setenv[2]");
				exit(2);
			}
		}
		if (execl(SHELL, SHELL, "-c", mod, NULL) == -1) {
			apperr = AESYS;
			exit(2);
		}
	}
	/* parent */
	while (waitpid(pid, &stat, 0) == -1)
		if (errno != EINTR) {
			apperr = AESYS;
			return -1;
		}
	switch (WEXITSTATUS(stat)) {
	case 0:
		if (sety(card, now) == -1)
			return -1;
		break;
	case 1:
		if (setn(card, now) == -1)
			return -1;
		break;
	}
	return 0;
}

static int recall(struct card *card, time_t now)
{
#define GETIN() do { \
	while (!fgets(in, sizeof in, stdin)) { \
		if (feof(stdin)) \
			return 1; \
		if (errno != EINTR) { \
			apperr = AESYS; \
			return -1; \
		} \
	} \
} while (0)

	char in[BUFSIZ], ques[VALSZ], answ[VALSZ];

	if (learnopt->any)
		putchar('\n');
	normval(getques(card), ques, VALSZ);
	normval(getansw(card), answ, VALSZ);
	puts("Q:\n");
	pindent(ques);
	puts("\n");
	fflush(stdout);
CHECK:
	fputs("Press <ENTER> to check the answer.\n", stdout);
	fflush(stdout);
	GETIN();
	if (strcmp(in, "\n"))
		goto CHECK;
	puts("A:\n");
	pindent(answ);
	puts("\n");
	fflush(stdout);
QUERY:
	fputs("Do you recall? (y/n/s)\n", stdout);
	fflush(stdout);
	GETIN();
	if (strcmp(in, "y\n") && strcmp(in, "n\n") && strcmp(in, "s\n"))
		goto QUERY;
	switch (in[0]) {
	case 'y':
		if (sety(card, now) == -1)
			return -1;
		break;
	case 'n':
		if (setn(card, now) == -1)
			return -1;
		break;
	}
	return 0;
}

static int sety(struct card *card, time_t now)
{
	time_t diff, prev, next;

	preset(card, now, &prev, &next, &diff);
	if (setprev(card, now)) return -1;
	if (setnext(card, now + 2*diff)) return -1;
	if (dumpctab(curfile, cardtab, ncard)) return -1;
	return 0;
}

static int setn(struct card *card, time_t now)
{
	time_t diff, prev, next;

	preset(card, now, &prev, &next, &diff);
	if (setprev(card, now)) return -1;
	if (setnext(card, now + DAY)) return -1;
	if (dumpctab(curfile, cardtab, ncard)) return -1;
	return 0;
}

static void preset(struct card *card, time_t now,
	time_t *prev, time_t *next, time_t *diff)
{
	*prev = getprev(card);
	if (*prev <= 0)
		*prev = now;	
	*next = getnext(card);
	if (*next <= 0)
		*next = now;
	if (*next < *prev || (*diff = *next - *prev) < DAY)
		*diff = DAY;
}

int pindent(char *s)
{
	char *sp;

	for (sp = s; *sp ; sp++) {
		if (sp == s || sp[-1] == '\n')
			putchar('\t');
		putchar(*sp);
	}
	return sp - s;
}
