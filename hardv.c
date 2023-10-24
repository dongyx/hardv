#define _XOPEN_SOURCE 700
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <libgen.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#define KCHAR	"abcdefghijklmnopqrstuvwxyz" \
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
		"0123456789_"
#define SHELL	"/bin/sh"
#define DAY	(60*60*24)
#define PATHSZ	32767
#define NLINE	32767
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

struct opt {
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
	int leadnewl;
	struct field *field;
	char *sep;
};

char *progname, *filename;
int sigtab[] = {SIGHUP,SIGINT,SIGTERM,SIGQUIT,0};
sigset_t bset, oset;	/* block set, old set */
jmp_buf jdump;
volatile int aborted;	/* why volatile: signal handler */
struct opt opt;
int card1 = 1;
int lineno;
time_t now;

void learn(char *fn);
void help(FILE *fp, int ret);
void version(FILE *fp, int ret);
void stdquiz(struct card *card);
void modquiz(struct card *card);
void dump(int sig);
int loadcard(FILE *fp, struct card *card);
void dumpcard(FILE *fp, struct card *card);
void destrcard(struct card *card);
void shuf(struct card *a[], int n);
void sety(struct card *card);
void setn(struct card *card);
char *timev(time_t clock);
char *getv(struct card *card, char *k);
char *setv(struct card *card, char *k, char *v);
int isvalidf(struct field *f);
time_t getdiff(struct card *card);
struct field *revfield(struct field *f);
int isnow(struct card *card);
void setsig(void (*rtn)(int));
char *normv(char *s, char *buf, int n);
int pindent(char *s);
time_t tmparse(char *s);
FILE *mkswap(char *fn, char *sn);
void err(char *s, ...);
void parserr(char *s);
void syserr();

int main(int argc, char **argv)
{
	int ch, *sig;

	progname = argv[0];
	if (argc < 2)
		help(stderr, -1);
	if (!strcmp(argv[1], "--help"))
		help(stdout, 0);
	if (!strcmp(argv[1], "--version"))
		version(stdout, 0);
	srand(getpid()^time(NULL));
	if ((now = tmparse(getenv("HARDV_NOW"))) <= 0)
		time(&now);
	memset(&opt, 0, sizeof opt);
	opt.maxn = -1;
	while ((ch = getopt(argc, argv, "dern:")) != -1)
		switch (ch) {
		case 'e':
			opt.exact = 1;
			break;
		case 'r':
			opt.rand = 1;
			break;
		case 'n':
			if ((opt.maxn = atoi(optarg)) <= 0)
				help(stderr, -1);
			break;
		default:
			help(stderr, -1);
		}
	if (optind >= argc)
		help(stderr, -1);
	sigemptyset(&bset);
	for (sig=sigtab; *sig; sig++)
		sigaddset(&bset, *sig);
	sigaddset(&bset, SIGTSTP);
	argv += optind;
	argc -= optind;
	while (*argv)
		learn(*argv++);
	return 0;
}

void help(FILE *fp, int ret)
{
	fputs("Usage:\n", fp);
	fprintf(fp, "\t%s [options] FILE...\n", progname);
	fprintf(fp, "\t%s --help|--version\n", progname);
	fputs("Options:\n", fp);
	fputs("\t-e	enable exact quiz time\n", fp);
	fputs("\t-r	randomize the quiz order within a file\n", fp);
	fputs("\t-n N	quiz at most N cards\n", fp);
	fputs("\t--help	print the brief help\n", fp);
	fputs("\t--version\n", fp);
	fputs("\t	print the version information\n", fp);
	exit(ret);
}

void version(FILE *fp, int ret)
{
	fputs("HardV 3.2.0 <https://github.com/dongyx/hardv>\n", fp);
	fputs(
		"Copyright (c) "
		"2022 DONG Yuxuan <https://www.dyx.name>\n",
		fp
	);
	fputc('\n', fp);
	fprintf(fp, "NLINE:	%d\n", NLINE);
	fprintf(fp, "LINESZ:	%d\n", LINESZ);
	fprintf(fp, "NCARD:	%d\n", NCARD);
	fprintf(fp, "NFIELD:	%d\n", NFIELD);
	fprintf(fp, "KEYSZ:	%d\n", KEYSZ);
	fprintf(fp, "VALSZ:	%d\n", VALSZ);
	fprintf(fp, "PATHSZ:	%d\n", PATHSZ);
	exit(ret);
}

