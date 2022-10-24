#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>
#include "card.h"
#include "ctab.h"
#include "apperr.h"
#include "applim.h"
#include "parse.h"
#include "learn.h"

static struct learnopt *learnopt;
static char *curfile;
static struct card cardtab[NCARD];
static int ncard;

static int isnow(struct card *card, time_t now);
static int recall(struct card *card, time_t now);
static int plancmp(int *i, int *j);

int learn(char *filename, int now, struct learnopt *opt)
{
	static int plan[NCARD];
	struct card *card;
	int i, j, swp, first;

	curfile = filename;
	learnopt = opt;
	lineno = 0;
	if ((ncard = loadctab(curfile, cardtab, NCARD)) == -1)
		return -1;
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
	first = 1;
	for (i = 0; i < ncard && opt->maxn; i++)
		if (isnow(&cardtab[plan[i]], now)) {
			if (first)
				first = 0;
			else
				putchar('\n');
			if (recall(&cardtab[plan[i]], now) == -1)
				return -1;
			if (opt->maxn > 0)
				opt->maxn--;
		}
	return 0;
}

static int isnow(struct card *card, time_t now)
{
	struct tm today, theday;
	time_t next;

	getnext(card, &next);
	if (learnopt->exact)
		return now >= next;
	memcpy(&today, localtime(&now), sizeof today);
	memcpy(&theday, localtime(&next), sizeof theday);
	return today.tm_year >= theday.tm_year &&
		today.tm_mon >= theday.tm_mon &&
		today.tm_mday >= theday.tm_mday;
}

static int recall(struct card *card, time_t now)
{
	const time_t day = 60*60*24;
	char in[BUFSIZ], *ques, *answ;
	time_t diff, prev, next;

	getprev(card, &prev);
	if (prev == 0)
		prev = now;	
	getnext(card, &next);
	if (next < prev || (diff = next - prev) < day)
		diff = day;
	ques = getques(card);
	answ = getansw(card);
	while (*ques && *ques == '\n')
		ques++;
	while (*answ && *answ == '\n')
		answ++;
	puts("Q:\n");
	printf("%s\n", ques);
	if (ques[strlen(ques) - 1] != '\n')
		putchar('\n');
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
	printf("%s\n", answ);
	if (answ[strlen(answ) - 1] != '\n')
		putchar('\n');
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
		if (setprev(card, now)) return -1;
		if (setnext(card, now + 2*diff)) return -1;
		if (dumpctab(curfile, cardtab, ncard)) return -1;
		break;
	case 'n':
		if (setprev(card, now)) return -1;
		if (setnext(card, now + day)) return -1;
		if (dumpctab(curfile, cardtab, ncard)) return -1;
		break;
	case 's':
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
