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
time_t getprev(struct card *card);
time_t getnext(struct card *card);
void setprev(struct card *card, time_t prev);
void setnext(struct card *card, time_t next);
void validcard(struct card *card, char *header);
time_t parsetm(char *s);

#endif
