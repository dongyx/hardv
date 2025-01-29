#define MAXN 65536
#define KCHAR "abcdefghijklmnopqrstuvwxyABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_" 
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
extern char *progname;
extern struct option opt;
extern time_t now;

/* ctab.c */
extern struct card *ctab;
extern size_t ncards;
void ctabload(char *file);
void ctabdump(char *file);

/* learn.c */
void learn(void);

/* parse.c */
void parseinit(char *path);
struct card *parsecard(struct card *dst);
void parsedone(void);


/* utils.c */
time_t elapsecs(char *buf);
void err(char *fmt, ...);
void syserr(void);
