#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "apperr.h"
#include "card.h"
#include "parse.h"

#define CHK_FIELD() do { \
	if (!field) { \
		apperr = AENOKEY; \
		return -1; \
	} \
} while (0)

#define CHK_VALSZ(n) do { \
	if (val - field->val + (n) >= VALSZ) { \
		apperr = AEVALSZ; \
		return -1; \
	} \
} while (0)

#define TRIM() do { \
	if (field && val != field->val && val[-1] == '\n') \
		val[-1] = '\0'; \
} while (0)

int lineno;

int readcard(FILE *fp, struct card *card)
{
	char line[LINESZ], *val;
	int n, sep, nblank, ch;
	struct field *field, *i;

	while ((ch = fgetc(fp)) == '\n')
		;
	if (ch != EOF)
		ungetc(ch, fp);
	field = NULL, val = NULL;
	memset(card, 0, sizeof *card);
	while (fgets(line, sizeof line, fp)) {
		lineno++;
		n = strlen(line);
		if (line[n - 1] != '\n' && !feof(fp)) {
			apperr = AELINESZ;
			return -1;
		}
		if (line[0] == '\t') {	/* successive value line */
			CHK_FIELD();
			CHK_VALSZ(n - 1);
			val = stpcpy(val, &line[1]);
		} else if (line[0] != '\n') {	/* new field key */
			if (++card->nfield > NFIELD) {
				apperr = AENFIELD;
				return -1;
			}
			TRIM();
			field = &card->field[card->nfield - 1];
			val = field->val;
			sep = strcspn(line, "\t");
			if (!line[sep]) {
				apperr = AENOVAL;
				return -1;
			}
			line[sep++] = '\0';
			if (sep > KEYSZ) {
				apperr = AEKEYSZ;
				return -1;
			}
			strcpy(field->key, line);
			for (i = card->field; i != field; i++)
				if (!strcmp(field->key, i->key)) {
					apperr = AEDUPKEY;
					return -1;
				}
			CHK_VALSZ(n - sep);
			val = stpcpy(val, &line[sep]);
		} else {	/* blank line */
			ungetc('\n', fp);
			nblank = 0;
			while (nblank <= NBLANK
				&& (ch = fgetc(fp)) == '\n')
				nblank++;
			if (nblank > NBLANK) {
				apperr = AENNEWL;
				return -1;
			}
			if (ch != EOF)
				ungetc(ch, fp);
			if (ch != '\t')
				break;
			CHK_FIELD();
			CHK_VALSZ(nblank);
			while (nblank-- > 0)
				*val++ = '\n';
			*val = '\0';
		}
	}
	if (ferror(fp)) {
		apperr = AESYS;
		return -1;
	}
	if (card->nfield) {
		TRIM();
		if (validcard(card) == -1)
			return -2;
	}
	return card->nfield;
}

int writecard(FILE *fp, struct card *card)
{
	char *j;
	int i;

	for (i = 0; i < card->nfield; i++) {
		if (fprintf(fp, "%s\t", card->field[i].key) < 0) {
			apperr = AESYS;
			return -1;
		}
		for (j = card->field[i].val; *j; j++) {
			if (j != card->field[i].val && j[-1] == '\n')
				if (fputc('\t', fp) == EOF) {
					apperr = AESYS;
					return -1;
				}
			if (fputc(*j, fp) == EOF) {
				apperr = AESYS;
				return -1;
			}
		}
		if (fputc('\n', fp) == EOF) {
			apperr = AESYS;
			return -1;
		}
	}
	return 0;
}
