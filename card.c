#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include "card.h"

static int readfront(FILE *fp, char *buf, int maxn);
static int readattrs(FILE *fp, struct attr *buf, int maxn);
static int readback(FILE *fp, char *buf, int maxn);

int readcard(FILE *fp, struct card *card)
{
	char buf[BUFSIZ];

	if (feof(fp))
		return 0;
	if (readfront(fp, buf, sizeof buf) == -1)
		return -1;
	if (!(card->front = strdup(buf)))
		return -1;
	if ((card->nattr = readattrs(fp, card->attr,
		sizeof card->attr / sizeof card->attr[0])) == -1)
		return -1;
	if (readback(fp, buf, sizeof buf) == -1)
		return -1;
	if (!(card->back = strdup(buf)))
		return -1;
	return 1;
}

static int readfront(FILE *fp, char *buf, int maxn)
{
	char *bufp;
	int n;

	bufp = buf, n = 0;
	while (maxn > 0 && fgets(bufp, maxn, fp)) {
		n = strlen(bufp);
		if (bufp[n - 1] != '\n') {
			errno = ERANGE;
			return -1;
		}
		if (!strcmp("-\n", bufp)) {
			*bufp = '\0';
			return bufp - buf;
		}
		bufp += n, maxn -= n;
	}
	if (maxn <= 0)
		errno = ERANGE;
	else
		errno = ferror(fp) ? EIO : EINVAL;
	return -1;
}

static int readattrs(FILE *fp, struct attr *buf, int maxn)
{
	char line[BUFSIZ];
	int n, i;

	n = 0;
	while (maxn-- > 0 && fgets(line, sizeof line, fp)) {
		if (line[strlen(line) - 1] != '\n') {
			errno = ERANGE;
			return -1;
		}
		line[strcspn(line, "\n")] = '\0';
		if (!strcmp("-", line))
			return n;
		if (line[i = strcspn(line, ":")]) {
			line[i] = '\0';
			buf[n].key = line;
			buf[n].val = &line[i + 1];
			while (isspace(*buf[n].key))
				buf[n].key++;
			while (isspace(*buf[n].val))
				buf[n].val++;
			if (!(buf[n].key = strdup(buf[n].key)))
				return -1;
			if (!(buf[n].val = strdup(buf[n].val)))
				return -1;
			n++;
		} else {
			errno = EINVAL;
			return -1;
		}
	}
	if (maxn <= 0)
		errno = ERANGE;
	else
		errno = ferror(fp) ? EIO : EINVAL;
	return -1;
}

static int readback(FILE *fp, char *buf, int maxn)
{
	char *bufp;
	int n;

	bufp = buf, n = 0;
	while (maxn > 0 && fgets(bufp, maxn, fp)) {
		n = strlen(bufp);
		if (bufp[n - 1] != '\n' && !feof(fp)) {
			errno = ERANGE;
			return -1;
		}
		if (!strcmp("---", bufp)
			|| !strcmp("---\n", bufp)) {
			*bufp = '\0';
			return bufp - buf;
		}
		bufp += n, maxn -= n;
	}
	if (feof(fp))
		return bufp - buf;
	if (maxn <= 0)
		errno = ERANGE;
	else
		errno = ferror(fp) ? EIO : EINVAL;
	return -1;
}
