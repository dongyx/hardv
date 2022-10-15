#ifndef LEARN_H
#define LEARN_H

#include "card.h"

struct learnopt {
	int exact;
};

void learn(char *filename, int now, struct learnopt *opt);

#endif