void learn(char *fn)
{
	struct card ctab[NCARD], *plan[NCARD], *card;
	char lb[LINESZ], sn[PATHSZ];
	FILE *fp, *sp;
	int nc, np, i;

	filename = fn;
	if (!(fp = fopen(fn, "r")))
		syserr();
	lineno = 0;
	nc = 0;
	while (nc < NCARD && loadcard(fp, ctab+nc))
		nc++;
	if (!feof(fp))
		err("too many cards in %s\n", fn);
	fclose(fp);
	np = 0;
	for (card=ctab; card<ctab+nc; card++)
		if (card->field && isnow(card))
			plan[np++] = card;
	if (opt.rand)
		shuf(plan, np);
	sigprocmask(SIG_BLOCK, &bset, &oset);
	if (setjmp(jdump))
		goto DUMP;
	setsig(dump);
	sigprocmask(SIG_SETMASK, &oset, NULL);
	for (i=0; opt.maxn && i<np; i++) {
		card = plan[i];
		if (getv(card, MOD))
			modquiz(card);
		else
			stdquiz(card);
		card1 = 0;
		if (opt.maxn > 0)
			opt.maxn--;
	}
DUMP:	sigprocmask(SIG_BLOCK, &bset, &oset);
	sp = mkswap(fn, sn);
	for (card=ctab; card<ctab+nc; card++)
		dumpcard(sp, card);
	if (fflush(sp) == EOF || fsync(fileno(sp)) == -1)
		syserr();
	if (!(fp = fopen(fn, "w")))
		syserr();
	fseek(sp, 0, SEEK_SET);
	while (fgets(lb, LINESZ, sp))
		if (fputs(lb, fp) == EOF)
			syserr();
	if (fflush(fp) == EOF || fsync(fileno(fp)) == -1)
		syserr();
	fclose(fp);
	fclose(sp);
	unlink(sn);
	if (aborted)
		exit(127+aborted);
	for (card=ctab; card<ctab+nc; card++)
		destrcard(card);
	setsig(SIG_DFL);
	sigprocmask(SIG_SETMASK, &oset, NULL);
}

FILE *mkswap(char *fn, char *sn)
{
	static char *epsz = "file path too long\n";
	static char pb[PATHSZ], dn[PATHSZ], *bn;
	struct stat st;
	int fd;

	strncpy(pb, fn, PATHSZ);
	if (pb[PATHSZ-1])
		err(epsz);
	strcpy(dn, dirname(pb));
	/* dirname() may change the original string */
	strncpy(pb, fn, PATHSZ);
	bn = basename(pb);
	if (snprintf(sn, PATHSZ, "%s/.%s.swp", dn, bn) >= PATHSZ)
		err(epsz);
	if (stat(fn, &st) == -1)
		syserr();
	if ((fd = open(sn, O_RDWR|O_CREAT|O_EXCL, st.st_mode)) == -1) {
		if (errno == EEXIST)
			err(	"The swap file %s is detected. "
				"This may be caused by "
				"simultaneous quiz/editing, "
				"or a previous crash. "
				"If it's caused by a crash, "
				"you should "
				"run a diff on the original file with "
				"the swap file, "
				"recover the data, "
				"and delete the swap file.\n",
				sn
			);
		syserr();
	}
	return fdopen(fd, "r+");
}

