#ifndef CTAB_H
#define CTAB_H

#include "card.h"

extern int cardno;

int loadctab(char *filename, struct card *cards, int maxn);
int dumpctab(char *filename, struct card *cards, int n);

#endif
