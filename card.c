#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include "apperr.h"
#include "card.h"
#define QUES "Q"
#define ANSW "A"
#define PREV "PREV"
#define NEXT "NEXT"
#define TIMEFMT "%Y-%m-%d %H:%M:%S"
#define TIMEFMT_P "%d-%d-%d %d:%d:%d %c%2d%2d"

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
	char buf[VALSZ - 2];
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
	if (!strftime(buf, sizeof buf, TIMEFMT " %z", lctime)) {
		apperr = AEVALSZ;
		return -1;
	}
	sprintf(card->field[i].val, "\t%s\n", buf);
	return 0;
}

int parsetm(char *s, time_t *clock)
{
	int year, mon, day, hour, min, sec, zhour, zmin;
	char zsign;
	struct tm dtime;

	memset(&dtime, 0, sizeof dtime);
	while (*s && isspace(*s))
		s++;
	if (sscanf(s, TIMEFMT_P, &year, &mon, &day, &hour, &min, &sec,
		&zsign, &zhour, &zmin) != 9
		|| zsign != '-' && zsign != '+') {
		apperr = AETIMEF;
		return -1;
	}
	dtime.tm_year = year - 1900;
	dtime.tm_mon = mon - 1;
	dtime.tm_mday = day;
	dtime.tm_hour = hour;
	dtime.tm_min = min;
	dtime.tm_sec = sec;
	*clock = timegm(&dtime);
	if (zsign == '+')
		*clock -= zmin*60 + zhour*3600;
	else
		*clock += zmin*60 + zhour*3600;
	return 0;
}

int validkey(char *key)
{
	while (*key && (isalpha(*key) || isdigit(*key) || *key == '_'))
		key++;
	if (*key)
		apperr = AEINVKEY;
	return *key;
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