void stdquiz(struct card *card)
{
	char in[LINESZ], ques[VALSZ], answ[VALSZ], *act;

	if (!card1)
		putchar('\n');
	normv(getv(card, Q), ques, VALSZ);
	normv(getv(card, A), answ, VALSZ);
	puts("Q:\n");
	pindent(ques);
	puts("\n");
	fflush(stdout);
CHECK:
	fputs("Press <ENTER> to check the answer.\n", stdout);
	fflush(stdout);
	while (!fgets(in, LINESZ, stdin)) {
		if (feof(stdin))
			longjmp(jdump, 1);
		if (errno != EINTR)
			syserr();
	}
	if (strcmp(in, "\n"))
		goto CHECK;
	puts("A:\n");
	pindent(answ);
	puts("\n");
	fflush(stdout);
QUERY:
	fputs("Do you recall? (y/n/s)\n", stdout);
	fflush(stdout);
	while (!fgets(in, LINESZ, stdin)) {
		if (feof(stdin))
			longjmp(jdump, 1);
		if (errno != EINTR)
			syserr();
	}
	act = strstr("y\nn\ns\n", in);
	if (!act || *act == '\n')
		goto QUERY;
	switch (*act) {
	case 'y': sety(card); break;
	case 'n': setn(card); break;
	}
}

void modquiz(struct card *card)
{
	char pfx[] = "HARDV_F_";
	char sprev[64], snext[64], snow[64], first[2];
	char k[KEYSZ + sizeof(pfx) - 1];
	char mod[VALSZ], q[VALSZ], a[VALSZ];
	struct field *f;
	pid_t pid;
	int stat;

	if ((pid=fork()) == -1)
		syserr();
	/* child */
	if (pid == 0) {
		strcpy(k, pfx);
		normv(getv(card, MOD), mod, VALSZ);
		normv(getv(card, Q), q, VALSZ);
		normv(getv(card, A), a, VALSZ);
		sprintf(snext, "%ld", (long)tmparse(getv(card, NEXT)));
		sprintf(sprev, "%ld", (long)tmparse(getv(card, PREV)));
		sprintf(snow, "%ld", (long)now);
		first[0] = first[1] = '\0';
		if (card1)
			first[0] = '1';
		if (	setenv("HARDV_Q",	q,	1) == -1 ||
			setenv("HARDV_A",	a,	1) == -1 ||
			setenv("HARDV_NEXT",	snext,	1) == -1 ||
			setenv("HARDV_PREV",	sprev,	1) == -1 ||
			setenv("HARDV_NOW",	snow,	1) == -1 ||
			setenv("HARDV_FIRST",	first,	1) == -1
		)
			syserr();
		for (f=card->field; f!=NULL; f=f->next) {
			strcpy(&k[sizeof(pfx)-1], f->key);
			if (setenv(k, f->val, 1) == -1)
				syserr();
		}
		if (execl(SHELL, SHELL, "-c", mod, NULL) == -1)
			syserr();
	}
	/* parent */
	while (waitpid(pid, &stat, 0) == -1)
		if (errno != EINTR)
			syserr();
	switch (WEXITSTATUS(stat)) {
	case 0: sety(card); break;
	case 1: setn(card); break;
	}
}

void sety(struct card *card)
{
	time_t diff;

	/* signal atomic */
	sigprocmask(SIG_BLOCK, &bset, &oset);
	diff = getdiff(card);
	setv(card, PREV, timev(now));
	setv(card, NEXT, timev(now + 2*diff));
	sigprocmask(SIG_SETMASK, &oset, NULL);
}

void setn(struct card *card)
{
	/* signal atomic */
	sigprocmask(SIG_BLOCK, &bset, &oset);
	setv(card, PREV, timev(now));
	setv(card, NEXT, timev(now+DAY));
	sigprocmask(SIG_SETMASK, &oset, NULL);
}

