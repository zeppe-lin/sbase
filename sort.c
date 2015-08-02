/* See LICENSE file for copyright and license details. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"
#include "text.h"
#include "util.h"

struct keydef {
	int start_column;
	int end_column;
	int start_char;
	int end_char;
	int flags;
	TAILQ_ENTRY(keydef) entry;
};

enum {
	MOD_N      = 1 << 0,
	MOD_STARTB = 1 << 1,
	MOD_ENDB   = 1 << 2,
	MOD_R      = 1 << 3,
};

static TAILQ_HEAD(kdhead, keydef) kdhead = TAILQ_HEAD_INITIALIZER(kdhead);

static void addkeydef(char *, int);
static void check(FILE *);
static int linecmp(const char **, const char **);
static int parse_flags(char **, int *, int);
static int parse_keydef(struct keydef *, char *, int);
static char *skipblank(char *);
static char *skipnonblank(char *);
static char *skipcolumn(char *, char *, int);
static size_t columns(char *, const struct keydef *, char **, size_t *);

static int Cflag = 0, cflag = 0, uflag = 0;
static char *fieldsep = NULL;
static char *col1, *col2;
static size_t col1siz, col2siz;

static void
addkeydef(char *kdstr, int flags)
{
	struct keydef *kd;

	kd = enmalloc(2, sizeof(*kd));
	if (parse_keydef(kd, kdstr, flags))
		enprintf(2, "faulty key definition\n");

	TAILQ_INSERT_TAIL(&kdhead, kd, entry);
}

static void
check(FILE *fp)
{
	static struct { char *buf; size_t size; } prev, cur, tmp;

	if (!prev.buf && getline(&prev.buf, &prev.size, fp) < 0)
		eprintf("getline:");
	while (getline(&cur.buf, &cur.size, fp) > 0) {
		if (uflag > linecmp((const char **) &cur.buf, (const char **) &prev.buf)) {
			if (!Cflag)
				weprintf("disorder: %s", cur.buf);
			exit(1);
		}
		tmp = cur;
		cur = prev;
		prev = tmp;
	}
}

static int
linecmp(const char **a, const char **b)
{
	int res = 0;
	long double x, y;
	struct keydef *kd;

	TAILQ_FOREACH(kd, &kdhead, entry) {
		columns((char *)*a, kd, &col1, &col1siz);
		columns((char *)*b, kd, &col2, &col2siz);

		/* if -u is given, don't use default key definition
		 * unless it is the only one */
		if (uflag && kd == TAILQ_LAST(&kdhead, kdhead) &&
		    TAILQ_LAST(&kdhead, kdhead) != TAILQ_FIRST(&kdhead)) {
			res = 0;
		} else if (kd->flags & MOD_N) {
			x = strtold(col1, NULL);
			y = strtold(col2, NULL);
			res = (x < y) ? (-1) : (x > y);
		} else {
			res = strcmp(col1, col2);
		}

		if (kd->flags & MOD_R)
			res = -res;
		if (res)
			break;
	}

	return res;
}

static int
parse_flags(char **s, int *flags, int bflag)
{
	while (isalpha((int)**s)) {
		switch (*((*s)++)) {
		case 'b':
			*flags |= bflag;
			break;
		case 'n':
			*flags |= MOD_N;
			break;
		case 'r':
			*flags |= MOD_R;
			break;
		default:
			return -1;
		}
	}

	return 0;
}

static int
parse_keydef(struct keydef *kd, char *s, int flags)
{
	char *rest = s;

	kd->start_column = 1;
	kd->start_char = 1;
	kd->end_column = 0; /* 0 means end of line */
	kd->end_char = 0;
	kd->flags = flags;

	kd->start_column = strtol(rest, &rest, 10);
	if (kd->start_column < 1)
		return -1;
	if (*rest == '.')
		kd->start_char = strtol(rest+1, &rest, 10);
	if (kd->start_char < 1)
		return -1;
	if (parse_flags(&rest, &kd->flags, MOD_STARTB) < 0)
		return -1;
	if (*rest == ',') {
		kd->end_column = strtol(rest+1, &rest, 10);
		if (kd->end_column < 1)
			return -1;
		if (*rest == '.') {
			kd->end_char = strtol(rest+1, &rest, 10);
			if (kd->end_char < 0)
				return -1;
		}
		if (parse_flags(&rest, &kd->flags, MOD_ENDB) < 0)
			return -1;
	}

	return -(*rest);
}

