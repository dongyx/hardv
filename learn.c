#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
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
static int plancmp(int *i, int *j);
static int sety(struct card *card, time_t now);
static int setn(struct card *card, time_t now);
static void preset(struct card *card, time_t now,
	time_t *prev, time_t *next, time_t *diff);
int pindent(char *s);

int learn(char *filename, int now, struct learnopt *opt)
{
	static int plan[NCARD];
	struct card *card;
	int i, j, swp, ret;

	ret = -1;
	curfile = filename;
	learnopt = opt;
	lineno = 0;
	if ((ncard = loadctab(curfile, cardtab, NCARD)) == -1)
		goto CLR;
	for (i = 0; i < ncard; i++)
		plan[i] = i;
	if (opt->rand)
		for (i = 0; i < ncard; i++) {
			j = i + rand() % (ncard - i);
			swp = plan[i];
			plan[i] = plan[j];
			plan[j] = swp;
		}
	else
		qsort(plan, ncard, sizeof plan[0], (int (*)())plancmp);
	for (i = 0; i < ncard && opt->maxn; i++) {
		card = &cardtab[plan[i]];
		if (!card->field)
			continue;
		if (isnow(card, now)) {
			if (!opt->any)
				puts(CAVEAT);
			if (getmod(card)) {
				if (exemod(card, now) == -1)
					goto CLR;
			} else {
				if (recall(card, now) == -1)
					goto CLR;
			}
			opt->any = 1;
			if (opt->maxn > 0)
				opt->maxn--;
		}
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

	getnext(card, &next);
	if (next <= 0)
		next = now;
	if (learnopt->exact)
		return now >= next;
	memcpy(&today, localtime(&now), sizeof today);
	memcpy(&theday, localtime(&next), sizeof theday);
	return today.tm_year >= theday.tm_year &&
		today.tm_mon >= theday.tm_mon &&
		today.tm_mday >= theday.tm_mday;
}

static int exemod(struct card *card, time_t now)
{
	char vbuf[VALSZ];
	pid_t pid;
	int stat;

	if ((pid = fork()) == -1) {
		apperr = AESYS;
		return -1;
	}
	/* child */
	if (pid == 0) {
		if (setenv("HARDV_Q",
			normval(getques(card), vbuf, VALSZ)
			, 1) == -1
		|| setenv("HARDV_A",
			normval(getansw(card), vbuf, VALSZ)
			, 1) == -1
		|| setenv("HARDV_FIRST",
			learnopt->any ? "" : "1"
			, 1) == -1
		) {
			perror("setenv");
			exit(2);
		}
		if (execl(SHELL, SHELL, "-c",
			normval(getmod(card), vbuf, VALSZ)
			, NULL) == -1
		) {
			apperr = AESYS;
			exit(2);
		}
	}
	/* parent */
	if (waitpid(pid, &stat, 0) != pid) {
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
	if (!fgets(in, sizeof in, stdin)) {
		if (feof(stdin))
			return 0;
		apperr = AESYS;
		return -1;
	}
	if (strcmp(in, "\n"))
		goto CHECK;
	puts("A:\n");
	pindent(answ);
	puts("\n");
	fflush(stdout);
QUERY:
	fputs("Do you recall? (y/n/s)\n", stdout);
	fflush(stdout);
	if (!fgets(in, sizeof in, stdin)) {
		if (feof(stdin))
			return 0;
		apperr = AESYS;
		return -1;
	}
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

static int plancmp(int *i, int *j)
{
	time_t ni, nj;

	getnext(&cardtab[*i], &ni);
	getnext(&cardtab[*j], &nj);
	if (ni < nj) return -1;
	if (ni > nj) return 1;
	if (*i < *j) return -1;
	if (*i > *j) return 1;
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
	getprev(card, prev);
	if (*prev <= 0)
		*prev = now;	
	getnext(card, next);
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
