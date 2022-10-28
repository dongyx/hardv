#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "apperr.h"
#include "applim.h"
#include "card.h"
#include "parse.h"

#define INCNLINE(n) do { \
	if (maxnl - (*nline) < (n)) { \
		apperr = AENLINE; \
		return -1; \
	} \
	(*nline) += (n); \
} while (0)

#define CHK_VALSZ(n) do { \
	if (val - field->val >= VALSZ - (n)) { \
		apperr = AEVALSZ; \
		return -1; \
	} \
} while (0)

#define CHK_FIELD() do { \
	if (!field) { \
		apperr = AENOKEY; \
		return -1; \
	} \
} while (0)

int readcard(FILE *fp, struct card *card, int *nline, int maxnl)
{
	char line[LINESZ], *val;
	int n, sep, nblank, ch;
	struct field *field, *i;

	memset(card, 0, sizeof *card);
	*nline = 0;
	while ((ch = fgetc(fp)) == '\n') {
		INCNLINE(1);
		card->leadnewl++;
	}
	if (ungetc(ch, fp) != ch) {
		apperr = AESYS;
		return -1;
	}
	field = NULL, val = NULL;
	while (fgets(line, sizeof line, fp)) {
		INCNLINE(1);
		n = strlen(line);
		if (line[n - 1] != '\n' && !feof(fp)) {
			apperr = AELINESZ;
			return -1;
		}
		if (line[0] == '\t') {	/* successive value line */
			CHK_FIELD();
			CHK_VALSZ(n);
			val = stpcpy(val, line);
		} else if (line[0] != '\n') {	/* new field key */
			if (card->nfield >= NFIELD) {
				apperr = AENFIELD;
				return -1;
			}
			card->nfield++;
			if (field && validfield(field)) {
				(*nline)--;
				return -1;
			}
			field = &card->field[card->nfield - 1];
			val = field->val;
			sep = strcspn(line, "\n\t");
			if (sep >= KEYSZ) {
				apperr = AEKEYSZ;
				return -1;
			}
			strncpy(field->key, line, sep);
			if (validkey(field->key))
				return -1;
			for (i = card->field; i != field; i++)
				if (!strcmp(field->key, i->key)) {
					apperr = AEDUPKEY;
					return -1;
				}
			if (!line[sep])
				continue;
			CHK_VALSZ(n - sep);
			val = stpcpy(val, &line[sep]);
		} else {	/* blank line */
			if (ungetc('\n', fp) != '\n') {
				apperr = AESYS;
				return -1;
			}
			nblank = 0;
			(*nline)--;
			while ((ch = fgetc(fp)) == '\n') {
				INCNLINE(1);
				nblank++;
			}
			if (ungetc(ch, fp) != ch) {
				apperr = AESYS;
				return -1;
			}
			if (ch != '\t') {
				card->trainewl = nblank;
				if (ch != EOF)
					card->trainewl--;
				break;
			}
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
		if (validfield(field)) {
			if (!feof(fp))
				(*nline)--;
			return -1;
		}
		if (validcard(card))
			return -1;
	}
	return card->nfield;
}

int writecard(FILE *fp, struct card *card)
{
	struct field *field;
	char *key, *val;
	int i;

	for (i = 0; i < card->leadnewl; i++)
		if (fputc('\n', fp) == EOF) {
			apperr = AESYS;
			return -1;
		}
	for (i = 0; i < card->nfield; i++) {
		field = &card->field[i];
		key = field->key;
		val = field->val;
		if (fprintf(fp, "%s%s", key, val) < 0) {
			apperr = AESYS;
			return -1;
		}
		if (i < card->nfield - 1
			&& val[strlen(val) - 1] != '\n')
			if (fputc('\n', fp) == EOF) {
				apperr = AESYS;
				return -1;
			}
	}
	for (i = 0; i < card->trainewl; i++)
		if (fputc('\n', fp) == EOF) {
			apperr = AESYS;
			return -1;
		}
	return 0;
}
