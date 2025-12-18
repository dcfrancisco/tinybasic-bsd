/* --- Expression evaluation (CBM-ish) --- */

#define VAL_NUM 0
#define VAL_STR 1

typedef struct {
	int type;
	double num;
	char *str;
	int owned;
} Value;

static double eval_expr(BasicVm *vm, const char **p); /* forward */

static void value_release(v)
	Value *v;
{
	if (v->type == VAL_STR && v->owned && v->str != NULL)
		free(v->str);
	v->type = VAL_NUM;
	v->num = 0.0;
	v->str = NULL;
	v->owned = 0;
}

static Value value_num(x)
	double x;
{
	Value v;
	v.type = VAL_NUM;
	v.num = x;
	v.str = NULL;
	v.owned = 0;
	return v;
}

static Value value_str_ref(s)
	char *s;
{
	Value v;
	v.type = VAL_STR;
	v.num = 0.0;
	v.str = s;
	v.owned = 0;
	return v;
}

static Value value_str_own(s)
	char *s;
{
	Value v;
	v.type = VAL_STR;
	v.num = 0.0;
	v.str = s;
	v.owned = 1;
	return v;
}

static double value_to_num(vm, v)
	BasicVm *vm;
	Value *v;
{
	(void)vm;
	if (v->type == VAL_NUM)
		return v->num;
	if (v->str == NULL)
		return 0.0;
	return atof(v->str);
}

static char *num_to_str(x)
	double x;
{
	char buf[64];
	char *out;

	/* Not a perfect CBM formatter; keep compact and predictable */
	sprintf(buf, "%g", x);
	out = xstrdup(buf);
	return out;
}

static char *value_to_new_str(vm, v)
	BasicVm *vm;
	Value *v;
{
	(void)vm;
	if (v->type == VAL_STR) {
		if (v->str == NULL)
			return xstrdup("");
		return xstrdup(v->str);
	}
	return num_to_str(v->num);
}

static int parse_ident(p, buf, buflen, is_string)
	const char **p;
	char *buf;
	int buflen;
	int *is_string;
{
	const char *s;
	int n;

	s = *p;
	skip_ws(&s);
	if (!isalpha((unsigned char)*s))
		return 0;
	n = 0;
	while ((isalpha((unsigned char)*s) || isdigit((unsigned char)*s)) && n < buflen - 1) {
		buf[n++] = (char)toupper((unsigned char)*s);
		s++;
	}
	while (isalpha((unsigned char)*s) || isdigit((unsigned char)*s))
		s++;
	buf[n] = '\0';
	*is_string = 0;
	if (*s == '$') {
		*is_string = 1;
		s++;
	}
	*p = s;
	return 1;
}

static int ident_to_var_idx(name)
	const char *name;
{
	int a;
	int b;

	if (name == NULL || !isalpha((unsigned char)name[0]))
		return -1;
	a = (int)(toupper((unsigned char)name[0]) - 'A');
	b = 0;
	if (isalpha((unsigned char)name[1]))
		b = 1 + (int)(toupper((unsigned char)name[1]) - 'A');
	else if (isdigit((unsigned char)name[1]))
		b = 27 + (int)(name[1] - '0');
	return a * 37 + b;
}

static BasicArray *find_array(vm, idx, is_string)
	BasicVm *vm;
	int idx;
	int is_string;
{
	int i;

	for (i = 0; i < vm->narrays; i++) {
		if (vm->arrays[i].idx == idx && vm->arrays[i].is_string == is_string)
			return &vm->arrays[i];
	}
	return NULL;
}

static int array_alloc(vm, idx, is_string, d1, d2)
	BasicVm *vm;
	int idx;
	int is_string;
	int d1;
	int d2;
{
	BasicArray *na;
	int ncap;
	int cnt;

	if (d1 < 0)
		d1 = 0;
	if (d2 < 0)
		d2 = 0;
	if (vm->narrays >= vm->array_cap) {
		ncap = (vm->array_cap == 0) ? 16 : (vm->array_cap * 2);
		na = (BasicArray *)realloc(vm->arrays, (size_t)ncap * sizeof(BasicArray));
		if (na == NULL)
			return 0;
		vm->arrays = na;
		vm->array_cap = ncap;
	}
	vm->arrays[vm->narrays].idx = idx;
	vm->arrays[vm->narrays].is_string = is_string;
	vm->arrays[vm->narrays].d1 = d1;
	vm->arrays[vm->narrays].d2 = d2;
	vm->arrays[vm->narrays].nums = NULL;
	vm->arrays[vm->narrays].strs = NULL;
	cnt = (d1 + 1) * (d2 + 1);
	if (is_string) {
		vm->arrays[vm->narrays].strs = (char **)calloc((size_t)cnt, sizeof(char *));
		if (vm->arrays[vm->narrays].strs == NULL)
			return 0;
	} else {
		vm->arrays[vm->narrays].nums = (double *)calloc((size_t)cnt, sizeof(double));
		if (vm->arrays[vm->narrays].nums == NULL)
			return 0;
	}
	vm->narrays++;
	return 1;
}

