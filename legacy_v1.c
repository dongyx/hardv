#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "apperr.h"
#include "applim.h"
#include "siglock.h"
#include "legacy_v1.h"
#define MOD "MOD"
#define QUES "Q"
#define ANSW "A"
#define PREV "PREV"
#define NEXT "NEXT"
#define TIMEFMT "%Y-%m-%d %H:%M:%S"
#define TIMEFMT_P "%d-%d-%d %d:%d:%d %c%2d%2d"

static char *getfield_v1(struct card_v1 *card, char *key);
static int gettime_v1(struct card_v1 *card, char *key, time_t *tp);
static int settime_v1(struct card_v1 *card, char *key, time_t t);
static int
readcard_v1(FILE *fp, struct card_v1 *card, int *nline, int maxnl);
static int writecard_v1(FILE *fp, struct card_v1 *card);
static int validcard_v1(struct card_v1 *card);
static int validfield_v1(struct field_v1 *field);
static int validkey_v1(char *key);
static int parsetm_v1(char *s, time_t *clock);
static char *backup_v1(char *src);
static int dumpctab_v1(char *filename, struct card_v1 *cards, int n);
static int loadctab_v1(char *filename, struct card_v1 *cards, int maxn);

extern char *bakfname;
extern int lineno, bakferr;

int conv_1to2(char *fname)
{
	static struct card_v1 cards[NCARD];
	int n;

	if ((n = loadctab_v1(fname, cards, NCARD)) < 0)
		return -1;
	return dumpctab_v1(fname, cards, n);
}

static int loadctab_v1(char *filename, struct card_v1 *cards, int maxn)
{
	FILE *fp;
	int ret, ncard, nline, maxnl, n;

	ret = -1;
	if (!(fp = fopen(filename, "r"))) {
		apperr = AESYS;
		goto RET;
	}
	maxnl = NLINE;
	ncard = lineno = 0;
	while (ncard < maxn) {
		n = readcard_v1(fp, &cards[ncard], &nline, maxnl);
		maxnl -= nline;
		if (n == 0)
			break;
		if (n == -1) {
			lineno = NLINE - maxnl;
			goto CLS;
		}
		ncard++;
	}
	if (feof(fp))
		ret = ncard;
CLS:	fclose(fp);
RET:	return ret;
}

static int dumpctab_v1(char *filename, struct card_v1 *cards, int n)
{
	FILE *fp;
	int ret, i;

	ret = -1;
	siglock(SIGLOCK_LOCK);
	if (!(bakfname = backup_v1(filename))) {
		bakferr = apperr;
		apperr = AEBACKUP;
		goto RET;
	}
	if (!(fp = fopen(filename, "w"))) {
		apperr = AESYS;
		goto ULK;
	}
	for (i = 0; i < n; i++)
		if (i && fputs("%%\n", fp) == EOF
			|| writecard_v1(fp, &cards[i]) == -1)
			goto WERR;
	if (fclose(fp) == EOF) {
		apperr = AESYS;
		goto RET;
	}
	ret = 0;
ULK:	unlink(bakfname);
	bakfname = NULL;
RET:	siglock(SIGLOCK_UNLOCK);
	return ret;
WERR:	fclose(fp);
	goto RET;
}

static char *backup_v1(char *src)
{
	static char dst[PATHSZ];
	char *ret, buf[BUFSIZ];
	int fd[2], n;

	ret = NULL;
	strncpy(dst, "/tmp/hardv.XXXXXX", PATHSZ);
	if (dst[PATHSZ - 1]) {
		apperr = AEPATHSZ;
		goto RET;
	}
	if ((fd[1] = mkstemp(dst)) == -1) {
		apperr = AESYS;
		goto RET;
	}
	if ((fd[0] = open(src, O_RDONLY)) == -1) {
		apperr = AESYS;
		goto CL1;
	}
	while ((n = read(fd[0], buf, sizeof buf)) > 0)
		 if (write(fd[1], buf, n) != n) {
		 	apperr = AESYS;
			goto CL0;
		}
	if (n < 0) {
		apperr = AESYS;
		goto CL0;
	}
	ret = dst;
CL0:	close(fd[0]);
CL1:	close(fd[1]);
RET:	return ret;
}

static int
readcard_v1(FILE *fp, struct card_v1 *card, int *nline, int maxnl)
{
#define INCNLINE(n) do { \
	if (maxnl - (*nline) < (n)) { \
		apperr = AENLINE; \
		return -1; \
	} \
	(*nline) += (n); \
} while (0)

#define CHK_VALSZ(n) do { \
	if (val - field->val >= VALSZ - (n)) { \
		apperr = AEVALSZ; \
		return -1; \
	} \
} while (0)

