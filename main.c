#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include "card.h"
#define MAXCARD 65536
#define quit(f, s) do { f(s); exit(1); } while (0)

enum {
	SIGLOCK_INIT,
	SIGLOCK_LOCK,
	SIGLOCK_UNLOCK
};

static const char iso8601[] = "%Y-%m-%d %H:%M:%S %z";

static char *curfile;
static struct card cardtab[MAXCARD];
static int ncard, isexact;

static void help(FILE *fp);
static void pversion(FILE *fp);
static void siglock(int act);
static void loadctab(char *path);
static void schedule(time_t now);
static void dumpctab(int signo);
static void validc(struct card *card);
static int plancmp(int *i, int *j);
static int isnow(struct card *card, time_t now);
static void recall(struct card *card, time_t now);
static char *getfront(struct card *card);
static char *getback(struct card *card);
static time_t getprev(struct card *card);
static time_t getnext(struct card *card);
static int setprev(struct card *card, time_t t);
static int setnext(struct card *card, time_t t);
static time_t gettime(struct card *card, char *key);
static int settime(struct card *card, char *key, time_t t);
static time_t parsetm(char *s);

main(int argc, char **argv)
{
	char *envnow;
	time_t now;
	int ch;

	if ((envnow = getenv("HARDV_NOW")))
		now = parsetm(envnow);
	else
		now = time(NULL);
	while ((ch = getopt(argc, argv, "ehv")) != -1)
		switch (ch) {
		case 'e':
			isexact = 1;
			break;
		case 'h':
			help(stdout);
			return 0;
		case 'v':
			pversion(stdout);
			return 0;
		case '?':
		default:
			help(stderr);
			return 1;
		}
	if ((argv += optind, argc -= optind) == 0) {
		help(stderr);
		return 1;
	}
	siglock(SIGLOCK_INIT);
	while (*argv) {
		loadctab(*argv);
		schedule(now);
		dumpctab(0);
		argv++;
	}
	return 0;
}

static void help(FILE *fp)
{
	fputs("hardv [options] file [file ...]\n", fp);
	fputs("\n", fp);
	fputs("options\n", fp);
	fputs("\n", fp);
	fputs("-e	enable exact quiz time\n", fp);
	fputs("-h	print this help information\n", fp);
	fputs("-v	print version and building information\n", fp);
}

static void pversion(FILE *fp)
{
	fprintf(fp, "hardv %s\n", VERSION);
	fprintf(fp, "%s\n", COPYRT);
	fputc('\n', fp);
	fprintf(fp, "BUFSIZ: %d\n", BUFSIZ);
	fprintf(fp, "MAXCARD: %d\n", MAXCARD);
}

static void siglock(int act)
{
	static const int sig[] = { SIGTERM, SIGINT, SIGHUP };
	static sigset_t set, oset;
	int i;

	switch (act) {
	case SIGLOCK_INIT:
		sigemptyset(&set);
		for (i = 0; i < sizeof sig / sizeof sig[0]; i++)
			sigaddset(&set, sig[i]);
		sigprocmask(SIG_BLOCK, &set, &oset);
		for (i = 0; i < sizeof sig / sizeof sig[0]; i++)
			signal(sig[i], dumpctab);
		sigprocmask(SIG_SETMASK, &oset, NULL);
		break;
	case SIGLOCK_LOCK:
		sigprocmask(SIG_BLOCK, &set, &oset);
		break;
	case SIGLOCK_UNLOCK:
		sigprocmask(SIG_SETMASK, &oset, NULL);
	}
}

static void loadctab(char *path)
{
	struct card card;
	FILE *fp;
	int n;

	siglock(SIGLOCK_LOCK);
	curfile = path;
	ncard = 0;
	if (!(fp = fopen(path, "r")))
		quit(perror, "fopen");
	while ((n = cardread(fp, &card)) > 0) {
		if (ncard >= MAXCARD) {
			fprintf(stderr,
				"too many cards in one file\n");
			exit(1);
		}
		validc(&card);
		cardtab[ncard] = card;
		ncard++;
	}
	if (n == -1)
		quit(cardperr, "cardread");
	fclose(fp);
	siglock(SIGLOCK_UNLOCK);
}

static void schedule(time_t now)
{
	int plan[MAXCARD], n, i;
	struct card *card;

	for (i = 0; i < ncard; i++)
		plan[i] = i;
	qsort(plan, ncard, sizeof plan[0], (int (*)())plancmp);
	for (n = i = 0; i < ncard; i++) {
		card = &cardtab[plan[i]];
		if (isnow(card, now)) {
			if (n++)
				putchar('\n');
			recall(card, now);
		}
	}
}

static void dumpctab(int signo)
{
	FILE *fp;
	int i;

	siglock(SIGLOCK_LOCK);
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
		curfile = NULL;
	}
	siglock(SIGLOCK_UNLOCK);
}

static void validc(struct card *card)
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
	if (*i < *j)
		return -1;
	if (*i > *j)
		return -1;
	return 0;
}

static int isnow(struct card *card, time_t now)
{
	struct tm today, theday;
	time_t next;

	next = getnext(card);
	if (isexact)
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
	siglock(SIGLOCK_LOCK);
	switch (in[0]) {
	case 'y':
		if (setprev(card, now) == -1)
			quit(cardperr, "setprev");
		if (setnext(card, now + 2*diff) == -1)
			quit(cardperr, "setprev");
		break;
	case 'n':
		if (setprev(card, now) == -1)
			quit(cardperr, "setprev");
		if (setnext(card, now + day) == -1)
			quit(cardperr, "setprev");
		break;
	case 's':
		break;
	}
	siglock(SIGLOCK_UNLOCK);
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
	char *s;

	if (!(s = cardget(card, key)))
		return 0;
	return parsetm(s);
}

static time_t parsetm(char *s)
{
	struct tm buf;

	memset(&buf, 0, sizeof buf);
	if (!strptime(s, iso8601, &buf)) {
		fputs("invalid time format\n", stderr);
		exit(1);
	}
	return mktime(&buf);
}