/* return 1 for success, 0 for EOF */
int loadcard(FILE *fp, struct card *card)
{
	static char *enl = "too many lines";
	static char *evf = "invalid value";
	static char *evsz = "value too large";
	char lb[LINESZ], k[KEYSZ], v[VALSZ], *vp;
	struct field *f;
	int s, n;
	int ch, nf, nq, na;
	int kl = -1;	/* starting line of current field */
	int ol; /* lineno swap */

	ol = lineno;
	if (feof(fp))
		return 0;
	memset(card, 0, sizeof *card);
	nf = 0;
	while ((ch=fgetc(fp)) == '\n') {
		if (lineno >= NLINE)
			parserr(enl);
		lineno++;
		card->leadnewl++;
	}
	if (ungetc(ch, fp) != ch)
		syserr();
	vp = NULL;
	f = NULL;
	nq = na = 0;
	while (fgets(lb, LINESZ, fp)) {
		if (lineno >= NLINE)
			parserr(enl);
		lineno++;
		n = strlen(lb);
		if (lb[n - 1] != '\n' && !feof(fp))
			parserr("line too long");
		if (lb[0] == '%') {
			/* end */
			if (!(card->sep = strdup(lb)))
				syserr();
			break;
		} else if (lb[0] == '\t' || lb[0] == '\n') {
			/* successive lines in value */
			if (!f)
				parserr("key is expected");
			if (vp-v >= VALSZ-n)
				parserr(evsz);
			vp = stpcpy(vp, lb);
		} else {
			/* new field */
			if (nf >= NFIELD)
				parserr("too many fields");
			if (f) {
				f->key = strdup(k);
				f->val = strdup(v);
				if (!f->key || !f->val)
					syserr();
				if (!isvalidf(f)) {
					lineno = kl;
					parserr(evf);
				}
			}
			kl = lineno;
			nf++;
			vp = v;
			*vp = '\0';
			s = strcspn(lb, "\n\t");
			if (s >= KEYSZ)
				parserr("key too large");
			*stpncpy(k, lb, s) = '\0';
			if (k[strspn(k, KCHAR)])
				parserr("invalid key");
			for (f=card->field; f; f=f->next)
				if (!strcmp(f->key, k))
					parserr("duplicated key");
			if (!strcmp(k, Q)) nq++;
			if (!strcmp(k, A)) na++;
			if (!(f = malloc(sizeof *f)))
				syserr();
			f->next = card->field;
			card->field = f;
			if (!lb[s])
				continue;
			if (vp-v >= VALSZ-(n-s))
				parserr(evsz);
			vp = stpcpy(vp, &lb[s]);
		}
	}
	if (ferror(fp))
		syserr();
	if (f) {
		if (!(f->key = strdup(k)) || !(f->val = strdup(v)))
			syserr();
		if (!isvalidf(f)) {
			lineno = kl;
			parserr(evf);
		}
		if (nq != 1 || na != 1) {
			lineno = ol+card->leadnewl+1;
			parserr("no mandatory field");
		}
		/* fields were installed in the reversed order */
		card->field = revfield(card->field);
	}
	return 1;
}

void dumpcard(FILE *fp, struct card *card)
{
	struct field *f;
	int i;

	for (i = 0; i < card->leadnewl; i++)
		if (fputc('\n', fp) == EOF)
			syserr();
	for (f = card->field; f != NULL; f = f->next)
		if (fprintf(fp, "%s%s", f->key, f->val) < 0)
			syserr();
	if (card->sep && fputs(card->sep, fp) == EOF)
		syserr();
}

void dump(int sig)
{
	aborted = sig;
	longjmp(jdump, 1);
}

void parserr(char *s)
{
	fprintf(
		stderr,
		"%s: %s: line %d: %s\n",
		progname,
		filename,
		lineno,
		s
	);
	exit(-1);
}

void setsig(void (*rtn)(int))
{
	int *sig;

	for (sig=sigtab; *sig; sig++)
		signal(*sig, rtn);
}

void destrcard(struct card *card)
{
	struct field *i, *j;

	free(card->sep);
	for (i = card->field; i != NULL; i = j) {
		free(i->key);
		free(i->val);
		j = i->next;
		free(i);
	}
}

char *normv(char *s, char *buf, int n)
{
	char *sp, *bp;

	while (*s == '\n')
		s++;
	for (sp = s, bp = buf; bp != &buf[n] && *sp; sp++)
		if (*sp != '\t' || sp > s && sp[-1] != '\n')
			*bp++ = *sp;
	if (bp == &buf[n])
		return NULL;
	while (bp > buf && bp[-1] == '\n')
		bp--;
	*bp = '\0';
	return buf;
}

