#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "apperr.h"
#include "card.h"
#define TIMEFMT "%Y-%m-%d %H:%M:%S %z"
#define QUES "Q"
#define ANSW "A"
#define NEXT "NEXT"
#define PREV "PREV"

static char *getfield(struct card *card, char *key);
static int gettime(struct card *card, char *key, time_t *tp);
static int settime(struct card *card, char *key, time_t t);

char *getques(struct card *card)
{
	return getfield(card, QUES);
}

char *getansw(struct card *card)
{
	return getfield(card, ANSW);
}

int getprev(struct card *card, time_t *tp)
{
	return gettime(card, PREV, tp);
}

int getnext(struct card *card, time_t *tp)
{
	return gettime(card, NEXT, tp);
}

int setprev(struct card *card, time_t prev)
{
	return settime(card, PREV, prev);
}

int setnext(struct card *card, time_t next)
{
	return settime(card, NEXT, next);
}

int validfield(struct field *field)
{
	time_t t;

	if (!strcmp(field->key, PREV) || !strcmp(field->key, NEXT))
		return parsetm(field->val, &t);
	return 0;
}

int validcard(struct card *card)
{
	time_t prev, next;
	int n;

	if (!getques(card) || !getansw(card)) {
		apperr = AEMFIELD;
		return -1;
	}
	n = !!getfield(card, PREV) + !!getfield(card, NEXT);
	if (card->nfield - n > NFIELD - 2) {
		apperr = AERSVFIELD;
		return -1;
	}
	return 0;
}

static char *getfield(struct card *card, char *key)
{
	int i;

	for (i = 0; i < card->nfield; i++)
		if (!strcmp(card->field[i].key, key))
			return card->field[i].val;
	return NULL;
}

static int gettime(struct card *card, char *key, time_t *tp)
{
	struct tm buf;
	char *val;

	if (!(val = getfield(card, key))) {
		*tp = 0;
		return 0;
	}
	return parsetm(val, tp);
}

static int settime(struct card *card, char *key, time_t t)
{
	struct tm *lctime;
	int i;

	for (i = 0; i < card->nfield; i++)
		if (!strcmp(card->field[i].key, key))
			break;
	if (i == card->nfield) {
		if (strlen(key) >= KEYSZ) {
			apperr = AEKEYSZ;
			return -1;
		}
		strcpy(card->field[card->nfield++].key, key);
	}
	if (!(lctime = localtime(&t))) {
		apperr = AESYS;
		return -1;
	}
	if (!strftime(card->field[i].val, VALSZ, TIMEFMT, lctime)) {
		apperr = AEVALSZ;
		return -1;
	}
	return 0;
}

int parsetm(char *s, time_t *tp)
{
	struct tm buf;

	memset(&buf, 0, sizeof buf);
	if (!strptime(s, TIMEFMT, &buf)) {
		apperr = AETIMEF;
		return -1;
	}
	*tp = mktime(&buf);
	return 0;
}
