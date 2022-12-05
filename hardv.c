#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <libgen.h>
#include <stddef.h>
#include <stdlib.h>
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
struct opt {
	int exact;
	int lowio;
	int rand;
	int maxn;
};

char *progname = "hardv";
char *filename, *swapname;
int sigtab[] = {SIGHUP,SIGINT,SIGTERM,SIGQUIT,0};
sigset_t bset, oset;	/* block set, old set */
jmp_buf jrmswp, jdump;
int aborted;
struct opt opt;
int card1 = 1;
int lineno;
time_t now;

void init(int argc, char **argv);
void help(FILE *fp, int ret);
void pversion(FILE *fp, int ret);
void lowio(char *fn);
void lowmem(char *fn);
char *mkswap(char *fn);
void stdquiz(struct card *card);
void modquiz(struct card *card);
void sety(struct card *card);
void setn(struct card *card);
int loadcard(FILE *fp, struct card *card);
void dumpcard(FILE *fp, struct card *card);
void godump(int sig);
void parserr(char *s);
void dumperr(char *s);

int main(int argc, char **argv)
{
	/* opt and optind is set by init() */
	init(argc, argv);
	argv += optind;
	argc -= optind;
	while (*argv) {
		if (opt.lowio)
			lowio(*argv);	/* optimize for disk I/O */
		else
			lowmem(*argv);	/* optimize for memory */
		argv++;
	}
	return 0;
}

void setsig(void (*rtn)(int));
void destrcard(struct card *card);
char *normval(char *s, char *buf, int n);
int isnow(struct card *card);
void shuf(struct card *a[], int n);
int isvalidf(struct field *f);
time_t tmparse(char *s);
struct field *revfield(struct field *f);
void fatal(char *s); 
void syserr(jmp_buf *j);
time_t getdiff(struct card *card);
char *timev(time_t clock);
char *getv(struct card *card, char *k);
char *setv(struct card *card, char *k, char *v);
int pindent(char *s);

void init(int argc, char *argv[])
{
	int ch, *sig;

	srand(getpid()^time(NULL));
	if ((now = tmparse(getenv("HARDV_NOW"))) <= 0)
		time(&now);
	memset(&opt, 0, sizeof opt);
	opt.maxn = -1;
	while ((ch = getopt(argc, argv, "dhvern:")) != -1)
		switch (ch) {
		case 'e':
			opt.exact = 1;
			break;
		case 'r':
			opt.rand = 1;
		case 'd':
			opt.lowio = 1;
			break;
		case 'n':
			if ((opt.maxn = atoi(optarg)) <= 0)
				help(stderr, -1);
			break;
		case 'h':
			help(stdout, 0);
		case 'v':
			pversion(stdout, 0);
		case '?':
		default:
			help(stderr, -1);
		}
	if (optind >= argc)
		help(stderr, -1);
	sigemptyset(&bset);
	for (sig=sigtab; *sig; sig++)
		sigaddset(&bset, *sig);
	sigaddset(&bset, SIGTSTP);
}

void help(FILE *fp, int ret)
{
	fputs("usage:\n", fp);
	fputs("\thardv [options] file ...\n", fp);
	fputs("\thardv -h|-v\n", fp);
	fputs("\n", fp);
	fputs("options\n", fp);
	fputs("\n", fp);
	fputs("-e	enable exact quiz time\n", fp);
	fputs("-r	randomize the quiz order within a file\n"
		"	this option implies -d\n" , fp);
	fputs("-n <n>	quiz at most <n> cards\n", fp);
	fputs("-d	optimize for disk i/o instead of memory\n", fp);
	fputs("-h	print this help information\n", fp);
	fputs("-v	print version and building arguments\n", fp);
	exit(ret);
}

