#ifndef CARD_H
#define	CARD_H

#include <time.h>
#include "applim.h"

struct field {
	char *key;
	char *val;
	struct field *next;
};

struct card {
	int leadnewl;
	struct field *field;
	char *sep;
};

int readcard(FILE *fp, struct card *card, int *nline, int maxnl);
int writecard(FILE *fp, struct card *card);
void destrcard(struct card *card);
char *getmod(struct card *card);
char *getques(struct card *card);
char *getansw(struct card *card);
time_t getprev(struct card *card);
time_t getnext(struct card *card);
int setprev(struct card *card, time_t prev);
int setnext(struct card *card, time_t next);
int parsetm(char *s, time_t *tp);
char *normval(char *s, char *buf, int n);

#endif