static int ensure_array(vm, idx, is_string, d1, d2)
	BasicVm *vm;
	int idx;
	int is_string;
	int d1;
	int d2;
{
	BasicArray *a;

	a = find_array(vm, idx, is_string);
	if (a != NULL)
		return 1;
	return array_alloc(vm, idx, is_string, d1, d2);
}

static int parse_subscripts(vm, p, out1, out2, nd)
	BasicVm *vm;
	const char **p;
	int *out1;
	int *out2;
	int *nd;
{
	const char *s;
	double v1;
	double v2;

	(void)vm;
	s = *p;
	skip_ws(&s);
	if (*s != '(')
		return 0;
	s++;
	v1 = eval_expr(vm, &s);
	skip_ws(&s);
	*out1 = (int)v1;
	*out2 = 0;
	*nd = 1;
	if (*s == ',') {
		s++;
		v2 = eval_expr(vm, &s);
		skip_ws(&s);
		*out2 = (int)v2;
		*nd = 2;
	}
	if (*s == ')')
		s++;
	*p = s;
	return 1;
}

static unsigned long rnd_next(vm)
	BasicVm *vm;
{
	/* LCG; deterministic across platforms */
	vm->rnd_seed = vm->rnd_seed * 1103515245UL + 12345UL;
	return vm->rnd_seed;
}

static char *substr_copy(s, start, n)
	const char *s;
	int start;
	int n;
{
	int len;
	char *out;

	if (s == NULL)
		s = "";
	len = (int)strlen(s);
	if (start < 0)
		start = 0;
	if (start > len)
		start = len;
	if (n < 0)
		n = 0;
	if (start + n > len)
		n = len - start;
	out = (char *)xmalloc((size_t)n + 1);
	if (out == NULL)
		return xstrdup("");
	if (n > 0)
		memcpy(out, s + start, (size_t)n);
	out[n] = '\0';
	return out;
}

static int ensure_mem(vm)
	BasicVm *vm;
{
	if (vm->mem != NULL)
		return 1;
	vm->mem_size = 65536;
	vm->mem = (unsigned char *)calloc((size_t)vm->mem_size, 1);
	if (vm->mem == NULL) {
		vm->mem_size = 0;
		return 0;
	}
	return 1;
}

static Value eval_value(BasicVm *vm, const char **p);

