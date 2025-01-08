#define MAXN 65536
#define KCHAR "abcdefghijklmnopqrstuvwxyABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_" 
#define SHELL	"/bin/sh"
#define DAY	(60*60*24)
#define PATHSZ	32767
#define NLINE	INT_MAX
#define LINESZ	32767
#define NCARD	32767
#define NFIELD	16
#define KEYSZ	8
#define VALSZ	32767
#define Q	"Q"
#define A	"A"
#define MOD	"MOD"
#define PREV	"PREV"
#define NEXT	"NEXT"

struct option {
	int exact;
	int rand;
	int maxn;
};

struct field {
	char *key;
	char *val;
	struct field *next;
};

struct card {
	char *head;
	struct field *field;
	char *tail;
	char *file;
};

/* main.c */
extern struct option opt;
extern long now;

/* ctab.c */
extern struct card ctab[MAXN];
extern int ncards;
void ctabload(char *file);
void ctabdump(char *file);

/* learn.c */
void learn(void);

/* parse.c */
void parseinit(char *path);
struct card *parsecard(struct card *dst);
void parsedone(void);

/* main.c */
extern char *progname;

/* utils.c */
void err(char *fmt, ...);
void syserr(void);
