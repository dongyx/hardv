#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include "card.h"
#define MAXCARD 65536
#define quit(f, s) do { f(s); exit(1); } while (0)
#define _S(x) #x
#define S(x) _S(x)

static const char iso8601[] = "%Y-%m-%d %H:%M:%S %z";
const int savesigs[] = { SIGTERM, SIGINT, SIGHUP };
static sigset_t savesigset, osigset;
static char *curfile;
static struct card cardtab[MAXCARD];
static int ncard, plan[MAXCARD];
static void validcard(struct card *card);
static int plancmp(int *i, int *j);
static void learn(struct card *card, time_t now);
static void saveback(int signo);
static char *getfront(struct card *card);
static char *getback(struct card *card);
static time_t getprev(struct card *card);
static time_t getnext(struct card *card);
static time_t gettime(struct card *card, char *key);
static int setprev(struct card *card, time_t t);
static int setnext(struct card *card, time_t t);
static int settime(struct card *card, char *key, time_t t);
static void pversion(FILE *fp);

main(int argc, char **argv)
{
	struct card card, *cardp;
	time_t now;
	FILE *fp;
	int n, i;

	if (argc < 2) {
		pversion(stderr);
		return 1;
	}
	if (!strcmp(argv[1], "-v")) {
		pversion(stdout);
		return 0;
	}
	sigemptyset(&savesigset);
	for (i = 0; i < sizeof savesigs / sizeof savesigs[0]; i++)
		sigaddset(&savesigset, savesigs[i]);
	sigprocmask(SIG_BLOCK, &savesigset, &osigset);
	for (i = 0; i < sizeof savesigs / sizeof savesigs[0]; i++)
		signal(savesigs[i], saveback);
	now = time(0);
	while (*++argv) {
		curfile = *argv;
		if (!(fp = fopen(*argv, "r")))
			quit(perror, "fopen");
		while ((n = cardread(fp, &card)) > 0) {
			if (ncard >= MAXCARD) {
				fprintf(stderr,
					"too many cards in one file\n");
				exit(1);
			}
			validcard(&card);
			cardtab[ncard] = card;
			plan[ncard] = ncard;
			ncard++;
		}
		if (n == -1)
			quit(cardperr, "cardread");
		fclose(fp);
		sigprocmask(SIG_SETMASK, &osigset, NULL);
		puts("muhaha");
		getchar();
		qsort(plan, ncard, sizeof plan[0], (int (*)())plancmp);
		for (i = 0; i < ncard; i++) {
			cardp = &cardtab[plan[i]];
			if (getnext(cardp) <= time(0))
				learn(cardp, now);
		}
		saveback(0);
	}
	return 0;
}

static void validcard(struct card *card)
{
	char *msg;

	msg = NULL;
	if (!getfront(card))
		msg = "the card must contain a front side";
	if (!getback(card))
		msg = "the card must contain a back side";
	if (msg) {
		fprintf(stderr, "%s\n", msg);
		exit(1);
	}
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
	return 0;
}

static void learn(struct card *card, time_t now)
{
	const time_t day = 60*60*24;
	char in[BUFSIZ];
	time_t diff, prev, next;

	if (0 == (prev = getprev(card)))
		prev = now;	
	next = getnext(card);
	if ((diff = next - prev) < day)
		diff = day;
CHECK:
	printf("%s\n\n", getfront(card));
	fputs("press <ENTER> to check the back ", stdout);
	fgets(in, sizeof in, stdin);
	if (strcmp(in, "\n"))
		goto CHECK;
	printf("\n%s\n", getback(card));
QUERY:
	fputs("\ndo you recall? (y/n/c) ", stdout);
	fgets(in, sizeof in, stdin);
	if (strcmp(in, "y\n") && strcmp(in, "n\n") && strcmp(in, "c\n"))
		goto QUERY;
	switch (in[0]) {
	case 'y':
		sigprocmask(SIG_BLOCK, &savesigset, &osigset);
		if (setprev(card, now) == -1)
			quit(cardperr, "setprev");
		if (setnext(card, now + 2*diff) == -1)
			quit(cardperr, "setprev");
		sigprocmask(SIG_SETMASK, &osigset, NULL);
		break;
	case 'n':
		sigprocmask(SIG_BLOCK, &savesigset, &osigset);
		if (setprev(card, now) == -1)
			quit(cardperr, "setprev");
		if (setnext(card, now + day) == -1)
			quit(cardperr, "setprev");
		sigprocmask(SIG_SETMASK, &osigset, NULL);
		break;
	case 'c':
		break;
	}
	putchar('\n');
}

static void saveback(int signo)
{
	FILE *fp;
	int i;

	sigprocmask(SIG_BLOCK, &savesigset, &osigset);
	if (curfile) {
		if (!(fp = fopen(curfile, "w")))
			quit(perror, "fopen");
		for (i = 0; i < ncard; i++) {
			if (i && fputc('\n', fp) == EOF)
				quit(perror, "fputc");
			if (cardwrite(fp, &cardtab[i]) == -1)
				quit(cardperr, "cardwrite");
			carddestr(&cardtab[i]);
		}
		fclose(fp);
		if (signo > 0)
			exit(128 + signo);
	}
	curfile = NULL;
	sigprocmask(SIG_SETMASK, &osigset, NULL);
}

static char *getfront(struct card *card)
{
	return cardget(card, "FRONT");
}

static char *getback(struct card *card)
{
	return cardget(card, "BACK");
}

static time_t getprev(struct card *card)
{
	return gettime(card, "PREV");
}

static time_t getnext(struct card *card)
{
	return gettime(card, "NEXT");
}

static int setprev(struct card *card, time_t t)
{
	return settime(card, "PREV", t);
}

static int setnext(struct card *card, time_t t)
{
	return settime(card, "NEXT", t);
}

static int settime(struct card *card, char *key, time_t t)
{
	char buf[(sizeof iso8601 / sizeof *iso8601) * 8];

	strftime(buf, sizeof buf, iso8601, localtime(&t));
	return cardset(card, key, buf, 1);
}

static time_t gettime(struct card *card, char *key)
{
	const char *s;
	struct tm buf;

	if (!(s = cardget(card, key)))
		return 0;
	memset(&buf, 0, sizeof buf);
	if (!strptime(s, iso8601, &buf)) {
		fputs("invalid time format\n", stderr);
		exit(1);
	}
	return mktime(&buf);
}

static void pversion(FILE *fp)
{
	fprintf(fp, "hardv %s\n", S(VERSION));
}
