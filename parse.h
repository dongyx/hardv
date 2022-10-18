#ifndef PARSE_H
#define PARSE_H

#include "card.h"

int readcard(FILE *fp, struct card *card, int *nline, int maxnl);
int writecard(FILE *fp, struct card *card);

#endif
