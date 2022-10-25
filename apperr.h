#ifndef APPERR_H
#define APPERR_H

#define ECHK(expr, e, r) do { \
	if ((expr)) { \
		apperr = (e); \
		r; \
	} \
} while (0)

extern int apperr;

char *aestr(int eno);
void aeprint(char *head);

enum {
	AESYS,	/* system error */
	AENFIELD,	/* too many fields */
	AEMFIELD,	/* mandatory field not found */
	AETIMEF,	/* invalid time format */
	AEKEYSZ,	/* field key too large */
	AEVALSZ,	/* feild val too large */
	AENOKEY,	/* filed key is expected */
	AELINESZ,	/* line too large */
	AENOVAL,	/* field value is expected */
	AEDUPKEY,	/* duplicated key */
	AENLINE,	/* too many lines */
	AENCARD,	/* too many cards */
	AEINVAL,	/* invalid function arguments */
	AEPATHSZ,	/* file name too large */
	AERSVFIELD,	/* no space for reserved fields */
	AEBACKUP,	/* fail to create backup file */
	AEINVKEY	/* invalid file key */
};

#endif
