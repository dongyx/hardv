#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "siglock.h"
#include "card.h"
#include "parse.h"
#include "learn.h"
#define NCARD 65536

static struct learnopt *learnopt;
static char *curfile;
static struct card cardtab[NCARD];
static int ncard;

static void loadctab(void);
static int isnow(struct card *card, time_t now);
static void recall(struct card *card, time_t now);
static int plancmp(int *i, int *j);
static void dumpctab(void);

void learn(char *filename, int now, struct learnopt *opt)
{
	static int plan[NCARD];
	struct card *card;
	int i;

	curfile = filename;
	learnopt = opt;
	loadctab();
	for (i = 0; i < ncard; i++)
		plan[i] = i;
	qsort(plan, ncard, sizeof plan[0], (int (*)())plancmp);
	for (i = 0; i < ncard; i++)
		if (isnow(&cardtab[plan[i]], now))
			recall(&cardtab[plan[i]], now);
}

static void loadctab(void)
{
	FILE *fp;

	ncard = 0;
	if (!(fp = fopen(curfile, "r"))) {
		perror(curfile);
		exit(1);
	}
	while (ncard < NCARD
		&& readcard(fp, curfile, &cardtab[ncard]) > 0)
		ncard++;
	if (!feof(fp)) {
		fprintf(stderr, "an input file can't contain more than "
			"%d cards\n", NCARD);
		exit(1);
	}
}

static int isnow(struct card *card, time_t now)
{
	struct tm today, theday;
	time_t next;

	next = getnext(card);
	if (learnopt->exact)
		return now >= next;
	memcpy(&today, localtime(&now), sizeof today);
	memcpy(&theday, localtime(&next), sizeof theday);
	return today.tm_year >= theday.tm_year &&
		today.tm_mon >= theday.tm_mon &&
		today.tm_mday >= theday.tm_mday;
}

static void recall(struct card *card, time_t now)
{
	const time_t day = 60*60*24;
	char in[BUFSIZ];
	time_t diff, prev, next;

	if ((prev = getprev(card)) == 0)
		prev = now;	
	next = getnext(card);
	if ((diff = next - prev) < day)
		diff = day;
	printf("%s\n\n", getfront(card));
	fflush(stdout);
CHECK:
	fputs("press <ENTER> to check the back\n", stdout);
	fflush(stdout);
	fgets(in, sizeof in, stdin);
	if (strcmp(in, "\n"))
		goto CHECK;
	printf("%s\n\n", getback(card));
	fflush(stdout);
QUERY:
	fputs("do you recall? (y/n/s)\n", stdout);
	fflush(stdout);
	fgets(in, sizeof in, stdin);
	if (strcmp(in, "y\n") && strcmp(in, "n\n") && strcmp(in, "s\n"))
		goto QUERY;
	switch (in[0]) {
	case 'y':
		setprev(card, now);
		setnext(card, now + 2*diff);
		dumpctab();
		break;
	case 'n':
		setprev(card, now);
		setnext(card, now + day);
		dumpctab();
		break;
	case 's':
		break;
	}
}

static void dumpctab(void)
{
	FILE *fp;
	int i;

	siglock(SIGLOCK_LOCK);
	if (!(fp = fopen(curfile, "w"))) {
		perror(curfile);
		exit(1);
	}
	for (i = 0; i < ncard; i++) {
		if (i && fputc('\n', fp) == EOF) {
			perror(curfile);
			exit(1);
		}
		writecard(fp, curfile, &cardtab[i]);
	}
	fclose(fp);
	siglock(SIGLOCK_UNLOCK);
}

static int plancmp(int *i, int *j)
{
	time_t ni, nj;

	ni = getnext(&cardtab[*i]);
	nj = getnext(&cardtab[*j]);
	if (ni < nj)
		return -1;
	if (ni > nj)
		return 1;
	if (*i < *j)
		return -1;
	if (*i > *j)
		return -1;
	return 0;
}