#define CHK_FIELD() do { \
	if (!field) { \
		apperr = AENOKEY; \
		return -1; \
	} \
} while (0)

	char line[LINESZ], *val;
	int n, sep, nblank, ch;
	struct field_v1 *field, *i;

	memset(card, 0, sizeof *card);
	*nline = 0;
	while ((ch = fgetc(fp)) == '\n') {
		INCNLINE(1);
		card->leadnewl++;
	}
	if (ungetc(ch, fp) != ch) {
		apperr = AESYS;
		return -1;
	}
	field = NULL, val = NULL;
	while (fgets(line, sizeof line, fp)) {
		INCNLINE(1);
		n = strlen(line);
		if (line[n - 1] != '\n' && !feof(fp)) {
			apperr = AELINESZ;
			return -1;
		}
		if (line[0] == '\t') {	/* successive value line */
			CHK_FIELD();
			CHK_VALSZ(n);
			val = stpcpy(val, line);
		} else if (line[0] != '\n') {	/* new field key */
			if (card->nfield >= NFIELD) {
				apperr = AENFIELD;
				return -1;
			}
			card->nfield++;
			if (field && validfield_v1(field)) {
				(*nline)--;
				return -1;
			}
			field = &card->field[card->nfield - 1];
			val = field->val;
			sep = strcspn(line, "\n\t");
			if (sep >= KEYSZ) {
				apperr = AEKEYSZ;
				return -1;
			}
			strncpy(field->key, line, sep);
			if (validkey_v1(field->key))
				return -1;
			for (i = card->field; i != field; i++)
				if (!strcmp(field->key, i->key)) {
					apperr = AEDUPKEY;
					return -1;
				}
			if (!line[sep])
				continue;
			CHK_VALSZ(n - sep);
			val = stpcpy(val, &line[sep]);
		} else {	/* blank line */
			if (ungetc('\n', fp) != '\n') {
				apperr = AESYS;
				return -1;
			}
			nblank = 0;
			(*nline)--;
			while ((ch = fgetc(fp)) == '\n') {
				INCNLINE(1);
				nblank++;
			}
			if (ungetc(ch, fp) != ch) {
				apperr = AESYS;
				return -1;
			}
			if (ch != '\t') {
				card->trainewl = nblank;
				if (ch != EOF)
					card->trainewl--;
				break;
			}
			CHK_FIELD();
			CHK_VALSZ(nblank);
			while (nblank-- > 0)
				*val++ = '\n';
			*val = '\0';
		}
	}
	if (ferror(fp)) {
		apperr = AESYS;
		return -1;
	}
	if (card->nfield) {
		if (validfield_v1(field)) {
			if (!feof(fp))
				(*nline)--;
			return -1;
		}
		if (validcard_v1(card))
			return -1;
	}
	return card->nfield;

#undef INCNLINE
#undef CHK_FIELD
#undef CHK_VALSZ
}

static int writecard_v1(FILE *fp, struct card_v1 *card)
{
	struct field_v1 *field;
	char *key, *val;
	int i;

	/*
	for (i = 0; i < card->leadnewl; i++)
		if (fputc('\n', fp) == EOF) {
			apperr = AESYS;
			return -1;
		}
	*/
	for (i = 0; i < card->nfield; i++) {
		field = &card->field[i];
		key = field->key;
		val = field->val;
		if (fprintf(fp, "%s%s", key, val) < 0) {
			apperr = AESYS;
			return -1;
		}
		if (i < card->nfield - 1
			&& val[strlen(val) - 1] != '\n')
			if (fputc('\n', fp) == EOF) {
				apperr = AESYS;
				return -1;
			}
	}
	/*
	for (i = 0; i < card->trainewl; i++)
		if (fputc('\n', fp) == EOF) {
			apperr = AESYS;
			return -1;
		}
	*/
	return 0;
}

static char *getmod_v1(struct card_v1 *card)
{
	return getfield_v1(card, MOD);
}

static char *getques_v1(struct card_v1 *card)
{
	return getfield_v1(card, QUES);
}

static char *getansw_v1(struct card_v1 *card)
{
	return getfield_v1(card, ANSW);
}

static int getprev_v1(struct card_v1 *card, time_t *tp)
{
	return gettime_v1(card, PREV, tp);
}

static int getnext_v1(struct card_v1 *card, time_t *tp)
{
	return gettime_v1(card, NEXT, tp);
}

static int setprev_v1(struct card_v1 *card, time_t prev)
{
	return settime_v1(card, PREV, prev);
}

static int setnext_v1(struct card_v1 *card, time_t next)
{
	return settime_v1(card, NEXT, next);
}

static char *getfield_v1(struct card_v1 *card, char *key)
{
	int i;

	for (i = 0; i < card->nfield; i++)
		if (!strcmp(card->field[i].key, key))
			return card->field[i].val;
	return NULL;
}

static int gettime_v1(struct card_v1 *card, char *key, time_t *tp)
{
	struct tm buf;
	char *val;

	if (!(val = getfield_v1(card, key))) {
		*tp = 0;
		return 0;
	}
	return parsetm_v1(val, tp);
}

static int settime_v1(struct card_v1 *card, char *key, time_t t)
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

static int parsetm_v1(char *s, time_t *clock)
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

static int validkey_v1(char *key)
{
	while (*key && (isalpha(*key) || isdigit(*key) || *key == '_'))
		key++;
	if (*key)
		apperr = AEINVKEY;
	return *key;
}

static int validfield_v1(struct field_v1 *field)
{
	time_t t;

	if (!strcmp(field->key, PREV) || !strcmp(field->key, NEXT))
		return parsetm_v1(field->val, &t);
	return 0;
}

static int validcard_v1(struct card_v1 *card)
{
	time_t prev, next;
	int n;

	if (!getques_v1(card) || !getansw_v1(card)) {
		apperr = AEMFIELD;
		return -1;
	}
	n = !!getfield_v1(card, PREV) + !!getfield_v1(card, NEXT);
	if (card->nfield - n > NFIELD - 2) {
		apperr = AERSVFIELD;
		return -1;
	}
	return 0;
}

static char *normval_v1(char *s, char *buf, int n)
{
	char *sp, *bp;

	while (*s == '\n')
		s++;
	for (sp = s, bp = buf; bp != &buf[n] && *sp; sp++)
		if (*sp != '\t' || sp > s && sp[-1] != '\n')
			*bp++ = *sp;
	if (bp == &buf[n])
		return NULL;
	if (bp > buf && bp[-1] == '\n')
		bp--;
	*bp = '\0';
	return buf;
}
