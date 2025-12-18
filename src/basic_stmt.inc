static int print_num(v)
	double v;
{
	char buf[64];
	int n;

	/* CBM prints a leading space for positives */
	n = 0;
	if (v >= 0.0) {
		putchar(' ');
		n++;
	}
	sprintf(buf, "%g", v);
	fputs(buf, stdout);
	n += (int)strlen(buf);
	return n;
}

static int print_spaces(n)
	int n;
{
	int i;

	if (n <= 0)
		return 0;
	for (i = 0; i < n; i++)
		putchar(' ');
	return n;
}

static int exec_print(vm, p)
	BasicVm *vm;
	const char **p;
{
	const char *s;
	int last_sep;
	int col;

	s = *p;
	last_sep = 0;
	col = 0;
	for (;;) {
		skip_ws(&s);
		if (*s == '\0' || *s == ':')
			break;
		if (*s == ',') {
			/* crude zone advance */
			putchar('\t');
			col = ((col / 8) + 1) * 8;
			s++;
			last_sep = 1;
			continue;
		}
		if (*s == ';') {
			s++;
			last_sep = 1;
			continue;
		}
		if (match_kw(&s, "TAB")) {
			int target;
			int nsp;
			skip_ws(&s);
			if (*s != '(')
				return 0;
			s++;
			target = (int)eval_expr(vm, &s);
			skip_ws(&s);
			if (*s == ')')
				s++;
			nsp = target - col;
			if (nsp <= 0)
				nsp = 1;
			col += print_spaces(nsp);
			last_sep = 0;
			continue;
		}
		if (match_kw(&s, "SPC")) {
			int nsp;
			skip_ws(&s);
			if (*s != '(')
				return 0;
			s++;
			nsp = (int)eval_expr(vm, &s);
			skip_ws(&s);
			if (*s == ')')
				s++;
			if (nsp < 0)
				nsp = 0;
			col += print_spaces(nsp);
			last_sep = 0;
			continue;
		}
		{
			Value v;
			v = eval_value(vm, &s);
			if (v.type == VAL_STR) {
				if (v.str != NULL) {
					fputs(v.str, stdout);
					col += (int)strlen(v.str);
				}
			} else {
				col += print_num(v.num);
			}
			value_release(&v);
		}
		last_sep = 0;
	}
	if (!last_sep)
		putchar('\n');
	*p = s;
	return 1;
}

static int exec_let(vm, p)
	BasicVm *vm;
	const char **p;
{
	const char *s;
	LValue lv;
	Value rhs;

	s = *p;
	if (!parse_lvalue(vm, &s, &lv))
		return 0;
	skip_ws(&s);
	if (*s != '=')
		return 0;
	s++;
	rhs = eval_value(vm, &s);
	if (!set_lvalue(vm, &lv, &rhs)) {
		value_release(&rhs);
		return 0;
	}
	value_release(&rhs);
	*p = s;
	return 1;
}

static int exec_sleep(vm, p)
	BasicVm *vm;
	const char **p;
{
	const char *s;
	long ticks;

	(void)vm;
	s = *p;
	skip_ws(&s);
	if (*s == '(') {
		s++;
		ticks = (long)eval_expr(vm, &s);
		skip_ws(&s);
		if (*s == ')')
			s++;
	} else {
		ticks = (long)eval_expr(vm, &s);
	}
	if (ticks > 0)
		host_sleep_ticks(ticks);
	*p = s;
	return 1;
}