static Value eval_primary(vm, p)
	BasicVm *vm;
	const char **p;
{
	const char *s;
	char *lit;
	char name[32];
	int is_string;
	int idx;

	s = *p;
	skip_ws(&s);
	if (*s == '(') {
		s++;
		{
			Value v;
			v = eval_value(vm, &s);
			skip_ws(&s);
			if (*s == ')')
				s++;
			*p = s;
			return v;
		}
	}
	lit = parse_string_lit(&s);
	if (lit != NULL) {
		*p = s;
		return value_str_own(lit);
	}
	if (isdigit((unsigned char)*s) || *s == '.') {
		double n;
		n = parse_number(&s);
		*p = s;
		return value_num(n);
	}
	if (parse_ident(&s, name, (int)sizeof(name), &is_string)) {
		/* Functions */
		if (!is_string && strcmp(name, "ABS") == 0) {
			Value v;
			double x;
			skip_ws(&s);
			if (*s != '(')
				return value_num(0.0);
			s++;
			v = eval_value(vm, &s);
			skip_ws(&s);
			if (*s == ')')
				s++;
			x = value_to_num(vm, &v);
			value_release(&v);
			*p = s;
			return value_num(fabs(x));
		}
		if (!is_string && strcmp(name, "INT") == 0) {
			Value v;
			skip_ws(&s);
			if (*s == '(') {
				s++;
				v = eval_value(vm, &s);
				skip_ws(&s);
				if (*s == ')')
					s++;
				{
					double x;
					x = value_to_num(vm, &v);
					value_release(&v);
					*p = s;
					return value_num(floor(x));
				}
			}
		}
		if (!is_string && strcmp(name, "SIN") == 0) {
			Value arg;
			double x;
			skip_ws(&s);
			if (*s != '(')
				return value_num(0.0);
			s++;
			arg = eval_value(vm, &s);
			skip_ws(&s);
			if (*s == ')')
				s++;
			x = value_to_num(vm, &arg);
			value_release(&arg);
			*p = s;
			return value_num(sin(x));
		}
		if (!is_string && strcmp(name, "COS") == 0) {
			Value arg;
			double x;
			skip_ws(&s);
			if (*s != '(')
				return value_num(0.0);
			s++;
			arg = eval_value(vm, &s);
			skip_ws(&s);
			if (*s == ')')
				s++;
			x = value_to_num(vm, &arg);
			value_release(&arg);
			*p = s;
			return value_num(cos(x));
		}
		if (!is_string && strcmp(name, "TAN") == 0) {
			Value arg;
			double x;
			skip_ws(&s);
			if (*s != '(')
				return value_num(0.0);
			s++;
			arg = eval_value(vm, &s);
			skip_ws(&s);
			if (*s == ')')
				s++;
			x = value_to_num(vm, &arg);
			value_release(&arg);
			*p = s;
			return value_num(tan(x));
		}
		if (!is_string && strcmp(name, "ATN") == 0) {
			Value arg;
			double x;
			skip_ws(&s);
			if (*s != '(')
				return value_num(0.0);
			s++;
			arg = eval_value(vm, &s);
			skip_ws(&s);
			if (*s == ')')
				s++;
			x = value_to_num(vm, &arg);
			value_release(&arg);
			*p = s;
			return value_num(atan(x));
		}
		if (!is_string && strcmp(name, "SQR") == 0) {
			Value arg;
			double x;
			skip_ws(&s);
			if (*s != '(')
				return value_num(0.0);
			s++;
			arg = eval_value(vm, &s);
			skip_ws(&s);
			if (*s == ')')
				s++;
			x = value_to_num(vm, &arg);
			value_release(&arg);
			if (x < 0.0)
				x = 0.0;
			*p = s;
			return value_num(sqrt(x));
		}
		if (!is_string && strcmp(name, "LOG") == 0) {
			Value arg;
			double x;
			skip_ws(&s);
			if (*s != '(')
				return value_num(0.0);
			s++;
			arg = eval_value(vm, &s);
			skip_ws(&s);
			if (*s == ')')
				s++;
			x = value_to_num(vm, &arg);
			value_release(&arg);
			if (x <= 0.0)
				x = 1.0;
			*p = s;
			return value_num(log(x));
		}
		if (!is_string && strcmp(name, "EXP") == 0) {
			Value arg;
			double x;
			skip_ws(&s);
			if (*s != '(')
				return value_num(0.0);
			s++;
			arg = eval_value(vm, &s);
			skip_ws(&s);
			if (*s == ')')
				s++;
			x = value_to_num(vm, &arg);
			value_release(&arg);
			*p = s;
			return value_num(exp(x));
		}
		if (!is_string && strcmp(name, "SGN") == 0) {
			Value arg;
			double x;
			skip_ws(&s);
			if (*s != '(')
				return value_num(0.0);
			s++;
			arg = eval_value(vm, &s);
			skip_ws(&s);
			if (*s == ')')
				s++;
			x = value_to_num(vm, &arg);
			value_release(&arg);
			*p = s;
			if (x < 0.0)
				return value_num(-1.0);
			if (x > 0.0)
				return value_num(1.0);
			return value_num(0.0);
		}
		if (!is_string && strcmp(name, "RND") == 0) {
			Value arg;
			double x;
			double r;
			arg = value_num(1.0);
			skip_ws(&s);
			if (*s == '(') {
				s++;
				arg = eval_value(vm, &s);
				skip_ws(&s);
				if (*s == ')')
					s++;
			}
			x = value_to_num(vm, &arg);
			value_release(&arg);
			if (x < 0.0)
				vm->rnd_seed = (unsigned long)(-x);
			if (x != 0.0)
				(void)rnd_next(vm);
			r = (double)(rnd_next(vm) & 0x7fffffffUL) / 2147483648.0;
			*p = s;
			return value_num(r);
		}
		if (!is_string && strcmp(name, "LEN") == 0) {
			Value arg;
			char *ss;
			long n;
			skip_ws(&s);
			if (*s != '(')
				return value_num(0.0);
			s++;
			arg = eval_value(vm, &s);
			skip_ws(&s);
			if (*s == ')')
				s++;
			ss = value_to_new_str(vm, &arg);
			value_release(&arg);
			n = (ss == NULL) ? 0L : (long)strlen(ss);
			if (ss != NULL)
				free(ss);
			*p = s;
			return value_num((double)n);
		}
		if (!is_string && strcmp(name, "VAL") == 0) {
			Value arg;
			double n;
			skip_ws(&s);
			if (*s != '(')
				return value_num(0.0);
			s++;
			arg = eval_value(vm, &s);
			skip_ws(&s);
			if (*s == ')')
				s++;
			n = value_to_num(vm, &arg);
			value_release(&arg);
			*p = s;
			return value_num(n);
		}
		if (is_string && strcmp(name, "STR") == 0) {
			Value arg;
			char *out;
			skip_ws(&s);
			if (*s != '(')
				return value_str_own(xstrdup(""));
			s++;
			arg = eval_value(vm, &s);
			skip_ws(&s);
			if (*s == ')')
				s++;
			out = num_to_str(value_to_num(vm, &arg));
			value_release(&arg);
			*p = s;
			return value_str_own(out);
		}
		if (is_string && strcmp(name, "CHR") == 0) {
			Value arg;
			char *out;
			int c;
			skip_ws(&s);
			if (*s != '(')
				return value_str_own(xstrdup(""));
			s++;
			arg = eval_value(vm, &s);
			skip_ws(&s);
			if (*s == ')')
				s++;
			c = (int)value_to_num(vm, &arg);
			value_release(&arg);
			out = (char *)xmalloc(2);
			if (out == NULL)
				return value_str_own(xstrdup(""));
			out[0] = (char)(c & 0xff);
			out[1] = '\0';
			*p = s;
			return value_str_own(out);
		}
		if (!is_string && strcmp(name, "ASC") == 0) {
			Value arg;
			char *ss;
			int c;
			skip_ws(&s);
			if (*s != '(')
				return value_num(0.0);
			s++;
			arg = eval_value(vm, &s);
			skip_ws(&s);
			if (*s == ')')
				s++;
			ss = value_to_new_str(vm, &arg);
			value_release(&arg);
			c = 0;
			if (ss != NULL && ss[0] != '\0')
				c = (unsigned char)ss[0];
			if (ss != NULL)
				free(ss);
			*p = s;
			return value_num((double)c);
		}
		if (is_string && strcmp(name, "LEFT") == 0) {
			Value a1;
			Value a2;
			char *ss;
			int n;
			char *out;
			skip_ws(&s);
			if (*s != '(')
				return value_str_own(xstrdup(""));
			s++;
			a1 = eval_value(vm, &s);
			skip_ws(&s);
			if (*s != ',') {
				value_release(&a1);
				return value_str_own(xstrdup(""));
			}
			s++;
			a2 = eval_value(vm, &s);
			skip_ws(&s);
			if (*s == ')')
				s++;
			ss = value_to_new_str(vm, &a1);
			n = (int)value_to_num(vm, &a2);
			value_release(&a1);
			value_release(&a2);
			out = substr_copy(ss, 0, n);
			if (ss != NULL)
				free(ss);
			*p = s;
			return value_str_own(out);
		}
		if (is_string && strcmp(name, "RIGHT") == 0) {
			Value a1;
			Value a2;
			char *ss;
			int n;
			char *out;
			skip_ws(&s);
			if (*s != '(')
				return value_str_own(xstrdup(""));
			s++;
			a1 = eval_value(vm, &s);
			skip_ws(&s);
			if (*s != ',') {
				value_release(&a1);
				return value_str_own(xstrdup(""));
			}
			s++;
			a2 = eval_value(vm, &s);
			skip_ws(&s);
			if (*s == ')')
				s++;
			ss = value_to_new_str(vm, &a1);
			n = (int)value_to_num(vm, &a2);
			value_release(&a1);
			value_release(&a2);
			if (ss == NULL)
				ss = xstrdup("");
			out = substr_copy(ss, (int)strlen(ss) - n, n);
			if (ss != NULL)
				free(ss);
			*p = s;
			return value_str_own(out);
		}
		if (is_string && strcmp(name, "MID") == 0) {
			Value a1;
			Value a2;
			Value a3;
			char *ss;
			int pos;
			int n;
			int len;
			char *out;
			int have_len;
			skip_ws(&s);
			if (*s != '(')
				return value_str_own(xstrdup(""));
			s++;
			a1 = eval_value(vm, &s);
			skip_ws(&s);
			if (*s != ',') {
				value_release(&a1);
				return value_str_own(xstrdup(""));
			}
			s++;
			a2 = eval_value(vm, &s);
			have_len = 0;
			skip_ws(&s);
			if (*s == ',') {
				s++;
				a3 = eval_value(vm, &s);
				have_len = 1;
			}
			skip_ws(&s);
			if (*s == ')')
				s++;
			ss = value_to_new_str(vm, &a1);
			pos = (int)value_to_num(vm, &a2);
			value_release(&a1);
			value_release(&a2);
			if (ss == NULL)
				ss = xstrdup("");
			len = (int)strlen(ss);
			if (pos < 1)
				pos = 1;
			pos--; /* 0-based */
			if (pos > len)
				pos = len;
			if (have_len) {
				n = (int)value_to_num(vm, &a3);
				value_release(&a3);
				if (n < 0)
					n = 0;
			} else {
				n = len - pos;
			}
			if (pos + n > len)
				n = len - pos;
			out = substr_copy(ss, pos, n);
			if (ss != NULL)
				free(ss);
			*p = s;
			return value_str_own(out);
		}
		if (!is_string && strcmp(name, "PEEK") == 0) {
			Value arg;
			int addr;
			skip_ws(&s);
			if (*s != '(')
				return value_num(0.0);
			s++;
			arg = eval_value(vm, &s);
			skip_ws(&s);
			if (*s == ')')
				s++;
			addr = (int)value_to_num(vm, &arg);
			value_release(&arg);
			if (addr < 0)
				addr = 0;
			if (addr > 65535)
				addr = 65535;
			if (!ensure_mem(vm)) {
				*p = s;
				return value_num(0.0);
			}
			*p = s;
			return value_num((double)vm->mem[addr]);
		}

		/* Variable or array */
		idx = ident_to_var_idx(name);
		if (idx >= 0 && idx < 26 * 37) {
			BasicArray *a;
			int sub1, sub2, nd;
			skip_ws(&s);
			if (*s == '(') {
				/* array element; auto-DIM to 10 if needed */
				if (!parse_subscripts(vm, &s, &sub1, &sub2, &nd))
					return value_num(0.0);
				a = find_array(vm, idx, is_string);
				if (a == NULL) {
					if (!ensure_array(vm, idx, is_string, 10, (nd == 2) ? 10 : 0))
						return value_num(0.0);
					a = find_array(vm, idx, is_string);
				}
				if (a == NULL)
					return value_num(0.0);
				if (sub1 < 0 || sub1 > a->d1)
					return value_num(0.0);
				if (nd == 2 && (sub2 < 0 || sub2 > a->d2))
					return value_num(0.0);
				if (nd == 1)
					sub2 = 0;
				if (is_string) {
					int off;
					off = sub1 * (a->d2 + 1) + sub2;
					*p = s;
					return value_str_ref(a->strs[off] ? a->strs[off] : (char *)"");
				}
				else {
					int off;
					off = sub1 * (a->d2 + 1) + sub2;
					*p = s;
					return value_num(a->nums[off]);
				}
			}
			/* scalar */
			*p = s;
			if (is_string)
				return value_str_ref(vm->svars[idx] ? vm->svars[idx] : (char *)"");
			if (!vm->nset[idx])
				vm->nvars[idx] = 0.0;
			vm->nset[idx] = 1;
			return value_num(vm->nvars[idx]);
		}
	}
	/* unknown */
	*p = s;
	return value_num(0.0);
}

