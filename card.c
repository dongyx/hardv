#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include "card.h"
#define eret(e, r) do { carderr = (e); return (r); } while (0)
#define egoto(e, l) do { carderr = (e); goto l; } while (0)

static int saveattrv(struct attr *attr, char *buf);

int carderr;

int cardread(FILE *fp, struct card *card)
{
	struct attr *attr;
	char line[BUFSIZ], vbuf[BUFSIZ], *vp;
	int ch, n, sep;

	attr = NULL;
	*(vp = vbuf) = '\0';
	while (fgets(line, sizeof line, fp)) {
		n = strlen(line);
		if (line[n - 1] != '\n' && !feof(fp))
			egoto(CARD_ELINE, ERR);
		if (line[0] != '\t' && line[0] != '\n') {
			if (attr) {
				if (saveattrv(attr, vbuf) == -1)
					goto ERR;
				attr->next = malloc(sizeof *attr);
				if (!attr->next)
					egoto(CARD_ESYSL, ERR);
				attr = attr->next;
			} else {
				if (!(attr = malloc(sizeof *attr)))
					egoto(CARD_ESYSL, ERR);
				card->attr = attr;
			}
			memset(attr, 0, sizeof *attr);
			sep = strcspn(line, "\t");
			line[sep++] = '\0';
			if (!(attr->key = strdup(line)))
				egoto(CARD_ESYSL, ERR);
			if (n - sep >= sizeof vbuf)
				egoto(CARD_ESIZE, ERR);
			vp = stpcpy(vbuf, &line[sep]);
		} else if (line[0] == '\t') {
			if (!attr)
				egoto(CARD_ENOKEY, ERR);
			if ((vp - vbuf) + n > sizeof vbuf)
				egoto(CARD_ESIZE, ERR);
			vp = stpcpy(vp, &line[1]);
		} else {
			n = 1;
			while ((ch = fgetc(fp)) == '\n')
				n++;
			if (ch != EOF)
				ungetc(ch, fp);
			if (ch == '\t') {
				if (!attr)
					egoto(CARD_ENOKEY, ERR);
				while (n--) {
					if ((vp - vbuf) + 1
						>= sizeof vbuf)
						egoto(CARD_ESIZE, ERR);
					*vp++ = '\n';
				}
			} else if (attr)
				break;
		}
	}
	if (ferror(fp))
		egoto(CARD_ESYSL, ERR);
	if (attr && saveattrv(attr, vbuf) == -1)
		goto ERR;
	return !!attr;
ERR:
	carddestr(card);
	return -1;
}

int cardwrite(FILE *fp, struct card *card)
{
	struct attr *attr;
	char *i;

	for (attr = card->attr; attr; attr = attr->next) {
		if (fprintf(fp, "%s", attr->key) == -1)
			eret(CARD_ESYSL, -1);
		for (i = attr->val; *i; i++) {
			if (i == attr->val || *(i - 1) == '\n')
				if (fputc('\t', fp) == EOF)
					eret(CARD_ESYSL, -1);
			if (fputc(*i, fp) == EOF)
				eret(CARD_ESYSL, -1);
		}
		if (!*attr->val || *(i - 1) != '\n')
			if (fputc('\n', fp) == EOF)
				eret(CARD_ESYSL, -1);
	}
	return 0;
}

void carddestr(struct card *card)
{
	struct attr *attr;

	while ((attr = card->attr)) {
		free(attr->key);
		free(attr->val);
		card->attr = attr->next;
		free(attr);
	}
}

char *cardget(struct card *card, char *key)
{
	struct attr *i;

	for (i = card->attr; i; i = i->next)
		if (!strcmp(i->key, key))
			return i->val;
	return NULL;
}

int cardset(struct card *card, char *key, char *val, int creat)
{
	struct attr *i, *j;

	for (i = card->attr, j = NULL; i; j = i, i = i->next)
		if (!strcmp(i->key, key))
			break;
	if (creat && !i) {
		if (!(i = malloc(sizeof *i)))
			eret(CARD_ESYSL, -1);
		memset(i, 0, sizeof *i);
		if (!(i->key = strdup(key)))
			eret(CARD_ESYSL, -1);
		if (j)
			j->next = i;
		else
			card->attr = i;
	}
	else if (!i)
		eret(CARD_ENOKEY, -1);
	free(i->val);
	if (!(i->val = strdup(val)))
		eret(CARD_ESYSL, -1);
	return 0;
}

char *cardestr(void)
{
	static char *emsg[] = {
		/* CARD_ESYSL */	NULL,
		/* CARD_ELINE */	"line too long",
		/* CARD_ESIZE */	"content too large",
		/* CARD_ENOKEY */	"expect an attribute key"
	};

	if (carderr < 0 || carderr >= sizeof emsg / sizeof *emsg)
		return "undefined error";
	return carderr ? emsg[carderr] : strerror(errno);
}

void cardperr(char *s)
{
	if (s)
		fprintf(stderr, "%s: %s\n", s, cardestr());
	else
		fprintf(stderr, "%s\n", cardestr());
}

static int saveattrv(struct attr *attr, char *buf)
{
	if (!(attr->val = strdup(buf)))
		eret(CARD_ESYSL, -1);
	return 0;
}
