#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "card.h"
#define TIMEFMT "%Y-%m-%d %H:%M:%S %z"
#define FRONT "FRONT"
#define BACK "BACK"
#define NEXT "NEXT"
#define PREV "PREV"

static char *getfield(struct card *card, char *key);
static time_t gettime(struct card *card, char *key);
static void settime(struct card *card, char *key, time_t t);

char *getfront(struct card *card)
{
	return getfield(card, FRONT);
}

char *getback(struct card *card)
{

	return getfield(card, BACK);
}

time_t getprev(struct card *card)
{
	return gettime(card, PREV);
}

time_t getnext(struct card *card)
{

	return gettime(card, NEXT);
}

void setprev(struct card *card, time_t prev)
{
	settime(card, PREV, prev);
}

void setnext(struct card *card, time_t next)
{
	settime(card, NEXT, next);
}

void validcard(struct card *card, char *header)
{
	int prev, next;

	getprev(card);
	getnext(card);
	if (!getfront(card) || !getback(card)) {
		fprintf(stderr, "%s: the card must contain "
		"the FRONT and BACK fields\n", header);
		exit(1);
	}
	prev = !!getfield(card, PREV);
	next = !!getfield(card, NEXT);
	if (card->nfield - prev - next > NFIELD - 2) {
		fprintf(stderr, "%s: "
			"number of fields except PREV and NEXT must be"
			" less than %d\n", header, NFIELD - 2);
		exit(1);
	}
}

static char *getfield(struct card *card, char *key)
{
	int i;

	for (i = 0; i < card->nfield; i++)
		if (!strcmp(card->field[i].key, key))
			return card->field[i].val;
	return NULL;
}

static time_t gettime(struct card *card, char *key)
{
	struct tm buf;
	char *val;

	if (!(val = getfield(card, key)))
		return 0;
	return parsetm(val);
}

static void settime(struct card *card, char *key, time_t t)
{
	int i;

	for (i = 0; i < card->nfield; i++)
		if (!strcmp(card->field[i].key, key))
			break;
	if (i == card->nfield)
		strcpy(card->field[card->nfield++].key, key);
	strftime(card->field[i].val, VALSZ, TIMEFMT, localtime(&t)); 
}

time_t parsetm(char *s)
{
	struct tm buf;

	memset(&buf, 0, sizeof buf);
	if (!strptime(s, TIMEFMT, &buf)) {
		fprintf(stderr, "invalid time format: %s\n", s);
		exit(1);
	}
	return mktime(&buf);
}