static Value eval_unary(vm, p)
	BasicVm *vm;
	const char **p;
{
	const char *s;
	Value v;

	s = *p;
	skip_ws(&s);
	if (*s == '+') {
		s++;
		v = eval_unary(vm, &s);
		*p = s;
		return v;
	}
	if (*s == '-') {
		s++;
		v = eval_unary(vm, &s);
		if (v.type == VAL_STR) {
			double n;
			n = -value_to_num(vm, &v);
			value_release(&v);
			*p = s;
			return value_num(n);
		}
		v.num = -v.num;
		*p = s;
		return v;
	}
	if (match_kw(&s, "NOT")) {
		v = eval_unary(vm, &s);
		{
			long a;
			a = (long)value_to_num(vm, &v);
			value_release(&v);
			*p = s;
			return value_num((double)(~a));
		}
	}
	v = eval_primary(vm, &s);
	*p = s;
	return v;
}

static Value eval_mul(vm, p)
	BasicVm *vm;
	const char **p;
{
	const char *s;
	Value v;
	Value rhs;
	char op;

	s = *p;
	v = eval_unary(vm, &s);
	for (;;) {
		skip_ws(&s);
		op = *s;
		if (op != '*' && op != '/')
			break;
		s++;
		rhs = eval_unary(vm, &s);
		{
			double a;
			double b;
			a = value_to_num(vm, &v);
			b = value_to_num(vm, &rhs);
			value_release(&v);
			value_release(&rhs);
			if (op == '*')
				v = value_num(a * b);
			else
				v = value_num((b != 0.0) ? (a / b) : 0.0);
		}
	}
	*p = s;
	return v;
}

