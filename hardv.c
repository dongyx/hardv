#define _XOPEN_SOURCE 700
#include <sys/wait.h>
#include <libgen.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#define KCHAR	"abcdefghijklmnopqrstuvwxyz" \
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
		"0123456789_"
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
	char *filepath;
};

char *progname;

int loadctab(struct card *ctab, char *path);
void learn(struct card *ctab, int ncard, time_t now, struct opt *opt);
void help(void);
void version(void);
int stdquiz(struct card *card, time_t now, int card1);
int modquiz(struct card *card, time_t now, int card1);
void dump(struct card *ctab, int n, char *path);
int loadcard(char *fn, FILE *fp, int *lineno, struct card *card);
void dumpcard(FILE *fp, struct card *card);
void shuf(struct card *a[], int n);
void sety(struct card *card, time_t now);
void setn(struct card *card, time_t now);
char *timev(time_t clock, char *buf, int n);
char *getv(struct card *card, char *k);
char *setv(struct card *card, char *k, char *v);
int isvalidf(struct field *f);
time_t getdiff(struct card *card, time_t now);
struct field *revfield(struct field *f);
int isnow(struct card *card, time_t now, int exact);
char *normv(char *s, char *buf, int n);
int pindent(char *s);
time_t tmparse(char *s);
char *swapname(char *sn, char *fn);
void err(char *s, ...);
void parserr(char *fn, int ln, char *s);
void syserr();

int main(int argc, char **argv)
{
	static struct card ctab[NCARD];
	static int ncard;
	struct opt opt;
	struct flock lock;
	int n, ch, *sig;
	time_t now;

	progname = argv[0];
	srand(getpid()^time(NULL));
	if (argc > 1 && !strcmp(argv[1], "--help"))
		help();
	if (argc > 1 && !strcmp(argv[1], "--version"))
		version();
	if ((now = tmparse(getenv("HARDV_NOW"))) <= 0)
		time(&now);
	memset(&opt, 0, sizeof opt);
	opt.maxn = -1;
	while ((ch = getopt(argc, argv, "ern:")) != -1)
		switch (ch) {
		case 'e':
			opt.exact = 1;
			break;
		case 'r':
			opt.rand = 1;
			break;
		case 'n':
			if ((opt.maxn = atoi(optarg)) <= 0)
				err(
					"Not valid positive integer: "
						"%s\n",
					optarg
				);
			break;
		default:
			return 1;
		}
	argv += optind;
	argc -= optind;
	if (argc <= 0)
		err("File operand expected\n");
	for (; *argv != NULL; argv++) {
		int fd;

		if ((fd = open(*argv, O_RDWR)) == -1)
			syserr();
		lock.l_type = F_WRLCK;
		lock.l_whence = SEEK_SET;
		lock.l_start = lock.l_len = 0;
		if (fcntl(fd, F_SETLK, &lock) == -1) {
			if (errno == EAGAIN || errno == EACCES)
				err("%s: Other process is accessing\n",
					*argv);
			syserr();
		}
		ncard += loadctab(&ctab[ncard], *argv);
	}
	learn(ctab, ncard, now, &opt);
	return 0;
}

void help(void)
{
	puts("Usage:\n");
	printf("\t%s [options] FILE...\n", progname);
	printf("\t%s --help|--version\n", progname);
	puts("Options:");
	puts("	-e	enable exact quiz time");
	puts("	-r	randomize the quiz order");
	puts("	-n N	quiz at most N cards");
	puts("	--help	print the brief help");
	puts("	--version");
	puts("		print the version information");
	exit(0);
}

void version(void)
{
	puts("HardV 5.0.0-alpha.1 <https://github.com/dongyx/hardv>");
	puts("Copyright (c) 2022 DONG Yuxuan <https://www.dyx.name>");
	putchar('\n');
	printf("NLINE:	%d\n", NLINE);
	printf("LINESZ:	%d\n", LINESZ);
	printf("NCARD:	%d\n", NCARD);
	printf("NFIELD:	%d\n", NFIELD);
	printf("KEYSZ:	%d\n", KEYSZ);
	printf("VALSZ:	%d\n", VALSZ);
	printf("PATHSZ:	%d\n", PATHSZ);
	exit(0);
}

