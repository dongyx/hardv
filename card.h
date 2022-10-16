#ifndef CARD_H
#define	CARD_H

#include <time.h>
#define KEYSZ 8
#define VALSZ 1024
#define NFIELD 8

struct field {
	char key[KEYSZ];
	char val[VALSZ];
};

struct card {
	int nfield;
	struct field field[NFIELD];
};

char *getfront(struct card *card);
char *getback(struct card *card);
int getprev(struct card *card, time_t *tp);
int getnext(struct card *card, time_t *tp);
int setprev(struct card *card, time_t prev);
int setnext(struct card *card, time_t next);
int validcard(struct card *card);
int parsetm(char *s, time_t *tp);

#endif