static Value eval_add(vm, p)
	BasicVm *vm;
	const char **p;
{
	const char *s;
	Value v;
	Value rhs;
	char op;

	s = *p;
	v = eval_mul(vm, &s);
	for (;;) {
		skip_ws(&s);
		op = *s;
		if (op != '+' && op != '-')
			break;
		s++;
		rhs = eval_mul(vm, &s);
		if (op == '+' && (v.type == VAL_STR || rhs.type == VAL_STR)) {
			char *a;
			char *b;
			char *out;
			size_t na;
			size_t nb;
			a = value_to_new_str(vm, &v);
			b = value_to_new_str(vm, &rhs);
			na = (a == NULL) ? 0 : strlen(a);
			nb = (b == NULL) ? 0 : strlen(b);
			out = (char *)xmalloc(na + nb + 1);
			if (out == NULL)
				out = xstrdup("");
			else {
				if (a != NULL)
					memcpy(out, a, na);
				if (b != NULL)
					memcpy(out + na, b, nb);
				out[na + nb] = '\0';
			}
			if (a != NULL)
				free(a);
			if (b != NULL)
				free(b);
			value_release(&v);
			value_release(&rhs);
			v = value_str_own(out);
			continue;
		}
		{
			double a;
			double b;
			a = value_to_num(vm, &v);
			b = value_to_num(vm, &rhs);
			value_release(&v);
			value_release(&rhs);
			v = value_num((op == '+') ? (a + b) : (a - b));
		}
	}
	*p = s;
	return v;
}

static int value_cmp(vm, a, b)
	BasicVm *vm;
	Value *a;
	Value *b;
{
	if (a->type == VAL_STR || b->type == VAL_STR) {
		char *sa;
		char *sb;
		int r;
		sa = value_to_new_str(vm, a);
		sb = value_to_new_str(vm, b);
		r = strcmp(sa ? sa : "", sb ? sb : "");
		if (sa != NULL)
			free(sa);
		if (sb != NULL)
			free(sb);
		return r;
	}
	if (a->num < b->num)
		return -1;
	if (a->num > b->num)
		return 1;
	return 0;
}

