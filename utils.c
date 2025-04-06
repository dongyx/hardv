#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "hardv.h"

char *strptime(const char *, const char *, struct tm *);
time_t timegm(struct tm *);

void
syserr()
{
	perror(progname);
	exit(-1);
}

void
err(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "%s: ", progname);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
	exit(-1);
}

time_t
elapsecs(char *buf)
{
	unsigned hr, mi;
	char sg, *s;
	struct tm tm;
	time_t ck;

	s = buf;
	if (!s) return 0;
	while (isspace((unsigned char)*s)) s++;
	if (!(s=strptime(s, "%Y-%m-%d %H:%M:%S", &tm)))
		err("invalid time: %s", buf);
	if (sscanf(s," %c%2u%2u",&sg,&hr,&mi) != 3)
		err("invalid time: %s", buf);
	ck = timegm(&tm);
	if (sg=='+') ck -= mi*60+hr*3600LU;
	else if (sg=='-') ck += mi*60+hr*3600LU;
	else err("invalid time: %s", buf);
	return ck;
}
