#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "apperr.h"
#include "applim.h"
#include "card.h"
#include "parse.h"

int readcard(FILE *fp, struct card *card, int *nline, int maxnl)
{
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

#define BACK_LINENO() do { \
	while (val > field->val && *--val == '\n') \
		(*nline)--; \
} while (0)

	char line[LINESZ], *val;
	int n, sep, ch, fsz;
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
		if (line[0] == '%') {
			/* end of card */
			strncpy(card->sep, line, LINESZ);
			break;
		} else if (line[0] == '\t' || line[0] == '\n') {
			/* successive value line */
			CHK_FIELD();
			CHK_VALSZ(n);
			val = stpcpy(val, line);
		} else {
			/* new field */
			if (card->nfield >= NFIELD) {
				apperr = AENFIELD;
				return -1;
			}
			card->nfield++;
			if (field && validfield(field)) {
				BACK_LINENO();
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
		}
	}
	if (ferror(fp)) {
		apperr = AESYS;
		return -1;
	}
	if (card->nfield) {
		if (validfield(field)) {
			if (!feof(fp))
				BACK_LINENO();
			return -1;
		}
		if (validcard(card))
			return -1;
	}
	return card->nfield;

#undef INCNLINE
#undef CHK_FIELD
#undef CHK_VALSZ
#undef BACK_LINENO
}

int writecard(FILE *fp, struct card *card)
{
	struct field *field;
	int i;

	for (i = 0; i < card->leadnewl; i++)
		if (fputc('\n', fp) == EOF) {
			apperr = AESYS;
			return -1;
		}
	for (i = 0; i < card->nfield; i++) {
		field = &card->field[i];
		if (fprintf(fp, "%s%s", field->key, field->val) < 0) {
			apperr = AESYS;
			return -1;
		}
	}
	return 0;
}