static Value eval_rel(vm, p)
	BasicVm *vm;
	const char **p;
{
	const char *s;
	Value a;
	Value b;
	int r;

	s = *p;
	a = eval_add(vm, &s);
	skip_ws(&s);
	if (*s == '=' || *s == '<' || *s == '>') {
		int ok;
		ok = 0;
		if (*s == '=') {
			s++;
			b = eval_add(vm, &s);
			r = value_cmp(vm, &a, &b);
			ok = (r == 0);
			value_release(&b);
		} else if (*s == '<') {
			s++;
			if (*s == '=') {
				s++;
				b = eval_add(vm, &s);
				r = value_cmp(vm, &a, &b);
				ok = (r <= 0);
				value_release(&b);
			} else if (*s == '>') {
				s++;
				b = eval_add(vm, &s);
				r = value_cmp(vm, &a, &b);
				ok = (r != 0);
				value_release(&b);
			} else {
				b = eval_add(vm, &s);
				r = value_cmp(vm, &a, &b);
				ok = (r < 0);
				value_release(&b);
			}
		} else {
			s++;
			if (*s == '=') {
				s++;
				b = eval_add(vm, &s);
				r = value_cmp(vm, &a, &b);
				ok = (r >= 0);
				value_release(&b);
			} else {
				b = eval_add(vm, &s);
				r = value_cmp(vm, &a, &b);
				ok = (r > 0);
				value_release(&b);
			}
		}
		value_release(&a);
		*p = s;
		return value_num(ok ? -1.0 : 0.0);
	}
	/* no relop: pass-through value */
	*p = s;
	return a;
}

static Value eval_and(vm, p)
	BasicVm *vm;
	const char **p;
{
	const char *s;
	Value v;
	Value rhs;

	s = *p;
	v = eval_rel(vm, &s);
	for (;;) {
		if (!match_kw(&s, "AND"))
			break;
		rhs = eval_rel(vm, &s);
		{
			long a;
			long b;
			a = (long)value_to_num(vm, &v);
			b = (long)value_to_num(vm, &rhs);
			value_release(&v);
			value_release(&rhs);
			v = value_num((double)(a & b));
		}
	}
	*p = s;
	return v;
}

static Value eval_or(vm, p)
	BasicVm *vm;
	const char **p;
{
	const char *s;
	Value v;
	Value rhs;

	s = *p;
	v = eval_and(vm, &s);
	for (;;) {
		if (!match_kw(&s, "OR"))
			break;
		rhs = eval_and(vm, &s);
		{
			long a;
			long b;
			a = (long)value_to_num(vm, &v);
			b = (long)value_to_num(vm, &rhs);
			value_release(&v);
			value_release(&rhs);
			v = value_num((double)(a | b));
		}
	}
	*p = s;
	return v;
}

static Value eval_value(vm, p)
	BasicVm *vm;
	const char **p;
{
	return eval_or(vm, p);
}

static double eval_expr(vm, p)
	BasicVm *vm;
	const char **p;
{
	Value v;
	double n;

	v = eval_value(vm, p);
	n = value_to_num(vm, &v);
	value_release(&v);
	return n;
}

static int eval_relop(vm, p)
	BasicVm *vm;
	const char **p;
{
	const char *s;
	Value a;
	Value b;
	int r;
	int ok;
	double n;

	s = *p;
	a = eval_add(vm, &s);
	skip_ws(&s);
	ok = 0;
	if (*s == '=' || *s == '<' || *s == '>') {
		if (*s == '=') {
			s++;
			b = eval_add(vm, &s);
			r = value_cmp(vm, &a, &b);
			ok = (r == 0);
			value_release(&b);
		} else if (*s == '<') {
			s++;
			if (*s == '=') {
				s++;
				b = eval_add(vm, &s);
				r = value_cmp(vm, &a, &b);
				ok = (r <= 0);
				value_release(&b);
			} else if (*s == '>') {
				s++;
				b = eval_add(vm, &s);
				r = value_cmp(vm, &a, &b);
				ok = (r != 0);
				value_release(&b);
			} else {
				b = eval_add(vm, &s);
				r = value_cmp(vm, &a, &b);
				ok = (r < 0);
				value_release(&b);
			}
		} else {
			s++;
			if (*s == '=') {
				s++;
				b = eval_add(vm, &s);
				r = value_cmp(vm, &a, &b);
				ok = (r >= 0);
				value_release(&b);
			} else {
				b = eval_add(vm, &s);
				r = value_cmp(vm, &a, &b);
				ok = (r > 0);
				value_release(&b);
			}
		}
		value_release(&a);
		*p = s;
		return ok;
	}
	n = value_to_num(vm, &a);
	value_release(&a);
	*p = s;
	return (n != 0.0);
}

static int line_cmp(a, b)
	const void *a;
	const void *b;
{
	const ProgLine *pa;
	const ProgLine *pb;

	pa = (const ProgLine *)a;
	pb = (const ProgLine *)b;
	return pa->line - pb->line;
}