int loadctab(struct card *ctab, char *path)
{
	FILE *in;
	int n, ln;

	in = fopen(path, "r");
	if (in == NULL)
		syserr();
	ln = 0;
	n = 0;
	while (n < NCARD && loadcard(path, in, &ln, &ctab[n]))
		n++;
	if (!feof(in))
		err("Too many cards\n");
	return n;
}

void learn(struct card *ctab, int ncard, time_t now, struct opt *opt)
{
	static int card1 = 1;
	struct card *plan[NCARD];
	struct card *card;
	int n, i, dirty;

	n = 0;
	for (card = ctab; card < &ctab[ncard]; card++)
		if (card->field && isnow(card, now, opt->exact))
			plan[n++] = card;
	if (opt->rand)
		shuf(plan, n);
	for (i = 0; opt->maxn != 0 && i < n; i++) {
		card = plan[i];
		if (getv(card, MOD))
			dirty = modquiz(card, now, card1);
		else
			dirty = stdquiz(card, now, card1);
		if (dirty)
			dump(ctab, ncard, card->filepath);
		if (opt->maxn > 0)
			opt->maxn--;
		card1 = 0;
	}
}

void dump(struct card *ctab, int n, char *path)
{
	char sn[PATHSZ];
	FILE *fp, *sp;
	int fd;
	struct card *card;
	sigset_t bset, oset;

	sigemptyset(&bset);
	sigaddset(&bset, SIGHUP);
	sigaddset(&bset, SIGINT);
	sigaddset(&bset, SIGTERM);
	sigaddset(&bset, SIGQUIT);
	sigaddset(&bset, SIGTSTP);
	sigprocmask(SIG_BLOCK, &bset, &oset);
	swapname(sn, path);
	if ((fd = open(sn, O_WRONLY|O_CREAT|O_EXCL, 0600)) == -1)
		err("%s: %s\n", sn, strerror(errno));
	if (!(sp = fdopen(fd, "w")))
		syserr();
	for (card = ctab; card < ctab+n; card++)
		if (strcmp(card->filepath, path) == 0)
			dumpcard(sp, card);
	if (
		fflush(sp) == EOF ||
		fsync(fileno(sp)) == -1
	)
		syserr();
	fclose(sp);
	if ((fp = fopen(path, "w")) == NULL)
		syserr();
	for (card = ctab; card < ctab+n; card++)
		if (strcmp(card->filepath, path) == 0)
			dumpcard(fp, card);
	if (
		fflush(fp) == EOF ||
		fsync(fileno(fp)) == -1
	)
		syserr();
	if (unlink(sn) == -1)
		syserr();
	sigprocmask(SIG_SETMASK, &oset, NULL);
}

char *swapname(char *sn, char *fn)
{
	static char *epsz = "File path too long\n";
	char buf[PATHSZ], dn[PATHSZ], *bn;

	strncpy(buf, fn, PATHSZ);
	if (buf[PATHSZ - 1])
		err(epsz);
	strcpy(dn, dirname(buf));
	bn = basename(strcpy(buf, fn));
	if (snprintf(sn, PATHSZ, "%s/.%s.swp", dn, bn) >= PATHSZ)
		err(epsz);
	return sn;
}

int stdquiz(struct card *card, time_t now, int card1)
{
	char in[LINESZ], ques[VALSZ], answ[VALSZ];
	char *act;

	if (!card1)
		putchar('\n');
	normv(getv(card, Q), ques, VALSZ);
	normv(getv(card, A), answ, VALSZ);
	puts("Q:\n");
	pindent(ques);
	puts("\n");
	fflush(stdout);
	do {
		fputs("Press <ENTER> to check the answer.\n", stdout);
		fflush(stdout);
		while (!fgets(in, LINESZ, stdin)) {
			if (feof(stdin))
				exit(0);
			if (errno != EINTR)
				syserr();
		}
	} while (strcmp(in, "\n"));
	puts("A:\n");
	pindent(answ);
	puts("\n");
	fflush(stdout);
	do {
		fputs("Do you recall? (y/n/s)\n", stdout);
		fflush(stdout);
		while (!fgets(in, LINESZ, stdin)) {
			if (feof(stdin))
				exit(0);
			if (errno != EINTR)
				syserr();
		}
		act = strstr("y\nn\ns\n", in);
	} while (!act || *act == '\n');
	switch (*act) {
	case 'y':
		sety(card, now);
		return 1;
	case 'n':
		setn(card, now);
		return 1;
	}
	return 0;
}

