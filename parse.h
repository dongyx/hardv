#ifndef PARSE_H
#define PARSE_H

#include "card.h"
#define NBLANK 65536
#define LINESZ 1024

extern int lineno;

int readcard(FILE *fp, char *filename, struct card *card);
void writecard(FILE *fp, char *filename, struct card *card);

#endif
