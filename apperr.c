#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "apperr.h"

int apperr;

char *aestr(void)
{
	static char *msg[] = {
		NULL,
		"too many fields",
		"required fields not found",
		"invalid time format",
		"field key too large",
		"feild value too large",
		"filed key is expected",
		"line too large",
		"field value is expected",
		"duplicated key",
		"too many consecutive blank lines",
		"too many cards in an input file",
		"invalid function arguments"
	};

	if (!apperr)
		return strerror(errno);
	if (apperr < 0 || apperr >= sizeof msg / sizeof msg[0])
		return "undefined application error";
	return msg[apperr];
}

void aeprint(char *head)
{
	if (head)
		fprintf(stderr, "%s: %s\n", head, aestr());
	else
		fprintf(stderr, " %s\n", aestr());
}
