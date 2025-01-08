#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "hardv.h"

static char *path;
static FILE *fp;
static int lineno;

static void
invalid(char *msg)
{
	err("%s, line %d: %s\n", path, lineno, msg);
	exit(1);
}

static void
feedline(void)
{
	if (lineno == INT_MAX) invalid("too many lines");
	lineno++;
}

static char *
parsehead(void)
{
	char ch, buf[MAXN], *s;
	int n;

	n = 0;
	while (isspace((ch=getc(fp)))) {
		if (n+1 == MAXN) err("head too large");
		if ((buf[n++]=ch) == '\n') feedline();
	}
	ungetc(ch, fp);
	buf[n] = '\0';
	if (n>0 && buf[n-1]!='\n' && ch!=EOF) invalid("unexpected space");
	if (n == 0) return NULL;
	if (!(s=strdup(buf))) syserr();
	return s;
}

static char *
parsekey(void)
{
	char buf[MAXN], ch, *s;
	int n;

	n = 0;
	while ((ch=getc(fp)) != EOF) {
		if (!strchr(KCHAR, ch)) break;
		if (n+1 == MAXN) err("key too large");
		if ((buf[n++]=ch) == '\n') feedline();
	}
	ungetc(ch, fp);
	buf[n] = '\0';
	if (n == 0) return NULL;
	if (!(s=strdup(buf))) syserr();
	return s;
}

static char *
parseval(void)
{
	char buf[MAXN], ch, ahead, *s;
	int n;

	ch = ungetc(getc(fp), fp);
	if (ch != '\t' && ch != '\n') invalid("expect tab or newline");
	n = 0;
	while ((ch=getc(fp)) != EOF) {
		if (n+1 == MAXN) err("value too large");
		if ((buf[n++]=ch) == '\n') feedline();
		ahead = ungetc(getc(fp), fp);
		if (ch == '\n' && ahead != '\t' && ahead != '\n')
			break;
	}
	buf[n] = '\0';
	if (n == 0) return NULL;
	if (!(s=strdup(buf))) syserr();
	return s;
}

static struct field *
parsefield(void)
{
	struct field *p, *q;
	char *key, *val;

	p = NULL;
	while ((key=parsekey()) != NULL) {
		val = parseval();
		if ((q=malloc(sizeof *q)) == NULL) syserr();
		q->key = key;
		q->val = val;
		q->next = p;
		p = q;
	}
	return p;
}

static char *
parsetail(void)
{
	int ch, cap, n;
	char buf[MAXN], *s;

	ch = ungetc(getc(fp), fp);
	if (ch == EOF) return NULL;
	if (ch != '%') invalid("expect '%'");
	n = 0;
	while ((ch=getc(fp)) != EOF) {
		if (n+1 == MAXN) err("tail too large");
		if ((buf[n++]=ch) == '\n') {
			feedline();
			break;
		}
	}
	buf[n] = '\0';
	if (n == 0) return NULL;
	if (!(s=strdup(buf))) syserr();
	return s;
}

static struct field *
reverse(struct field *f)
{
	struct field *r, *s;

	r = NULL;
	while (f) {
		s = f->next;
		f->next = r;
		r = f;
		f = s;
	}
	return r;
}

void
parseinit(char *file)
{
	path = file;
	lineno = 1;
	fp = fopen(path, "r");
	if (!fp) syserr();
}

void
parsedone(void)
{
	fclose(fp);
}

struct card *
parsecard(struct card *dst)
{
	struct field *f;

	dst->head = parsehead();
	while ((f=parsefield()) != NULL) {
		f->next = dst->field;
		dst->field = f;
	}
	dst->field = reverse(dst->field);
	dst->tail = parsetail();
	if (!dst->head && !dst->field && !dst->tail) return NULL;
	return dst;
}
