#ifndef LEARN_H
#define LEARN_H

#include "ctab.h"
#include "card.h"

struct learnopt {
	int any;
	int exact;
	int rand;
	int maxn;
};

/* Load cards in `filename` with the quiz time `now` and options `opt`.
 * 
 * Upon successful completion 0 is returned.
 * Otherwise, -1 is returned and the global variable `apperr` is set
 * to indicate the error.
 *
 * If the error occurred during parsing, the global variable `lineno`
 * is also set to the line number where the error occurred.
 * Otherwise, `lineno` is set to 0.
 *
 * If the error is failing to create the backup file, `apperr` is set
 * to `AEBACKUP` and the global variable `bakferr` is set to indicate
 * the detailed reason.
 *
 * If the error occurred during dumping but the backup file was created,
 * the global variable `bakfname` is also set to the path of the
 * backup file. Otherwise, `bakfname` is set to `NULL`.
 */
int learn(char *filename, int now, struct learnopt *opt);

#endif