int isnow(struct card *card)
{
	struct tm today, theday;
	time_t next;

	next = tmparse(getv(card, NEXT));
	if (next <= 0)
		next = now;
	if (opt.exact)
		return now >= next;
	memcpy(&today, localtime(&now), sizeof today);
	memcpy(&theday, localtime(&next), sizeof theday);
	if (today.tm_year > theday.tm_year)
		return 1;
	if (today.tm_year == theday.tm_year
		&& today.tm_mon > theday.tm_mon)
		return 1;
	if (today.tm_year == theday.tm_year
		&& today.tm_mon == theday.tm_mon
		&& today.tm_mday >= theday.tm_mday)
		return 1;
	return 0;
}

void shuf(struct card *a[], int n)
{
	struct card *t;
	int i, j;

	for (i=0; i<n; i++) {
		j = i + rand()%(n-i);
		t = a[i];
		a[i] = a[j];
		a[j] = t;
	}
}

int isvalidf(struct field *f)
{
	if (!strcmp(f->key, NEXT) || !strcmp(f->key, PREV))
		if (tmparse(f->val) <= 0)
			return 0;
	return 1;
}

time_t tmparse(char *s)
{
	/* timegm() is not conforming to any standard,
	 * but provided by both BSD and Linux.
	 * Explicit declaration could avoid the trouble caused by
	 * feature test macros.
	 */
	extern time_t timegm(struct tm*);
	unsigned hr, mi;
	char sg;
	struct tm tm;
	time_t ck;

	if (!s)
		return 0;
	while (isspace((unsigned char)*s))
		s++;
	memset(&tm, 0, sizeof tm);
	if (!(s = strptime(s, "%Y-%m-%d %H:%M:%S", &tm))
		|| (sscanf(s, " %c%2u%2u", &sg, &hr, &mi) != 3))
		return 0;
	ck = timegm(&tm);
	if (sg == '+')
		ck -= mi*60 + hr*3600L;
	else if (sg == '-')
		ck += mi*60 + hr*3600L;
	else
		return 0;
	return ck;
}

struct field *revfield(struct field *f)
{
	struct field *r;
	struct field *swp;

	/* r points to the reversed part, f points to the left part */
	r = NULL;
	while (f) {
		swp = f->next;
		f->next = r;
		r = f;
		f = swp;
	}
	return r;
}

void err(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "%s: ", progname);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(-1);
}

void syserr()
{
	perror(progname);
	exit(-1);
}

time_t getdiff(struct card *card)
{
	time_t prev, next, diff;

	prev = tmparse(getv(card, PREV));
	next = tmparse(getv(card, NEXT));
	if (prev <= 0) prev = now;	
	if (next <= 0) next = now;
	if (next < prev || (diff = next - prev) < DAY)
		diff = DAY;
	return diff;
}

char *timev(time_t clock)
{
	static char buf[VALSZ];
	struct tm *lc;
	int n;

	lc = localtime(&clock);
	buf[0] = '\t';
	n = strftime(&buf[1], VALSZ - 2, "%Y-%m-%d %H:%M:%S %z", lc);
	buf[1 + n] = '\n';
	buf[2 + n] = '\0';
	return buf;
}

char *getv(struct card *card, char *k)
{
	struct field *i;

	for (i=card->field; i; i=i->next)
		if (!strcmp(i->key, k))
			return i->val;
	return NULL;
}

char *setv(struct card *card, char *k, char *v)
{
	struct field *i;

	for (i=card->field; i; i=i->next)
		if (!strcmp(i->key, k))
			break;
	if (!i) {
		if (!(i=malloc(sizeof *i)))
			syserr();
		memset(i, 0, sizeof *i);
		if (!(i->key=strdup(k)))
			syserr();
		i->val = NULL;
		i->next = card->field;
		card->field = i;
	}
	free(i->val);
	if (!(i->val=strdup(v)))
		syserr();
	return i->val;
}

int pindent(char *s)
{
	char *sp;

	for (sp = s; *sp ; sp++) {
		if (sp == s || sp[-1] == '\n')
			putchar('\t');
		putchar(*sp);
	}
	return sp - s;
}