int modquiz(struct card *card, time_t now, int card1)
{
	char mod[VALSZ], q[VALSZ], a[VALSZ];
	char pfx[] = "HARDV_F_";
	char sprev[64], snext[64], snow[64], first[2];
	char k[KEYSZ + sizeof(pfx) - 1];
	struct field *f;
	pid_t pid;
	int st;

	if ((pid = fork()) == -1)
		syserr();
	if (pid == 0) {
		normv(getv(card, MOD), mod, VALSZ);
		normv(getv(card, Q), q, VALSZ);
		normv(getv(card, A), a, VALSZ);
		sprintf(snext, "%ld", (long)tmparse(getv(card, NEXT)));
		sprintf(sprev, "%ld", (long)tmparse(getv(card, PREV)));
		sprintf(snow, "%ld", (long)now);
		first[0] = first[1] = '\0';
		if (card1)
			first[0] = '1';
		if (
			setenv("HARDV_Q",	q,	1) == -1 ||
			setenv("HARDV_A",	a,	1) == -1 ||
			setenv("HARDV_NEXT",	snext,	1) == -1 ||
			setenv("HARDV_PREV",	sprev,	1) == -1 ||
			setenv("HARDV_NOW",	snow,	1) == -1 ||
			setenv("HARDV_FIRST",	first,	1) == -1
		)
			syserr();
		strcpy(k, pfx);
		for (f = card->field; f != NULL; f = f->next) {
			strcpy(&k[sizeof(pfx) - 1], f->key);
			if (setenv(k, f->val, 1) == -1)
				syserr();
		}
		execl(SHELL, SHELL, "-c", mod, NULL);
		syserr();
	}
	while (waitpid(pid, &st, 0) == -1)
		if (errno != EINTR)
			syserr();
	switch (WEXITSTATUS(st)) {
	case 0:
		sety(card, now);
		return 1;
	case 1:
		setn(card, now);
		return 1;
	}
	return 0;
}

void sety(struct card *card, time_t now)
{
	char buf[VALSZ];
	time_t diff;

	diff = getdiff(card, now);
	setv(card, PREV, timev(now, buf, VALSZ));
	setv(card, NEXT, timev(now + 2 * diff, buf, VALSZ));
}

void setn(struct card *card, time_t now)
{
	char buf[VALSZ];

	setv(card, PREV, timev(now, buf, VALSZ));
	setv(card, NEXT, timev(now + DAY, buf, VALSZ));
}