void pversion(FILE *fp, int ret)
{
	fprintf(fp, "hardv %s\n", VERSION);
	fprintf(fp, "%s\n", COPYRT);
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

void lowio(char *fn)
{
	struct card ctab[NCARD], *plan[NCARD], *card;
	char lb[LINESZ];
	FILE *fp, *sp;
	char *sn;
	int nc, np, i;

	filename = fn;
	swapname = sn = mkswap(fn);
	if (setjmp(jrmswp)) {
		unlink(sn);
		exit(-1);
	}
	/* use r+ for sp to avoid recreation */
	if (!(fp=fopen(fn, "r")) || !(sp=fopen(sn, "r+"))) {
		perror(progname);
		longjmp(jrmswp, 1);
	}
	lineno = 0;
	nc = 0;
	while (nc < NCARD && loadcard(fp, ctab+nc))
		nc++;
	if (!feof(fp)) {
		fprintf(stderr,
			"%s: too many cards in %s\n", progname, fn);
		longjmp(jrmswp, 1);
	}
	fclose(fp);
	np = 0;
	for (card=ctab; card<ctab+nc; card++)
		if (card->field && isnow(card))
			plan[np++] = card;
	if (opt.rand)
		shuf(plan, np);
	/* if a signal in sigtab is delivered, jump to dump */
	if (setjmp(jdump))
		goto DUMP;
	sigprocmask(SIG_BLOCK, &bset, &oset);
	setsig(godump);
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
	for (card=ctab; card<ctab+nc; card++)
		dumpcard(sp, card);
	if (fflush(sp) == EOF)
		dumperr(strerror(errno));
	if (!(fp = fopen(fn, "w")))
		dumperr(strerror(errno));
	fseek(sp, 0, SEEK_SET);
	while (fgets(lb, LINESZ, sp))
		if (fputs(lb, fp) == EOF)
			dumperr(strerror(errno));
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

void lowmem(char *fn)
{
	struct card cb, *cp;
	FILE *fp, *sp;
	char *sn, lb[LINESZ];

	filename = fn;
	swapname = sn = mkswap(fn);
	if (setjmp(jrmswp)) {
		unlink(sn);
		exit(-1);
	}
	/* use r+ for sp to avoid recreation */
	if (!(fp=fopen(fn, "r")) || !(sp=fopen(sn, "r+"))) {
		perror(progname);
		longjmp(jrmswp, 1);
	}
	/* syntax checking */
	lineno = 0;
	while (loadcard(fp, &cb))
		destrcard(&cb);
	fseek(fp, SEEK_SET, 0);
	sigprocmask(SIG_BLOCK, &bset, &oset);
	if (setjmp(jdump))
		goto DUMP;
	setsig(godump);
	cp = NULL;
	sigprocmask(SIG_SETMASK, &oset, NULL);
	lineno = 0;
	while (opt.maxn) {
		sigprocmask(SIG_BLOCK, &bset, &oset);
		if (!loadcard(fp, &cb)) {
			sigprocmask(SIG_SETMASK, &oset, NULL);
			break;
		}
		cp = &cb;
		sigprocmask(SIG_SETMASK, &oset, NULL);
		if (cp->field && isnow(cp)) {
			if (getv(cp, MOD))
				modquiz(cp);
			else
				stdquiz(cp);
			if (opt.maxn > 0)
				opt.maxn--;
			card1 = 0;
		}
		sigprocmask(SIG_BLOCK, &bset, &oset);
		dumpcard(sp, &cb);
		cp = NULL;
		destrcard(&cb);
		if (EOF == fflush(sp))
			dumperr(strerror(errno));
		sigprocmask(SIG_SETMASK, &oset, NULL);
	}
DUMP:	sigprocmask(SIG_BLOCK, &bset, &oset);
	if (cp) {
		dumpcard(sp, cp);
		destrcard(cp);
	}
	while (loadcard(fp, &cb)) {
		dumpcard(sp, &cb);
		destrcard(&cb);
	}
	if (fflush(sp) == EOF)
		dumperr(strerror(errno));
	if (!(fp=freopen(fn, "w", fp)))
		dumperr(strerror(errno));
	fseek(sp, 0, SEEK_SET);
	while (fgets(lb, LINESZ, sp))
		if (fputs(lb, fp) == EOF)
			dumperr(strerror(errno));
	fclose(fp);
	fclose(sp);
	unlink(sn);
	if (aborted)
		exit(127+aborted);
	setsig(SIG_DFL);
	sigprocmask(SIG_SETMASK, &oset, NULL);
}

char *mkswap(char *fn)
{
	static char *epsz = "file path too long";
	static char sn[PATHSZ];
	char pb[PATHSZ], *dn, *bn;
	struct stat st;
	int fd;

	strncpy(pb, fn, PATHSZ);
	if (pb[PATHSZ-1])
		fatal(epsz);
	dn = dirname(pb);
	/* dirname() may change the original string */
	strncpy(pb, fn, PATHSZ);
	bn = basename(pb);
	if (snprintf(sn, PATHSZ, "%s/.%s.swp", dn, bn) >= PATHSZ)
		fatal(epsz);
	if (stat(fn, &st) == -1)
		syserr(NULL);
	if ((fd = open(sn, O_CREAT|O_EXCL, st.st_mode)) == -1) {
		if (errno == EEXIST) {
			fprintf(stderr,
				"The swap file %s is detected. "
				"This may be caused by "
				"simultaneous quiz/editing, "
				"or a previous crash. "
				"If it's caused by a crash, "
				"you should "
				"run a diff on the original file with "
				"the swap file, "
				"recover the data, "
				"and delete the swap file.\n",
				sn);
			exit(-1);
		}
		syserr(NULL);
	}
	close(fd);
	return sn;
}

void stdquiz(struct card *card)
{
	char in[LINESZ], ques[VALSZ], answ[VALSZ], *act;

	if (!card1)
		putchar('\n');
	normval(getv(card, Q), ques, VALSZ);
	normval(getv(card, A), answ, VALSZ);
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
			syserr(&jdump);
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
			syserr(&jdump);
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
		syserr(&jdump);
	/* child */
	if (pid == 0) {
		strcpy(k, pfx);
		normval(getv(card, MOD), mod, VALSZ);
		normval(getv(card, Q), q, VALSZ);
		normval(getv(card, A), a, VALSZ);
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
			syserr(NULL);
		for (f=card->field; f!=NULL; f=f->next) {
			strcpy(&k[sizeof(pfx)-1], f->key);
			if (setenv(k, f->val, 1) == -1)
				syserr(NULL);
		}
		if (execl(SHELL, SHELL, "-c", mod, NULL) == -1)
			syserr(NULL);
	}
	/* parent */
	while (waitpid(pid, &stat, 0) == -1)
		if (errno != EINTR)
			syserr(&jdump);
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
	if (!setv(card, PREV, timev(now)))
		syserr(&jdump);
	if (!setv(card, NEXT, timev(now + 2*diff)))
		syserr(&jdump);
	sigprocmask(SIG_SETMASK, &oset, NULL);
}

void setn(struct card *card)
{
	/* signal atomic */
	sigprocmask(SIG_BLOCK, &bset, &oset);
	if (!setv(card, PREV, timev(now)))
		syserr(&jdump);
	if (!setv(card, NEXT, timev(now+DAY)))
		syserr(&jdump);
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
	size_t s, n;
	int ch, nf, nq, na;
	int kl;	/* starting line of current field */
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
		syserr(&jrmswp);
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
				syserr(&jrmswp);
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
					syserr(&jrmswp);
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
				syserr(&jrmswp);
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
		syserr(&jrmswp);
	if (f) {
		if (!(f->key = strdup(k)) || !(f->val = strdup(v)))
			syserr(&jrmswp);
		if (!isvalidf(f)) {
			lineno = kl;
			parserr(evf);
		}
		if (nq != 1 || na != 1) {
			lineno = ol+card->leadnewl+1;
			parserr("no mandatory field");
		}
		/* fields were installed in the revered order */
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
			dumperr(strerror(errno));
	for (f = card->field; f != NULL; f = f->next)
		if (fprintf(fp, "%s%s", f->key, f->val) < 0)
			dumperr(strerror(errno));
	if (card->sep && fputs(card->sep, fp) == EOF)
		dumperr(strerror(errno));
}

void godump(int sig)
{
	aborted = sig;
	longjmp(jdump, 1);
}

void parserr(char *s)
{
	fprintf(stderr, "%s, line %d: %s\n", filename, lineno, s);
	longjmp(jrmswp, 1);
}

void dumperr(char *s)
{
	fprintf(stderr, "%s: %s\n", progname, s);
	fprintf(stderr,
		"The swap file %s is created. "
		"You should "
		"run a diff on the original file with the swap file, "
		"recover the data, "
		"and delete the swap file.\n",
		swapname);
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

char *normval(char *s, char *buf, int n)
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
	while (isspace(*s))
		s++;
	memset(&tm, 0, sizeof tm);
	if (!(s = strptime(s, "%Y-%m-%d %H:%M:%S", &tm))
		|| (sscanf(s, " %[+-]%2u%2u", &sg, &hr, &mi) != 3))
		return 0;
	ck = timegm(&tm);
	if (sg == '+')
		ck -= mi*60 + hr*3600L;
	else
		ck += mi*60 + hr*3600L;
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

void fatal(char *s)
{
	fprintf(stderr, "%s: %s\n", progname, s);
	exit(-1);
}

void syserr(jmp_buf *j)
{
	perror(progname);
	if (j)
		longjmp(*j, 1);
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
	size_t n;

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
	char *vb;
	int newf;

	newf = 0;
	for (i=card->field; i; i=i->next)
		if (!strcmp(i->key, k))
			break;
	if (!i) {
		newf = 1;
		if (!(i=malloc(sizeof *i)))
			return NULL;
		memset(i, 0, sizeof *i);
		if (!(i->key=strdup(k))) {
			free(i);
			return NULL;
		}
		/* we must delay the linkage,
		 * since if we do this now and strdup(v) fails,
		 * the list will be corrupted.
		 * thus the caller can't dump cards before exit. */
	}
	if (!(vb=strdup(v))) {
		if (newf) {
			free(i->key);
			free(i);
		}
		return NULL;
	}
	free(i->val);
	i->val = vb;
	if (newf) {
		/* delay linkage */
		i->next = card->field;
		card->field = i;
	}
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
