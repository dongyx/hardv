#define _XOPEN_SOURCE 700
#include <sys/wait.h>
#include <libgen.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
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

char *progname;

void learn(char *fn, time_t now, struct opt opt);
void help(FILE *fp, int ret);
void version(FILE *fp, int ret);
int stdquiz(struct card *card, time_t now, int card1);
int modquiz(struct card *card, time_t now, int card1);
void dump(struct card *ctab, int n, FILE *fp, char *fn);
int loadcard(char *fn, FILE *fp, int *lineno, struct card *card);
void dumpcard(FILE *fp, struct card *card);
void destrcard(struct card *card);
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
char *swapname(char *sn, char *fn, int n);
void err(char *s, ...);
void parserr(char *fn, int ln, char *s);
void syserr();

int main(int argc, char **argv)
{
	struct opt opt;
	int ch, *sig;
	time_t now;

	progname = argv[0];
	if (argc < 2)
		help(stderr, 1);
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
				help(stderr, 1);
			break;
		default:
			help(stderr, 1);
		}
	if (optind >= argc)
		help(stderr, 1);
	argv += optind;
	argc -= optind;
	while (*argv)
		learn(*argv++, now, opt);
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
	fputs("HardV 4.0.1 <https://github.com/dongyx/hardv>\n", fp);
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

void learn(char *fn, time_t now, struct opt opt)
{
	static int card1 = 1;
	struct card ctab[NCARD];
	struct card *plan[NCARD];
	struct card *card;
	char sn[PATHSZ];
	int nc, np, i, dirty, lineno;
	struct flock lock;
	FILE *fp;

	if (!(fp = fopen(fn, "r+")))
		syserr();
	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = lock.l_len = 0;
	if (fcntl(fileno(fp), F_SETLK, &lock) == -1) {
		if (errno == EAGAIN || errno == EACCES)
			err("Other process is accessing %s\n", fn);
		syserr();
	}
	if (!access(swapname(sn, fn, PATHSZ), F_OK))
		err(
			"Swap file detected: %s\n"
			"This may be caused by a previous crash. "
			"You should check the swap file "
			"to recover the data "
			"then delete the swap file.\n",
			sn
		);
	lineno = 0;
	nc = 0;
	while (nc < NCARD && loadcard(fn, fp, &lineno, &ctab[nc]))
		nc++;
	if (!feof(fp))
		err("Too many cards in %s\n", fn);
	np = 0;
	for (card = ctab; card < ctab + nc; card++)
		if (card->field && isnow(card, now, opt.exact))
			plan[np++] = card;
	if (opt.rand)
		shuf(plan, np);
	for (i = 0; opt.maxn && i < np; i++) {
		card = plan[i];
		if (getv(card, MOD))
			dirty = modquiz(card, now, card1);
		else
			dirty = stdquiz(card, now, card1);
		if (dirty)
			dump(ctab, nc, fp, sn);
		if (opt.maxn > 0)
			opt.maxn--;
		card1 = 0;
	}
	for (card = ctab; card < ctab + nc; card++)
		destrcard(card);
	fclose(fp);
}

void dump(struct card *ctab, int n, FILE *fp, char *sn)
{
	FILE *sp;
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
	if ((fd = open(sn, O_WRONLY|O_CREAT|O_EXCL, 0600)) == -1)
		err("%s: %s\n", sn, strerror(errno));
	if (!(sp = fdopen(fd, "w")))
		syserr();
	for (card = ctab; card < ctab + n; card++)
		dumpcard(sp, card);
	if (
		fflush(sp) == EOF ||
		fsync(fileno(sp)) == -1
	)
		syserr();
	fclose(sp);
	if (
		fseek(fp, 0, SEEK_SET) == -1 ||
		fflush(fp) == EOF ||
		ftruncate(fileno(fp), 0) == -1
	)
		syserr();
	for (card = ctab; card < ctab + n; card++)
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

char *swapname(char *sn, char *fn, int n)
{
	static char *epsz = "File path too long\n";
	char buf[PATHSZ], dn[PATHSZ], *bn;

	strncpy(buf, fn, PATHSZ);
	if (buf[PATHSZ - 1])
		err(epsz);
	strcpy(dn, dirname(buf));
	bn = basename(strcpy(buf, fn));
	if (snprintf(sn, n, "%s/.%s.swp", dn, bn) >= n)
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
	case 1: setn(card, now);
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

/* return 1 for success, 0 for EOF */
int loadcard(char *fn, FILE *fp, int *lineno, struct card *card)
{
	static char *enl = "Too many lines";
	static char *evf = "Invalid value";
	static char *evsz = "Value too large";
	char lb[LINESZ], k[KEYSZ], v[VALSZ], *vp;
	struct field *f;
	int s, n, ch;
	int nf, nq, na;
	int start, kl = -1;

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
	exit(1);
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
	exit(1);
}

void syserr()
{
	perror(progname);
	exit(1);
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
