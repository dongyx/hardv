#ifndef CTAB_H
#define CTAB_H

#include "card.h"

extern char *bakfname;
extern int lineno;

/* Load at most `maxn` cards from the file `filename` into `cards`
 * 
 * Upon successful completion the number of loaded cards is returned.
 * Otherwise, -1 is returned and the global variable `apperr` is set
 * to indicate the error.
 *
 * If the error occurred during parsing, the global variable `lineno`
 * is also set to the line number where the error occurred.
 * Otherwise, `lineno` is set to 0.
 */
int loadctab(char *filename, struct card *cards, int maxn);

/* Dump `n` cards in `cards` to the file `filename`.
 * 
 * Upon successful completion 0 is returned.
 * Otherwise, -1 is returned and the global variable `apperr` is set
 * to indicate the error.
 *
 * If the error occurred but the backup file was created, the global
 * variable `bakfname` is also set to the path of the backup file.
 * Otherwise, `bakfname` is set to `NULL`.
 */
int dumpctab(char *filename, struct card *cards, int n);

#endif