static char *
skipblank(char *s)
{
	while (isblank(*s))
		s++;
	return s;
}

static char *
skipnonblank(char *s)
{
	while (*s && *s != '\n' && !isblank(*s))
		s++;
	return s;
}

static char *
skipcolumn(char *s, char *eol, int next_col)
{
	if (fieldsep) {
		if ((s = strstr(s, fieldsep)))
			s += next_col ? strlen(fieldsep) : 0;
		else
			s = eol;
	} else {
		s = skipblank(s);
		s = skipnonblank(s);
	}
	return s;
}

static size_t
columns(char *line, const struct keydef *kd, char **col, size_t *colsiz)
{
	char *start, *end, *eol = strchr(line, '\n');
	size_t len;
	int i;

	for (i = 1, start = line; i < kd->start_column; i++)
		start = skipcolumn(start, eol, 1);
	if (kd->flags & MOD_STARTB)
		start = skipblank(start);
	start = MIN(eol, start + kd->start_char - 1);

	if (kd->end_column) {
		for (i = 1, end = line; i < kd->end_column; i++)
			end = skipcolumn(end, eol, 1);
		if (kd->flags & MOD_ENDB)
			end = skipblank(end);
		if (kd->end_char)
			end = MIN(eol, end + kd->end_char);
		else
			end = skipcolumn(end, eol, 0);
	} else {
		end = eol;
	}
	len = start > end ? 0 : end - start;
	if (!*col || *colsiz < len)
		*col = erealloc(*col, len + 1);
	memcpy(*col, start, len);
	(*col)[len] = '\0';
	if (*colsiz < len)
		*colsiz = len;
	return len;
}

static void
usage(void)
{
	enprintf(2, "usage: %s [-Cbcmnru] [-o outfile] [-t delim] [-k def]... [file ...]\n", argv0);
}

int
main(int argc, char *argv[])
{
	FILE *fp, *ofp = stdout;
	struct linebuf linebuf = EMPTY_LINEBUF;
	size_t i;
	int global_flags = 0, ret = 0;
	char *outfile = NULL;

	ARGBEGIN {
	case 'C':
		Cflag = 1;
		break;
	case 'b':
		global_flags |= MOD_STARTB | MOD_ENDB;
		break;
	case 'c':
		cflag = 1;
		break;
	case 'k':
		addkeydef(EARGF(usage()), global_flags);
		break;
	case 'm':
		/* more or less for free, but for performance-reasons,
		 * we should keep this flag in mind and maybe some later
		 * day implement it properly so we don't run out of memory
		 * while merging large sorted files.
		 */
		break;
	case 'n':
		global_flags |= MOD_N;
		break;
	case 'o':
		outfile = EARGF(usage());
		break;
	case 'r':
		global_flags |= MOD_R;
		break;
	case 't':
		fieldsep = EARGF(usage());
		break;
	case 'u':
		uflag = 1;
		break;
	default:
		usage();
	} ARGEND;

	/* -b shall only apply to custom key definitions */
	if (TAILQ_EMPTY(&kdhead) && global_flags)
		addkeydef("1", global_flags & ~(MOD_STARTB | MOD_ENDB));
	addkeydef("1", global_flags & MOD_R);

	if (!argc) {
		if (Cflag || cflag) {
			check(stdin);
		} else {
			getlines(stdin, &linebuf);
		}
	} else for (; *argv; argc--, argv++) {
		if (!strcmp(*argv, "-")) {
			*argv = "<stdin>";
			fp = stdin;
		} else if (!(fp = fopen(*argv, "r"))) {
			enprintf(2, "fopen %s:", *argv);
			continue;
		}
		if (Cflag || cflag) {
			check(fp);
		} else {
			getlines(fp, &linebuf);
		}
		if (fp != stdin && fshut(fp, *argv))
			ret = 1;
	}

	if (!Cflag && !cflag) {
		if (outfile && !(ofp = fopen(outfile, "w")))
			eprintf("fopen %s:", outfile);

		qsort(linebuf.lines, linebuf.nlines, sizeof *linebuf.lines,
				(int (*)(const void *, const void *))linecmp);

		for (i = 0; i < linebuf.nlines; i++) {
			if (!uflag || i == 0 || linecmp((const char **)&linebuf.lines[i],
						(const char **)&linebuf.lines[i-1])) {
				fputs(linebuf.lines[i], ofp);
			}
		}
	}

	if (fshut(stdin, "<stdin>") | fshut(stdout, "<stdout>"))
		ret = 2;

	return ret;
}
