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
		goto ERR; \
	} \
	(*nline) += (n); \
} while (0)

#define CHK_VALSZ(n) do { \
	if (val - vbuf >= VALSZ - (n)) { \
		apperr = AEVALSZ; \
		goto ERR; \
	} \
} while (0)

#define CHK_FIELD() do { \
	if (!field) { \
		apperr = AENOKEY; \
		goto ERR; \
	} \
} while (0)

#define DUMPVAL() do { \
	if (!(field->val = strdup(vbuf))) { \
		apperr = AESYS; \
		*nline = keylno; \
		goto ERR; \
	} \
	if (validfield(field)) { \
		*nline = keylno; \
		goto ERR; \
	} \
} while (0)

	char line[LINESZ], vbuf[VALSZ], *val;
	struct field *field, *f;
	size_t sep, n;
	int ch, i, keylno;

	memset(card, 0, sizeof *card);
	*nline = 0;
	while ((ch = fgetc(fp)) == '\n') {
		INCNLINE(1);
		card->leadnewl++;
	}
	if (ungetc(ch, fp) != ch) {
		apperr = AESYS;
		goto ERR;
	}
	field = NULL, val = NULL;
	while (fgets(line, LINESZ, fp)) {
		INCNLINE(1);
		n = strlen(line);
		if (line[n - 1] != '\n' && !feof(fp)) {
			apperr = AELINESZ;
			goto ERR;
		}
		if (line[0] == '%') {
			/* end of card */
			if (!(card->sep = strdup(line))) {
				apperr = AESYS;
				goto ERR;
			}
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
				goto ERR;
			}
			if (field)
				DUMPVAL();
			keylno = *nline;
			field = &card->field[card->nfield++];
			val = vbuf;
			*val = '\0';
			sep = strcspn(line, "\n\t");
			if (sep >= KEYSZ) {
				apperr = AEKEYSZ;
				goto ERR;
			}
			if (!(field->key = strndup(line, sep))) {
				apperr = AESYS;
				goto ERR;
			}
			if (validkey(field->key))
				goto ERR;
			for (f = card->field; f != field; f++)
				if (!strcmp(field->key, f->key)) {
					apperr = AEDUPKEY;
					goto ERR;
				}
			if (!line[sep])
				continue;
			CHK_VALSZ(n - sep);
			val = stpcpy(val, &line[sep]);
		}
	}
	if (ferror(fp)) {
		apperr = AESYS;
		goto ERR;
	}
	if (card->nfield) {
		DUMPVAL();
		if (validcard(card)) {
			*nline = card->leadnewl + 1;
			goto ERR;
		}
	}
	return card->nfield;
ERR:	destrcard(card);
	return -1;

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
