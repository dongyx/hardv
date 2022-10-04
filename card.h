#ifndef CARD_H
#define CARD_H

#include <stdio.h>
#define MAXATTR 8

struct attr {
	char *key;
	char *val;
};

struct card {
	char *front;
	char *back;
	struct attr attr[MAXATTR];
	int nattr;
	long prev, next;
};

int readcard(FILE *fp, struct card *card);

#endif
