#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "hardv.h"

static void
chtz(char *tz)
{
	if (tz)
		setenv("TZ", tz, 1);
	else
		unsetenv("TZ");
	tzset();
}

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
	time_t secs;
	struct tm tm;
	char tz[MAXN], *oldtz, *p;

	if (!buf) return 0;
	if (!(p=strptime(buf, " %Y-%m-%d %H:%M:%S", &tm)))
		err("invalid time: %s", buf);
	while (isspace((unsigned char)*p)) p++;
	strcpy(tz, p);
	if (tz[0] == '+') tz[0] = '-';
	else if (tz[0] == '-') tz[0] = '+';
	else err("invalid timezone: %s", buf);
	oldtz = getenv("TZ");
	chtz(tz);
	secs = mktime(&tm);
	if (secs < 0) err("too early: %s", buf);
	chtz(oldtz);
	return secs;
}
