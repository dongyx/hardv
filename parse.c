#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include "hardv.h"

static char *path;
static FILE *fp;
static size_t lineno;

static void
invalid(char *msg)
{
	err("%s, line %zu: %s", path, lineno, msg);
}

static char *
parsehead(void)
{
	char buf[MAXN], *s;
	int ch;
	size_t n;

	n = 0;
	while (isspace((unsigned char)(ch=getc(fp)))) {
		if (n+1 == MAXN) err("head too large");
		if ((buf[n++]=ch) == '\n') lineno++;
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
	char buf[MAXN], *s;
	int ch;
	size_t n;

	n = 0;
	while ((ch=getc(fp)) != EOF) {
		if (!strchr(KCHAR, ch)) break;
		if (n+1 == MAXN) err("key too large");
		if ((buf[n++]=ch) == '\n') lineno++;
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
	char buf[MAXN], ahead, *s;
	int ch;
	size_t n;

	ch = ungetc(getc(fp), fp);
	if (ch == EOF) return NULL;
	if (ch != '\t' && ch != '\n') invalid("expect tab or newline");
	n = 0;
	while ((ch=getc(fp)) != EOF) {
		if (n+1 == MAXN) err("value too large");
		if ((buf[n++]=ch) == '\n') lineno++;
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
	struct field *p;
	char *key, *val;

	if (!(key=parsekey())) return NULL;
	val = parseval();
	if ((p=malloc(sizeof *p)) == NULL) syserr();
	p->key = key;
	p->val = val;
	p->next = NULL;
	return p;
}

static char *
parsetail(void)
{
	size_t n;
	int ch;
	char buf[MAXN], *s;

	ch = ungetc(getc(fp), fp);
	if (ch == EOF) return NULL;
	if (ch != '%') invalid("expect '%'");
	n = 0;
	while ((ch=getc(fp)) != EOF) {
		if (n+1 == MAXN) err("tail too large");
		if ((buf[n++]=ch) == '\n') {
			lineno++;
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

	if (!(dst->file=strdup(path))) syserr();
	dst->head = parsehead();
	dst->field = NULL;
	while ((f=parsefield()) != NULL) {
		f->next = dst->field;
		dst->field = f;
	}
	dst->field = reverse(dst->field);
	dst->tail = parsetail();
	if (!dst->head && !dst->field && !dst->tail) return NULL;
	return dst;
}
