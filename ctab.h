#ifndef CTAB_H
#define CTAB_H

#include "card.h"

int loadctab(char *filename, struct card *cards, int maxn);
void dumpctab(char *filename, struct card *cards, int n);

#endif
