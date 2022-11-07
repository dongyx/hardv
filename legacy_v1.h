#ifndef CARD_V1_H
#define	CARD_V1_H

#include "applim.h"

struct field_v1 {
	char key[KEYSZ];
	char val[VALSZ];
};

struct card_v1 {
	int nfield, leadnewl, trainewl;
	struct field_v1 field[NFIELD];
};

int conv_1to2(char *fname);

#endif
