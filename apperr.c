#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "apperr.h"

int apperr;

char *aestr(int eno)
{
	static char *msg[] = {
		NULL,
		NULL,
		"too many fields",
		"mandaroty field not found",
		"invalid time format",
		"field key too large",
		"feild value too large",
		"filed key is expected",
		"line too large",
		"field value is expected",
		"duplicated key",
		"too many lines",
		"too many cards",
		"invalid function arguments",
		"file name too large",
		"no space for reserved fields",
		"fail to create backup file",
		"invalid field key"
	};

	if (eno == AESYS)
		return strerror(errno);
	if (eno < 2 || eno >= sizeof msg / sizeof msg[0])
		return "undefined application error";
	return msg[eno];
}

void aeprint(char *head)
{
	if (head)
		fprintf(stderr, "%s: %s\n", head, aestr(apperr));
	else
		fprintf(stderr, " %s\n", aestr(apperr));
}
