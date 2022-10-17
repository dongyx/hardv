#ifndef APPERR_H
#define APPERR_H

extern int apperr;

char *aestr(void);
void aeprint(char *head);

enum {
	AESYS,	/* check errno */
	AENFIELD,	/* too many fields */
	AEMFIELD,	/* mandatory field not found */
	AETIMEF,	/* invalid time format */
	AEKEYSZ,	/* field key too large */
	AEVALSZ,	/* feild val too large */
	AENOKEY,	/* filed key is expected */
	AELINESZ,	/* line too large */
	AENOVAL,	/* field value is expected */
	AEDUPKEY,	/* duplicated key */
	AENNEWL,	/* too many consecutive blank lines */
	AENCARD,	/* too many cards in an input file */
	AEINVAL	/* invalid function arguments */
};

#endif
