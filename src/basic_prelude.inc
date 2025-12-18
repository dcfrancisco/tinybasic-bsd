#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__) || defined(PDP11)
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#define HAVE_SELECT 1
#else
#define HAVE_SELECT 0
#endif

#include "basic.h"

typedef struct {
	int line;
	char *text;
} ProgLine;

typedef struct {
	int pc;
	const char *s;
} RetAddr;

typedef struct {
	int var_idx;
	double endv;
	double step;
	int pc;
	const char *s;
} ForFrame;

typedef struct {
	int is_string;
	int line;
	double num;
	char *str;
} DataItem;

typedef struct {
	int idx;
	int is_string;
	int d1;
	int d2;
	/* element count is (d1+1) * (d2+1) */
	double *nums;
	char **strs;
} BasicArray;

struct BasicVm {
	ProgLine *lines;
	int nlines;
	int cap;

	double nvars[26 * 37];
	unsigned char nset[26 * 37];
	char *svars[26 * 37];

	RetAddr *gosub;
	int gosub_sp;
	int gosub_cap;

	ForFrame *fors;
	int for_sp;
	int for_cap;

	DataItem *data;
	int ndata;
	int data_cap;
	int data_pos;

	BasicArray *arrays;
	int narrays;
	int array_cap;

	unsigned long rnd_seed;
	int had_error;

	unsigned char *mem;
	int mem_size;
};

static void vm_clear_runtime(BasicVm *vm);

static void *xmalloc(n)
	size_t n;
{
	void *p;

	p = malloc(n);
	return p;
}

static char *xstrdup(s)
	const char *s;
{
	size_t n;
	char *p;

	n = strlen(s);
	p = (char *)xmalloc(n + 1);
	if (p == NULL)
		return NULL;
	memcpy(p, s, n + 1);
	return p;
}

static void skip_ws(p)
	const char **p;
{
	const char *s;

	s = *p;
	while (*s == ' ' || *s == '\t')
		s++;
	*p = s;
}

static int match_kw(p, kw)
	const char **p;
	const char *kw;
{
	const char *s;
	const char *k;

	s = *p;
	skip_ws(&s);
	k = kw;
	while (*k && *s) {
		if (toupper((unsigned char)*s) != toupper((unsigned char)*k))
			return 0;
		s++;
		k++;
	}
	if (*k != '\0')
		return 0;
	if (isalnum((unsigned char)*s) || *s == '$')
		return 0;
	*p = s;
	return 1;
}

static double parse_number(p)
	const char **p;
{
	char tmp[64];
	int i;
	const char *s;

	s = *p;
	skip_ws(&s);
	i = 0;
	if (*s == '+' || *s == '-') {
		if (i < (int)sizeof(tmp) - 1)
			tmp[i++] = *s;
		s++;
	}
	while (isdigit((unsigned char)*s) || *s == '.') {
		if (i < (int)sizeof(tmp) - 1)
			tmp[i++] = *s;
		s++;
	}
	tmp[i] = '\0';
	*p = s;
	return atof(tmp);
}

static char *parse_string_lit(p)
	const char **p;
{
	const char *s;
	char *out;
	char *d;
	int n;

	s = *p;
	skip_ws(&s);
	if (*s != '"')
		return NULL;
	s++;
	n = 0;
	while (s[n] && s[n] != '"')
		n++;
	out = (char *)xmalloc((size_t)n + 1);
	if (out == NULL)
		return NULL;
	d = out;
	while (*s && *s != '"')
		*d++ = *s++;
	*d = '\0';
	if (*s == '"')
		s++;
	*p = s;
	return out;
}