static int add_line(vm, line, text)
	BasicVm *vm;
	int line;
	const char *text;
{
	ProgLine *nl;
	char *cpy;
	int ncap;

	if (vm->nlines >= vm->cap) {
		ncap = (vm->cap == 0) ? 128 : (vm->cap * 2);
		nl = (ProgLine *)realloc(vm->lines, (size_t)ncap * sizeof(ProgLine));
		if (nl == NULL)
			return 0;
		vm->lines = nl;
		vm->cap = ncap;
	}
	cpy = xstrdup(text);
	if (cpy == NULL)
		return 0;
	vm->lines[vm->nlines].line = line;
	vm->lines[vm->nlines].text = cpy;
	vm->nlines++;
	return 1;
}

static int data_add(vm, line, is_string, num, str)
	BasicVm *vm;
	int line;
	int is_string;
	double num;
	char *str;
{
	DataItem *nd;
	int ncap;

	if (vm->ndata >= vm->data_cap) {
		ncap = (vm->data_cap == 0) ? 64 : (vm->data_cap * 2);
		nd = (DataItem *)realloc(vm->data, (size_t)ncap * sizeof(DataItem));
		if (nd == NULL)
			return 0;
		vm->data = nd;
		vm->data_cap = ncap;
	}
	vm->data[vm->ndata].line = line;
	vm->data[vm->ndata].is_string = is_string;
	vm->data[vm->ndata].num = num;
	vm->data[vm->ndata].str = str;
	vm->ndata++;
	return 1;
}

static int token_is_number(tok)
	const char *tok;
{
	const char *s;
	int saw_digit;

	if (tok == NULL)
		return 0;
	s = tok;
	while (*s == ' ' || *s == '\t')
		s++;
	if (*s == '+' || *s == '-')
		s++;
	saw_digit = 0;
	while (isdigit((unsigned char)*s)) {
		saw_digit = 1;
		s++;
	}
	if (*s == '.') {
		s++;
		while (isdigit((unsigned char)*s)) {
			saw_digit = 1;
			s++;
		}
	}
	while (*s == ' ' || *s == '\t')
		s++;
	return saw_digit && *s == '\0';
}

static void skip_stmt_tail(p)
	const char **p;
{
	const char *s;

	s = *p;
	while (*s && *s != ':') {
		if (*s == '"') {
			s++;
			while (*s && *s != '"')
				s++;
			if (*s == '"')
				s++;
			continue;
		}
		s++;
	}
	*p = s;
}

static int collect_data(vm)
	BasicVm *vm;
{
	int i;
	const char *s;
	int line;

	vm->ndata = 0;
	vm->data_pos = 0;
	for (i = 0; i < vm->nlines; i++) {
		line = vm->lines[i].line;
		s = vm->lines[i].text;
		for (;;) {
			skip_ws(&s);
			if (*s == '\0')
				break;
			if (match_kw(&s, "REM")) {
				skip_stmt_tail(&s);
			} else if (match_kw(&s, "DATA")) {
				for (;;) {
					char *lit;
					char tok[256];
					int n;
					const char *t;
					skip_ws(&s);
					if (*s == '\0' || *s == ':')
						break;
					lit = parse_string_lit(&s);
					if (lit != NULL) {
						if (!data_add(vm, line, 1, 0.0, lit)) {
							free(lit);
							return 0;
						}
						skip_ws(&s);
						if (*s == ',') {
							s++;
							continue;
						}
						break;
					}
					/* unquoted token */
					t = s;
					n = 0;
					while (*t && *t != ',' && *t != ':') {
						if (n < (int)sizeof(tok) - 1)
							tok[n++] = *t;
						t++;
					}
					tok[n] = '\0';
					s = t;
					/* trim leading/trailing */
					{
						char *a;
						char *b;
						a = tok;
						while (*a == ' ' || *a == '\t')
							a++;
						b = a + strlen(a);
						while (b > a && (b[-1] == ' ' || b[-1] == '\t'))
							b--;
						*b = '\0';
						if (token_is_number(a)) {
							if (!data_add(vm, line, 0, atof(a), NULL))
								return 0;
						} else {
							char *ss;
							ss = xstrdup(a);
							if (ss == NULL)
								return 0;
							if (!data_add(vm, line, 1, 0.0, ss)) {
								free(ss);
								return 0;
							}
						}
					}
					skip_ws(&s);
					if (*s == ',') {
						s++;
						continue;
					}
					break;
				}
			} else {
				skip_stmt_tail(&s);
			}
			skip_ws(&s);
			if (*s == ':') {
				s++;
				continue;
			}
			break;
		}
	}
	return 1;
}

