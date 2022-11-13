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

enum { SETF_CREAT = 1, SETF_EXCLD = 2 };
static struct field *
setfield(struct card *card, char *key, char *val, int opt);
static char *getfield(struct card *card, char *key);
static int gettime(struct card *card, char *key, time_t *tp);
static int settime(struct card *card, char *key, time_t t);
static int validkey(char *key);
static int validfield(struct field *field);
static int validcard(struct card *card);
static struct field *revfield(struct field **f);

int readcard(FILE *fp, struct card *card, int *nline, int maxnl)
{
#define INCNLINE(n) do { \
	if (maxnl - (*nline) < (n)) { \
		apperr = AENLINE; \
		goto ERR; \
	} \
	(*nline) += (n); \
} while (0)

#define CHK_VALSZ(n) do { \
	if (val - vbuf >= VALSZ - (n)) { \
		apperr = AEVALSZ; \
		goto ERR; \
	} \
} while (0)

#define SAVE_FIELD() do { \
	struct field *f; \
\
	if (!(f = setfield(card, kbuf, vbuf, SETF_CREAT|SETF_EXCLD)) \
		|| validfield(f)) { \
		*nline = keylno; \
		goto ERR; \
	} \
} while (0)

	char line[LINESZ], kbuf[KEYSZ], vbuf[VALSZ], *val;
	size_t sep, n;
	int ch, keylno, nf;

	if (feof(fp))
		return 0;
	memset(card, 0, sizeof *card);
	*nline = 0;
	nf = 0;
	while ((ch = fgetc(fp)) == '\n') {
		INCNLINE(1);
		card->leadnewl++;
	}
	if (ungetc(ch, fp) != ch) {
		apperr = AESYS;
		goto ERR;
	}
	val = NULL;
	while (fgets(line, LINESZ, fp)) {
		INCNLINE(1);
		n = strlen(line);
		if (line[n - 1] != '\n' && !feof(fp)) {
			apperr = AELINESZ;
			goto ERR;
		}
		if (line[0] == '%') {
			/* end of card */
			if (!(card->sep = strdup(line))) {
				apperr = AESYS;
				goto ERR;
			}
			break;
		} else if (line[0] == '\t' || line[0] == '\n') {
			/* successive value line */
			if (!val) {
				apperr = AENOKEY;
				goto ERR;
			}
			CHK_VALSZ(n);
			val = stpcpy(val, line);
		} else {
			/* new field */
			if (nf >= NFIELD) {
				apperr = AENFIELD;
				goto ERR;
			}
			if (val)
				SAVE_FIELD();
			keylno = *nline;
			nf++;
			val = vbuf;
			*val = '\0';
			sep = strcspn(line, "\n\t");
			if (sep >= KEYSZ) {
				apperr = AEKEYSZ;
				goto ERR;
			}
			*stpncpy(kbuf, line, sep) = '\0';
			if (validkey(kbuf))
				goto ERR;
			if (!line[sep])
				continue;
			CHK_VALSZ(n - sep);
			val = stpcpy(val, &line[sep]);
		}
	}
	if (ferror(fp)) {
		apperr = AESYS;
		goto ERR;
	}
	if (val) {
		SAVE_FIELD();
		if (validcard(card)) {
			*nline = card->leadnewl + 1;
			goto ERR;
		}
	}
	revfield(&card->field);
	return 1;
ERR:	destrcard(card);
	return -1;

#undef INCNLINE
#undef CHK_VALSZ
#undef SAVE_FIELD
}

int writecard(FILE *fp, struct card *card)
{
	struct field *f;
	int i;

	for (i = 0; i < card->leadnewl; i++)
		if (fputc('\n', fp) == EOF) {
			apperr = AESYS;
			return -1;
		}
	for (f = card->field; f != NULL; f = f->next) {
		if (fprintf(fp, "%s%s", f->key, f->val) < 0) {
			apperr = AESYS;
			return -1;
		}
	}
	return 0;
}

void destrcard(struct card *card)
{
	struct field *i, *j;

	free(card->sep);
	for (i = card->field; i != NULL; i = j) {
		free(i->key);
		free(i->val);
		j = i->next;
		free(i);
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

time_t getprev(struct card *card)
{
	time_t t;

	gettime(card, PREV, &t);
	return t;
}

time_t getnext(struct card *card)
{
	time_t t;

	gettime(card, NEXT, &t);
	return t;
}

int setprev(struct card *card, time_t prev)
{
	return settime(card, PREV, prev);
}

int setnext(struct card *card, time_t next)
{
	return settime(card, NEXT, next);
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

static struct field *
setfield(struct card *card, char *key, char *val, int opt)
{
	struct field *i;

	if (strlen(key) >= KEYSZ) {
		apperr = AEKEYSZ;
		return NULL;
	}
	for (i = card->field; i != NULL; i = i->next)
		if (!strcmp(i->key, key)) {
			if (opt & SETF_EXCLD) {
				apperr = AEDUPKEY;
				return NULL;
			}
			break;
		}
	if (!i) {
		if (!(opt & SETF_CREAT)) {
			apperr = AEINVKEY;
			return NULL;
		}
		if (!(i = malloc(sizeof *i))) {
			apperr = AESYS;
			return NULL;
		}
		memset(i, 0, sizeof *i);
		if (!(i->key = strdup(key))) {
			free(i);
			apperr = AESYS;
			return NULL;
		}
		i->next = card->field;
		card->field = i;
	}
	free(i->val);
	if (!(i->val = strdup(val))) {
		apperr = AESYS;
		return NULL;
	}
	return i;
}

static int validkey(char *key)
{
	while (*key && (isalpha(*key) || isdigit(*key) || *key == '_'))
		key++;
	if (*key)
		apperr = AEINVKEY;
	return *key;
}

static int validfield(struct field *field)
{
	time_t t;

	if (!strcmp(field->key, PREV) || !strcmp(field->key, NEXT))
		return parsetm(field->val, &t);
	return 0;
}

static int validcard(struct card *card)
{
	if (!getques(card) || !getansw(card)) {
		apperr = AEMFIELD;
		return -1;
	}
	return 0;
}

static char *getfield(struct card *card, char *key)
{
	struct field *i;

	for (i = card->field; i != NULL; i = i->next)
		if (!strcmp(i->key, key))
			return i->val;
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

	if (!(lctime = localtime(&t))) { apperr = AESYS; return -1; }
	buf[0] = '\t';
	if (!(n = strftime(&buf[1], VALSZ - 2, TIMEFMT " %z",
		lctime))) {
		apperr = AEVALSZ; return -1;
	}
	buf[1 + n] = '\n';
	buf[2 + n] = '\0';
	return setfield(card, key, buf, SETF_CREAT) ? 0 : -1;
}

static struct field *revfield(struct field **f)
{
	struct field *c, *n;

	c = *f;
	if (c && (n = c->next)) {
		revfield(&n)->next = c;
		c->next = NULL;
		*f = n;
	}
	return c;
}
