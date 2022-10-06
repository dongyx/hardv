#ifndef CARD_H
#define CARD_H

#include <stdio.h>
#include <time.h>

enum {
	CARD_ESYSL,
	CARD_ELINE,
	CARD_ESIZE,
	CARD_ENOKEY
};

struct attr {
	char *key;
	char *val;
	struct attr *next;
};

struct card {
	struct attr *attr;
};

extern int carderr;

int cardread(FILE *fp, struct card *card);
int cardwrite(FILE *fp, struct card *card);
void carddestr(struct card *card);
char *cardget(struct card *card, char *key);
int cardset(struct card *card, char *key, char *val, int creat);
char *cardestr(void);
void cardperr(char *s);

#endif
