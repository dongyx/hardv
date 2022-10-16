#ifndef PARSE_H
#define PARSE_H

#include "card.h"
#define NBLANK 65536
#define LINESZ 1024

extern int inparse, lineno;

int readcard(FILE *fp, struct card *card);
int writecard(FILE *fp, struct card *card);

#endif
