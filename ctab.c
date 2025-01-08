#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "hardv.h"

struct card ctab[MAXN];
int ncards;

static void
dump(FILE *fp, struct card *card)
{
	struct field *p;

	if (card->head && fputs(card->head, fp) == EOF) syserr();
	for (p=card->field; p; p=p->next) {
		if (fputs(p->key, fp) == EOF) syserr();
		if (fputs(p->val, fp) == EOF) syserr();
	}
	if (card->tail && fputs(card->tail, fp) == EOF) syserr();
}

void
ctabload(char *file)
{
	struct card buf;

	parseinit(file);
	while (parsecard(&buf)) {
		if (ncards == MAXN) err("too many cards");
		ctab[ncards++] = buf;
	}
	parsedone();
}

void
ctabdump(char *file)
{
	int i;
	FILE *fp;

	fp = fopen(file, "w");
	if (!fp) syserr();
	for (i=0; i<ncards; i++)
		if (strcmp(ctab[i].file, file) == 0)
			dump(fp, &ctab[i]);
	if (fflush(fp) == EOF) syserr();
	if (fsync(fileno(fp)) == -1) syserr();
	if (fclose(fp) == EOF) syserr();
}
