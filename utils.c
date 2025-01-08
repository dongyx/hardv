#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "hardv.h"

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