static int find_line(vm, line)
	BasicVm *vm;
	int line;
{
	int lo;
	int hi;
	int mid;
	int v;

	lo = 0;
	hi = vm->nlines - 1;
	while (lo <= hi) {
		mid = (lo + hi) / 2;
		v = vm->lines[mid].line;
		if (v == line)
			return mid;
		if (v < line)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return -1;
}

static int jump_to(vm, target, pc, p)
	BasicVm *vm;
	int target;
	int *pc;
	const char **p;
{
	int npc;

	npc = find_line(vm, target);
	if (npc < 0)
		return 0;
	*pc = npc;
	*p = vm->lines[npc].text;
	return 1;
}

static int runtime_error(vm, msg)
	BasicVm *vm;
	const char *msg;
{
	vm->had_error = 1;
	fprintf(stderr, "?%s\n", msg);
	return 0;
}

typedef struct {
	int idx;
	int is_string;
	int is_array;
	int sub1;
	int sub2;
	int nd;
} LValue;

static int parse_lvalue(vm, p, lv)
	BasicVm *vm;
	const char **p;
	LValue *lv;
{
	const char *s;
	char name[32];
	int is_string;
	int idx;
	int sub1;
	int sub2;
	int nd;

	(void)vm;
	s = *p;
	if (!parse_ident(&s, name, (int)sizeof(name), &is_string))
		return 0;
	idx = ident_to_var_idx(name);
	if (idx < 0 || idx >= 26 * 37)
		return 0;
	lv->idx = idx;
	lv->is_string = is_string;
	lv->is_array = 0;
	lv->sub1 = 0;
	lv->sub2 = 0;
	lv->nd = 0;
	skip_ws(&s);
	if (*s == '(') {
		if (!parse_subscripts(vm, &s, &sub1, &sub2, &nd))
			return 0;
		lv->is_array = 1;
		lv->sub1 = sub1;
		lv->sub2 = (nd == 2) ? sub2 : 0;
		lv->nd = nd;
	}
	*p = s;
	return 1;
}

static int set_lvalue(vm, lv, v)
	BasicVm *vm;
	LValue *lv;
	Value *v;
{
	if (!lv->is_array) {
		if (lv->is_string) {
			char *ns;
			ns = value_to_new_str(vm, v);
			if (ns == NULL)
				ns = xstrdup("");
			if (vm->svars[lv->idx] != NULL)
				free(vm->svars[lv->idx]);
			vm->svars[lv->idx] = ns;
			return 1;
		}
		vm->nvars[lv->idx] = value_to_num(vm, v);
		vm->nset[lv->idx] = 1;
		return 1;
	}
	/* array element */
	{
		BasicArray *a;
		int off;
		a = find_array(vm, lv->idx, lv->is_string);
		if (a == NULL) {
			if (!ensure_array(vm, lv->idx, lv->is_string, 10, (lv->nd == 2) ? 10 : 0))
				return 0;
			a = find_array(vm, lv->idx, lv->is_string);
		}
		if (a == NULL)
			return 0;
		if (lv->sub1 < 0 || lv->sub1 > a->d1)
			return 0;
		if (lv->nd == 2 && (lv->sub2 < 0 || lv->sub2 > a->d2))
			return 0;
		off = lv->sub1 * (a->d2 + 1) + lv->sub2;
		if (lv->is_string) {
			char *ns;
			ns = value_to_new_str(vm, v);
			if (ns == NULL)
				ns = xstrdup("");
			if (a->strs[off] != NULL)
				free(a->strs[off]);
			a->strs[off] = ns;
			return 1;
		}
		a->nums[off] = value_to_num(vm, v);
		return 1;
	}
}

static int gosub_push(vm, pc, s)
	BasicVm *vm;
	int pc;
	const char *s;
{
	RetAddr *nr;
	int ncap;

	if (vm->gosub_sp >= vm->gosub_cap) {
		ncap = (vm->gosub_cap == 0) ? 32 : (vm->gosub_cap * 2);
		nr = (RetAddr *)realloc(vm->gosub, (size_t)ncap * sizeof(RetAddr));
		if (nr == NULL)
			return 0;
		vm->gosub = nr;
		vm->gosub_cap = ncap;
	}
	vm->gosub[vm->gosub_sp].pc = pc;
	vm->gosub[vm->gosub_sp].s = s;
	vm->gosub_sp++;
	return 1;
}

static int gosub_pop(vm, pc, s)
	BasicVm *vm;
	int *pc;
	const char **s;
{
	if (vm->gosub_sp <= 0)
		return 0;
	vm->gosub_sp--;
	*pc = vm->gosub[vm->gosub_sp].pc;
	*s = vm->gosub[vm->gosub_sp].s;
	return 1;
}

static int for_push(vm, f)
	BasicVm *vm;
	ForFrame *f;
{
	ForFrame *nf;
	int ncap;

	if (vm->for_sp >= vm->for_cap) {
		ncap = (vm->for_cap == 0) ? 32 : (vm->for_cap * 2);
		nf = (ForFrame *)realloc(vm->fors, (size_t)ncap * sizeof(ForFrame));
		if (nf == NULL)
			return 0;
		vm->fors = nf;
		vm->for_cap = ncap;
	}
	vm->fors[vm->for_sp] = *f;
	vm->for_sp++;
	return 1;
}

