#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include "apperr.h"
#include "card.h"
#define MOD "MOD"
#define QUES "Q"
#define ANSW "A"
#define PREV "PREV"
#define NEXT "NEXT"
#define TIMEFMT "%Y-%m-%d %H:%M:%S"
#define TIMEFMT_P "%d-%d-%d %d:%d:%d %c%2d%2d"

static char *getfield(struct card *card, char *key);
static int gettime(struct card *card, char *key, time_t *tp);
static int settime(struct card *card, char *key, time_t t);

void destrcard(struct card *card)
{
	int i;

	free(card->sep);
	for (i = 0; i < card->nfield; i++) {
		free(card->field[i].key);
		free(card->field[i].val);
	}
}

char *getmod(struct card *card)
{
	return getfield(card, MOD);
}

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
	char buf[VALSZ];
	size_t n;
	int i, newf;

	newf = 0;
	for (i = 0; i < card->nfield; i++)
		if (!strcmp(card->field[i].key, key))
			break;
	if (i == card->nfield) {
		if (strlen(key) >= KEYSZ) {
			apperr = AEKEYSZ;
			return -1;
		}
		if (card->nfield >= NFIELD) {
			apperr = AERSVFIELD;
			return -1;
		}
		newf = 1;
		memmove(&card->field[1], &card->field[0],
			card->nfield * sizeof card->field[0]);
		if (!(card->field[0].key = strdup(key))) {
			apperr = AESYS;
			return -1;
		}
		card->nfield++;
		i = 0;
	}
	if (!(lctime = localtime(&t))) {
		apperr = AESYS;
		return -1;
	}
	buf[0] = '\t';
	if (!(n = strftime(&buf[1], VALSZ - 2, TIMEFMT " %z",
		lctime))) {
		apperr = AEVALSZ;
		return -1;
	}
	buf[1 + n] = '\n';
	buf[2 + n] = '\0';
	if (!newf)
		free(card->field[i].val);
	if (!(card->field[i].val = strdup(buf))) {
		apperr = AEVALSZ;
		return -1;
	}
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

char *normval(char *s, char *buf, int n)
{
	char *sp, *bp;

	while (*s == '\n')
		s++;
	for (sp = s, bp = buf; bp != &buf[n] && *sp; sp++)
		if (*sp != '\t' || sp > s && sp[-1] != '\n')
			*bp++ = *sp;
	if (bp == &buf[n])
		return NULL;
	while (bp > buf && bp[-1] == '\n')
		bp--;
	*bp = '\0';
	return buf;
}
