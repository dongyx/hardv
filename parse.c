#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "card.h"
#include "parse.h"

#define CHK_FIELD() do { \
	if (!field) { \
		fprintf(stderr, "%s, line %d: " \
			"field key is expected\n", \
			ehaed, lineno); \
		exit(1); \
	} \
} while (0)

#define CHK_VALSZ(n) do { \
	if (val - field->val + (n) >= VALSZ) { \
		fprintf(stderr, "%s, line %d: " \
			"field value must be less than" \
			" %d bytes\n", \
			ehaed, lineno, VALSZ); \
		exit(1); \
	} \
} while (0)

#define TRIM() do { \
	if (field && val != field->val && val[-1] == '\n') \
		val[-1] = '\0'; \
} while (0)

int lineno;

int readcard(FILE *fp, struct card *card, char *ehaed)
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
			fprintf(stderr, "%s, line %d: "
				"line must be less than %d bytes\n",
				ehaed, lineno, LINESZ);
			exit(1);
		}
		if (line[0] == '\t') {	/* successive value line */
			CHK_FIELD();
			CHK_VALSZ(n - 1);
			val = stpcpy(val, &line[1]);
		} else if (line[0] != '\n') {	/* new field key */
			if (++card->nfield > NFIELD) {
				fprintf(stderr, "%s, line %d: "
				"fields of a card can't exceed %d"
				"\n",
				ehaed, lineno, NFIELD);
				exit(1);
			}
			TRIM();
			field = &card->field[card->nfield - 1];
			val = field->val;
			sep = strcspn(line, "\t");
			if (!line[sep]) {
				fprintf(stderr, "%s, line %d: "
					"field value is expected\n",
					ehaed, lineno);
				exit(1);
			}
			line[sep++] = '\0';
			if (sep > KEYSZ) {
				fprintf(stderr, "%s, line %d: "
				"field key must be less than %d\n",
				ehaed, lineno, KEYSZ);
				exit(1);
			}
			strcpy(field->key, line);
			for (i = card->field; i != field; i++)
				if (!strcmp(field->key, i->key)) {
					fprintf(stderr, "%s, line %d: "
					"duplicated key: %s\n",
					ehaed, lineno, i->key);
					exit(1);
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
				fprintf(stderr, "%s, line %d: "
					"number of continuous blank "
					"lines can't exceed %d\n",
					ehaed, lineno, NBLANK);
				exit(1);
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
		perror(NULL);
		exit(1);
	}
	if (card->nfield) {
		TRIM();
		validcard(card, ehaed);
	}
	return card->nfield;
}

void writecard(FILE *fp, struct card *card, char *ehaed)
{
	char *j;
	int i;

	for (i = 0; i < card->nfield; i++) {
		if (fprintf(fp, "%s\t", card->field[i].key) < 0) {
			perror(ehaed);
			exit(1);
		}
		for (j = card->field[i].val; *j; j++) {
			if (j != card->field[i].val && j[-1] == '\n')
				if (fputc('\t', fp) == EOF) {
					perror(ehaed);
					exit(1);
				}
			if (fputc(*j, fp) == EOF) {
				perror(ehaed);
				exit(1);
			}
		}
		if (fputc('\n', fp) == EOF) {
			perror(ehaed);
			exit(1);
		}
	}
}