static int stdin_ready(void)
{
#if HAVE_SELECT
	fd_set rfds;
	struct timeval tv;

	FD_ZERO(&rfds);
	FD_SET(0, &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	return select(1, &rfds, (fd_set *)0, (fd_set *)0, &tv) > 0;
#else
	return 0;
#endif
}

static int exec_poke(vm, p)
	BasicVm *vm;
	const char **p;
{
	const char *s;
	int addr;
	int val;

	s = *p;
	skip_ws(&s);
	addr = (int)eval_expr(vm, &s);
	skip_ws(&s);
	if (*s != ',')
		return 0;
	s++;
	val = (int)eval_expr(vm, &s);
	if (addr < 0)
		addr = 0;
	if (addr > 65535)
		addr = 65535;
	if (!ensure_mem(vm))
		return runtime_error(vm, "OUT OF MEMORY");
	vm->mem[addr] = (unsigned char)(val & 255);
	*p = s;
	return 1;
}

static int exec_wait(vm, p)
	BasicVm *vm;
	const char **p;
{
	const char *s;
	int addr;
	int mask;
	int value;
	int have_value;
	int m;
	int v;

	s = *p;
	skip_ws(&s);
	addr = (int)eval_expr(vm, &s);
	skip_ws(&s);
	if (*s != ',')
		return 0;
	s++;
	mask = (int)eval_expr(vm, &s);
	have_value = 0;
	skip_ws(&s);
	if (*s == ',') {
		s++;
		value = (int)eval_expr(vm, &s);
		have_value = 1;
	}
	if (addr < 0)
		addr = 0;
	if (addr > 65535)
		addr = 65535;
	if (!ensure_mem(vm))
		return runtime_error(vm, "OUT OF MEMORY");
	m = mask & 255;
	v = have_value ? (value & 255) : m;
	while (((int)vm->mem[addr] & m) != v) {
		/* avoid a tight spin */
		host_sleep_ticks(1);
	}
	*p = s;
	return 1;
}

static int exec_clr(vm, p)
	BasicVm *vm;
	const char **p;
{
	const char *s;

	s = *p;
	vm_clear_runtime(vm);
	*p = s;
	return 1;
}

static int exec_run(vm, p, pc)
	BasicVm *vm;
	const char **p;
	int *pc;
{
	(void)p;
	vm_clear_runtime(vm);
	if (vm->nlines <= 0) {
		*pc = -1;
		return 1;
	}
	*pc = 0;
	*p = vm->lines[0].text;
	return 1;
}

static int exec_get(vm, p)
	BasicVm *vm;
	const char **p;
{
	const char *s;
	LValue lv;
	Value v;
	int c;
	char buf[2];

	s = *p;
	skip_ws(&s);
	if (!parse_lvalue(vm, &s, &lv))
		return 0;
	if (!lv.is_string)
		return runtime_error(vm, "TYPE MISMATCH");
	c = -1;
	if (stdin_ready())
		c = getchar();
	if (c == EOF || c < 0) {
		v = value_str_own(xstrdup(""));
	} else {
		buf[0] = (char)c;
		buf[1] = '\0';
		v = value_str_own(xstrdup(buf));
	}
	if (!set_lvalue(vm, &lv, &v)) {
		value_release(&v);
		return runtime_error(vm, "OUT OF MEMORY");
	}
	value_release(&v);
	*p = s;
	return 1;
}

static int exec_data(vm, p)
	BasicVm *vm;
	const char **p;
{
	const char *s;

	(void)vm;
	s = *p;
	/* DATA is handled at load-time; at run-time it is a no-op */
	while (*s && *s != ':')
		s++;
	*p = s;
	return 1;
}

static int read_input_line(buf, n)
	char *buf;
	int n;
{
	char *e;

	if (fgets(buf, n, stdin) == NULL)
		return 0;
	e = buf + strlen(buf);
	while (e > buf && (e[-1] == '\n' || e[-1] == '\r'))
		e--;
	*e = '\0';
	return 1;
}

static void input_next_field(p, out, outsz)
	const char **p;
	char *out;
	int outsz;
{
	const char *s;
	int n;

	s = *p;
	while (*s == ' ' || *s == '\t')
		s++;
	n = 0;
	if (*s == '"') {
		s++;
		while (*s && *s != '"') {
			if (n < outsz - 1)
				out[n++] = *s;
			s++;
		}
		if (*s == '"')
			s++;
	} else {
		while (*s && *s != ',') {
			if (n < outsz - 1)
				out[n++] = *s;
			s++;
		}
		/* trim trailing whitespace */
		while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t'))
			n--;
	}
	out[n] = '\0';
	while (*s == ' ' || *s == '\t')
		s++;
	if (*s == ',')
		s++;
	*p = s;
}

static int exec_input(vm, p)
	BasicVm *vm;
	const char **p;
{
	const char *s;
	char *prompt;
	LValue lvs[64];
	int nlv;
	int i;
	char inbuf[256];
	const char *ip;
	char field[256];

	s = *p;
	prompt = NULL;
	skip_ws(&s);
	if (*s == '"') {
		prompt = parse_string_lit(&s);
		skip_ws(&s);
		if (*s == ';' || *s == ',')
			s++;
	}
	nlv = 0;
	for (;;) {
		skip_ws(&s);
		if (*s == '\0' || *s == ':')
			break;
		if (nlv >= (int)(sizeof(lvs) / sizeof(lvs[0])))
			break;
		if (!parse_lvalue(vm, &s, &lvs[nlv]))
			return 0;
		nlv++;
		skip_ws(&s);
		if (*s == ',') {
			s++;
			continue;
		}
		break;
	}
	if (prompt != NULL) {
		fputs(prompt, stdout);
		free(prompt);
	}
	fputs("? ", stdout);
	fflush(stdout);
	if (!read_input_line(inbuf, (int)sizeof(inbuf)))
		return runtime_error(vm, "INPUT PAST END");
	ip = inbuf;
	for (i = 0; i < nlv; i++) {
		Value v;
		input_next_field(&ip, field, (int)sizeof(field));
		if (lvs[i].is_string)
			v = value_str_own(xstrdup(field));
		else
			v = value_num(atof(field));
		if (!set_lvalue(vm, &lvs[i], &v)) {
			value_release(&v);
			return runtime_error(vm, "ILLEGAL QUANTITY");
		}
		value_release(&v);
	}
	*p = s;
	return 1;
}

static int exec_gosub(vm, p, pc)
	BasicVm *vm;
	const char **p;
	int *pc;
{
	const char *s;
	int target;
	const char *ret;

	s = *p;
	target = (int)eval_expr(vm, &s);
	ret = s;
	if (!gosub_push(vm, *pc, ret))
		return runtime_error(vm, "OUT OF MEMORY");
	if (!jump_to(vm, target, pc, p))
		return runtime_error(vm, "UNDEF'D STATEMENT");
	return 1;
}

static int exec_return(vm, p, pc)
	BasicVm *vm;
	const char **p;
	int *pc;
{
	const char *s;
	int rpc;
	const char *rs;

	(void)p;
	if (!gosub_pop(vm, &rpc, &rs))
		return runtime_error(vm, "RETURN WITHOUT GOSUB");
	*pc = rpc;
	s = rs;
	*p = s;
	return 1;
}

static void delete_array(vm, idx, is_string)
	BasicVm *vm;
	int idx;
	int is_string;
{
	int i;

	for (i = 0; i < vm->narrays; i++) {
		if (vm->arrays[i].idx == idx && vm->arrays[i].is_string == is_string) {
			if (vm->arrays[i].strs != NULL) {
				int j;
				int cnt;
				cnt = (vm->arrays[i].d1 + 1) * (vm->arrays[i].d2 + 1);
				for (j = 0; j < cnt; j++) {
					if (vm->arrays[i].strs[j] != NULL)
						free(vm->arrays[i].strs[j]);
				}
				free(vm->arrays[i].strs);
			}
			if (vm->arrays[i].nums != NULL)
				free(vm->arrays[i].nums);
			/* remove entry */
			for (; i + 1 < vm->narrays; i++)
				vm->arrays[i] = vm->arrays[i + 1];
			vm->narrays--;
			return;
		}
	}
}

static int exec_dim(vm, p)
	BasicVm *vm;
	const char **p;
{
	const char *s;
	char name[32];
	int is_string;
	int idx;
	int d1;
	int d2;
	int nd;

	s = *p;
	for (;;) {
		skip_ws(&s);
		if (*s == '\0' || *s == ':')
			break;
		if (!parse_ident(&s, name, (int)sizeof(name), &is_string))
			return 0;
		idx = ident_to_var_idx(name);
		if (idx < 0 || idx >= 26 * 37)
			return 0;
		if (!parse_subscripts(vm, &s, &d1, &d2, &nd))
			return 0;
		if (nd == 1)
			d2 = 0;
		delete_array(vm, idx, is_string);
		if (!array_alloc(vm, idx, is_string, d1, d2))
			return runtime_error(vm, "OUT OF MEMORY");
		skip_ws(&s);
		if (*s == ',') {
			s++;
			continue;
		}
		break;
	}
	*p = s;
	return 1;
}

static int exec_for(vm, p, pc)
	BasicVm *vm;
	const char **p;
	int *pc;
{
	const char *s;
	LValue lv;
	double startv;
	double endv;
	double step;
	ForFrame f;
	Value v;

	(void)pc;
	s = *p;
	if (!parse_lvalue(vm, &s, &lv))
		return 0;
	if (lv.is_string || lv.is_array)
		return runtime_error(vm, "TYPE MISMATCH");
	skip_ws(&s);
	if (*s != '=')
		return 0;
	s++;
	startv = eval_expr(vm, &s);
	if (!match_kw(&s, "TO"))
		return 0;
	endv = eval_expr(vm, &s);
	step = 1.0;
	if (match_kw(&s, "STEP"))
		step = eval_expr(vm, &s);
	v = value_num(startv);
	(void)set_lvalue(vm, &lv, &v);
	value_release(&v);
	f.var_idx = lv.idx;
	f.endv = endv;
	f.step = step;
	f.pc = *pc;
	f.s = s;
	if (!for_push(vm, &f))
		return runtime_error(vm, "OUT OF MEMORY");
	*p = s;
	return 1;
}

static int exec_next(vm, p, pc)
	BasicVm *vm;
	const char **p;
	int *pc;
{
	const char *s;
	char name[32];
	int is_string;
	int idx;
	ForFrame *f;
	double cur;
	int has_name;

	s = *p;
	skip_ws(&s);
	has_name = 0;
	idx = -1;
	if (isalpha((unsigned char)*s)) {
		const char *t;
		t = s;
		if (parse_ident(&t, name, (int)sizeof(name), &is_string)) {
			if (is_string)
				return runtime_error(vm, "TYPE MISMATCH");
			idx = ident_to_var_idx(name);
			s = t;
			has_name = 1;
		}
	}
	if (vm->for_sp <= 0)
		return runtime_error(vm, "NEXT WITHOUT FOR");
	if (has_name && idx != vm->fors[vm->for_sp - 1].var_idx)
		return runtime_error(vm, "NEXT WITHOUT FOR");
	f = &vm->fors[vm->for_sp - 1];
	cur = vm->nvars[f->var_idx] + f->step;
	vm->nvars[f->var_idx] = cur;
	vm->nset[f->var_idx] = 1;
	if ((f->step >= 0.0 && cur <= f->endv) || (f->step < 0.0 && cur >= f->endv)) {
		*pc = f->pc;
		*p = f->s;
		return 1;
	}
	vm->for_sp--;
	*p = s;
	return 1;
}

static int exec_read(vm, p)
	BasicVm *vm;
	const char **p;
{
	const char *s;
	LValue lv;
	DataItem *di;
	Value v;

	s = *p;
	for (;;) {
		skip_ws(&s);
		if (*s == '\0' || *s == ':')
			break;
		if (!parse_lvalue(vm, &s, &lv))
			return 0;
		if (vm->data_pos >= vm->ndata)
			return runtime_error(vm, "OUT OF DATA");
		di = &vm->data[vm->data_pos++];
		if (di->is_string)
			v = value_str_ref(di->str ? di->str : (char *)"");
		else
			v = value_num(di->num);
		if (!set_lvalue(vm, &lv, &v))
			return runtime_error(vm, "ILLEGAL QUANTITY");
		/* value_str_ref is non-owned; ok */
		value_release(&v);
		skip_ws(&s);
		if (*s == ',') {
			s++;
			continue;
		}
		break;
	}
	*p = s;
	return 1;
}

static int exec_restore(vm, p)
	BasicVm *vm;
	const char **p;
{
	const char *s;
	int line;
	int i;

	s = *p;
	skip_ws(&s);
	if (*s == '\0' || *s == ':') {
		vm->data_pos = 0;
		*p = s;
		return 1;
	}
	line = (int)eval_expr(vm, &s);
	for (i = 0; i < vm->ndata; i++) {
		if (vm->data[i].line >= line) {
			vm->data_pos = i;
			*p = s;
			return 1;
		}
	}
	vm->data_pos = vm->ndata;
	*p = s;
	return 1;
}

static int exec_on(vm, p, pc)
	BasicVm *vm;
	const char **p;
	int *pc;
{
	const char *s;
	int sel;
	int count;
	int is_gosub;
	int target;
	const char *ret;

	s = *p;
	sel = (int)eval_expr(vm, &s);
	is_gosub = 0;
	if (match_kw(&s, "GOTO"))
		is_gosub = 0;
	else if (match_kw(&s, "GOSUB"))
		is_gosub = 1;
	else
		return 0;
	count = 0;
	for (;;) {
		skip_ws(&s);
		if (*s == '\0' || *s == ':')
			break;
		target = (int)eval_expr(vm, &s);
		count++;
		if (count == sel) {
			if (is_gosub) {
				ret = s;
				if (!gosub_push(vm, *pc, ret))
					return runtime_error(vm, "OUT OF MEMORY");
			}
			if (!jump_to(vm, target, pc, p))
				return runtime_error(vm, "UNDEF'D STATEMENT");
			return 1;
		}
		skip_ws(&s);
		if (*s == ',') {
			s++;
			continue;
		}
		break;
	}
	/* out of range: do nothing */
	*p = s;
	return 1;
}

static int exec_statement(vm, p, pc)
	BasicVm *vm;
	const char **p;
	int *pc;
{
	const char *s;
	int target;

	s = *p;
	skip_ws(&s);
	if (*s == '\0') {
		*p = s;
		return 1;
	}
	if (match_kw(&s, "REM")) {
		/* comment to end of line */
		while (*s && *s != ':')
			s++;
		*p = s;
		return 1;
	}
	if (match_kw(&s, "DATA")) {
		*p = s;
		return exec_data(vm, p);
	}
	if (match_kw(&s, "PRINT")) {
		*p = s;
		return exec_print(vm, p);
	}
	if (match_kw(&s, "INPUT")) {
		*p = s;
		return exec_input(vm, p);
	}
	if (match_kw(&s, "LET")) {
		*p = s;
		return exec_let(vm, p);
	}
	if (match_kw(&s, "DIM")) {
		*p = s;
		return exec_dim(vm, p);
	}
	if (match_kw(&s, "READ")) {
		*p = s;
		return exec_read(vm, p);
	}
	if (match_kw(&s, "RESTORE")) {
		*p = s;
		return exec_restore(vm, p);
	}
	if (match_kw(&s, "SLEEP")) {
		*p = s;
		return exec_sleep(vm, p);
	}
	if (match_kw(&s, "POKE")) {
		*p = s;
		return exec_poke(vm, p);
	}
	if (match_kw(&s, "WAIT")) {
		*p = s;
		return exec_wait(vm, p);
	}
	if (match_kw(&s, "CLR")) {
		*p = s;
		return exec_clr(vm, p);
	}
	if (match_kw(&s, "RUN")) {
		*p = s;
		return exec_run(vm, p, pc);
	}
	if (match_kw(&s, "GET")) {
		*p = s;
		return exec_get(vm, p);
	}
	if (match_kw(&s, "GOSUB")) {
		*p = s;
		return exec_gosub(vm, p, pc);
	}
	if (match_kw(&s, "RETURN")) {
		*p = s;
		return exec_return(vm, p, pc);
	}
	if (match_kw(&s, "FOR")) {
		*p = s;
		return exec_for(vm, p, pc);
	}
	if (match_kw(&s, "NEXT")) {
		*p = s;
		return exec_next(vm, p, pc);
	}
	if (match_kw(&s, "ON")) {
		*p = s;
		return exec_on(vm, p, pc);
	}
	if (match_kw(&s, "END") || match_kw(&s, "STOP")) {
		*pc = -1;
		*p = s;
		return 1;
	}
	if (match_kw(&s, "GOTO")) {
		target = (int)eval_expr(vm, &s);
		if (!jump_to(vm, target, pc, p))
			return runtime_error(vm, "UNDEF'D STATEMENT");
		return 1;
	}
	if (match_kw(&s, "IF")) {
		int cond;
		cond = eval_relop(vm, &s);
		if (!match_kw(&s, "THEN"))
			return 0;
		skip_ws(&s);
		if (cond) {
			/* THEN <line> or THEN <statement> */
			if (isdigit((unsigned char)*s)) {
				target = (int)eval_expr(vm, &s);
				if (!jump_to(vm, target, pc, p))
					return runtime_error(vm, "UNDEF'D STATEMENT");
				return 1;
			}
			*p = s;
			return 1;
		}
		/* false: ignore the rest of the line (CBM-style) */
		while (*s)
			s++;
		*p = s;
		return 1;
	}

	/* implied LET */
	*p = s;
	return exec_let(vm, p);
}