int loadcard(char *fn, FILE *fp, int *lineno, struct card *card)
{
	static char *enl = "Too many lines";
	static char *evf = "Invalid value";
	static char *evsz = "Value too large";
	char lb[LINESZ], k[KEYSZ], v[VALSZ], *vp;
	struct field *f;
	int s, n, ch;
	int nf, nq, na;
	int start, kl;

	start = *lineno;
	if (feof(fp))
		return 0;
	memset(card, 0, sizeof *card);
	nf = 0;
	while ((ch = fgetc(fp)) == '\n') {
		if (*lineno >= NLINE)
			parserr(fn, *lineno, enl);
		(*lineno)++;
		card->leadnewl++;
	}
	ungetc(ch, fp);
	vp = NULL;
	f = NULL;
	nq = na = 0;
	kl = start + card->leadnewl;
	while (fgets(lb, LINESZ, fp)) {
		if (*lineno >= NLINE)
			parserr(fn, *lineno, enl);
		(*lineno)++;
		n = strlen(lb);
		if (lb[n - 1] != '\n' && !feof(fp))
			parserr(fn, *lineno, "Line too long");
		if (lb[0] == '%') {
			/* end */
			if (!(card->sep = strdup(lb)))
				syserr();
			break;
		} else if (lb[0] == '\t' || lb[0] == '\n') {
			/* successive lines in value */
			if (!f)
				parserr(fn, *lineno, "Key is expected");
			if (vp - v >= VALSZ - n)
				parserr(fn, *lineno, evsz);
			vp = stpcpy(vp, lb);
		} else {
			/* new field */
			if (nf >= NFIELD)
				parserr(fn, *lineno, "Too many fields");
			if (f) {
				if (!(f->key = strdup(k)))
					syserr();
				if (!(f->val = strdup(v)))
					syserr();
				if (!isvalidf(f))
					parserr(fn, kl, evf);
			}
			kl = *lineno;
			nf++;
			vp = v;
			*vp = '\0';
			s = strcspn(lb, "\n\t");
			if (s >= KEYSZ)
				parserr(fn, *lineno, "Key too large");
			*stpncpy(k, lb, s) = '\0';
			if (k[strspn(k, KCHAR)])
				parserr(fn, *lineno, "Invalid key");
			for (f = card->field; f; f = f->next)
				if (!strcmp(f->key, k))
					parserr(
						fn,
						*lineno,
						"Duplicated key"
					);
			if (!strcmp(k, Q))
				nq++;
			if (!strcmp(k, A))
				na++;
			if (!(f = malloc(sizeof *f)))
				syserr();
			f->next = card->field;
			card->field = f;
			if (!lb[s])
				continue;
			if (vp - v >= VALSZ - (n - s))
				parserr(fn, *lineno, evsz);
			vp = stpcpy(vp, &lb[s]);
		}
	}
	if (ferror(fp))
		syserr();
	if (f) {
		if (!(f->key = strdup(k)) || !(f->val = strdup(v)))
			syserr();
		if (!isvalidf(f))
			parserr(fn, kl, evf);
		if (nq != 1 || na != 1)
			parserr(
				fn,
				start + card->leadnewl + 1,
				"No mandatory field"
			);
		card->field = revfield(card->field);
	}
	if ((card->filepath = strdup(fn)) == NULL)
		syserr();
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

void parserr(char *fn, int ln, char *s)
{
	fprintf(
		stderr,
		"%s: %s: line %d: %s\n",
		progname,
		fn,
		ln,
		s
	);
	exit(-1);
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
		err("Buffer too small to normalize value\n");
	while (bp > buf && bp[-1] == '\n')
		bp--;
	*bp = '\0';
	return buf;
}

int isnow(struct card *card, time_t now, int exact)
{
	struct tm today, theday;
	time_t next;

	next = tmparse(getv(card, NEXT));
	if (next <= 0)
		next = now;
	if (exact)
		return now >= next;
	memcpy(&today, localtime(&now), sizeof today);
	memcpy(&theday, localtime(&next), sizeof theday);
	if (today.tm_year > theday.tm_year)
		return 1;
	if (
		today.tm_year == theday.tm_year
		&& today.tm_mon > theday.tm_mon
	)
		return 1;
	if (
		today.tm_year == theday.tm_year
		&& today.tm_mon == theday.tm_mon
		&& today.tm_mday >= theday.tm_mday
	)
		return 1;
	return 0;
}

void shuf(struct card *a[], int n)
{
	struct card *t;
	int i, j;

	for (i = 0; i < n; i++) {
		j = i + rand() % (n - i);
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
	if (
		!(s = strptime(s, "%Y-%m-%d %H:%M:%S", &tm)) ||
		(sscanf(s, " %c%2u%2u", &sg, &hr, &mi) != 3)
	)
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
	struct field *r, *swp;

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

time_t getdiff(struct card *card, time_t now)
{
	time_t prev, next, diff;

	prev = tmparse(getv(card, PREV));
	next = tmparse(getv(card, NEXT));
	if (prev <= 0)
		prev = now;
	if (next <= 0)
		next = now;
	if (next < prev || (diff = next - prev) < DAY)
		diff = DAY;
	return diff;
}

char *timev(time_t clock, char *buf, int n)
{
	struct tm *lc;
	int p;

	if (n < 3)
		goto noroom;
	lc = localtime(&clock);
	buf[0] = '\t';
	p = strftime(&buf[1], n - 2, "%Y-%m-%d %H:%M:%S %z", lc);
	if (!p)
		goto noroom;
	buf[1 + p] = '\n';
	buf[2 + p] = '\0';
	return buf;
noroom:	err("Buffer too small to format timestamp %ld\n", (long)clock);
	return NULL;
}

char *getv(struct card *card, char *k)
{
	struct field *i;

	for (i = card->field; i; i = i->next)
		if (!strcmp(i->key, k))
			return i->val;
	return NULL;
}

char *setv(struct card *card, char *k, char *v)
{
	struct field *i;

	for (i = card->field; i; i = i->next)
		if (!strcmp(i->key, k))
			break;
	if (!i) {
		if (!(i = malloc(sizeof *i)))
			syserr();
		memset(i, 0, sizeof *i);
		if (!(i->key = strdup(k)))
			syserr();
		i->val = NULL;
		i->next = card->field;
		card->field = i;
	}
	free(i->val);
	if (!(i->val = strdup(v)))
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
