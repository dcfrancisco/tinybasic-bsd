static int run_program(vm)
	BasicVm *vm;
{
	int pc;
	const char *s;

	if (vm->nlines <= 0)
		return 0;
	vm->had_error = 0;
	pc = 0;
	s = vm->lines[pc].text;
	while (pc >= 0 && pc < vm->nlines) {
		if (!exec_statement(vm, &s, &pc)) {
			if (!vm->had_error)
				fprintf(stderr, "?SYNTAX ERROR\n");
			return 1;
		}
		if (pc < 0)
			return 0;
		if (pc >= vm->nlines)
			break;
		skip_ws(&s);
		if (*s == ':') {
			s++;
			continue;
		}
		if (*s == '\0') {
			pc++;
			if (pc >= 0 && pc < vm->nlines)
				s = vm->lines[pc].text;
			continue;
		}
		/* otherwise, keep executing from current position */
	}
	return 0;
}

BasicVm *basic_create(void)
{
	BasicVm *vm;
	int i;

	vm = (BasicVm *)malloc(sizeof(BasicVm));
	if (vm == NULL)
		return NULL;
	memset(vm, 0, sizeof(BasicVm));
	for (i = 0; i < 26 * 37; i++) {
		vm->nvars[i] = 0.0;
		vm->nset[i] = 0;
		vm->svars[i] = NULL;
	}
	vm->rnd_seed = 1UL;
	vm->had_error = 0;
	vm->mem = NULL;
	vm->mem_size = 0;
	return vm;
}

void basic_destroy(vm)
	BasicVm *vm;
{
	int i;
	int n;

	if (vm == NULL)
		return;
	n = vm->nlines;
	for (i = 0; i < n; i++) {
		if (vm->lines[i].text)
			free(vm->lines[i].text);
	}
	free(vm->lines);
	for (i = 0; i < 26 * 37; i++) {
		if (vm->svars[i] != NULL)
			free(vm->svars[i]);
	}
	free(vm->gosub);
	free(vm->fors);
	for (i = 0; i < vm->ndata; i++) {
		if (vm->data[i].str != NULL)
			free(vm->data[i].str);
	}
	free(vm->data);
	for (i = 0; i < vm->narrays; i++) {
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
	}
	free(vm->arrays);
	if (vm->mem != NULL)
		free(vm->mem);
	free(vm);
}

static void vm_clear_runtime(vm)
	BasicVm *vm;
{
	int i;

	/* scalar vars */
	for (i = 0; i < 26 * 37; i++) {
		vm->nvars[i] = 0.0;
		vm->nset[i] = 0;
		if (vm->svars[i] != NULL) {
			free(vm->svars[i]);
			vm->svars[i] = NULL;
		}
	}

	/* arrays */
	for (i = 0; i < vm->narrays; i++) {
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
	}
	free(vm->arrays);
	vm->arrays = NULL;
	vm->narrays = 0;
	vm->array_cap = 0;

	/* stacks */
	vm->gosub_sp = 0;
	vm->for_sp = 0;

	/* DATA pointer */
	vm->data_pos = 0;

	/* RNG */
	vm->rnd_seed = 1UL;
}

int basic_run_file(vm, path)
	BasicVm *vm;
	const char *path;
{
	FILE *f;
	char buf[1024];
	char *p;
	int lineno;
	const char *s;

	if (vm == NULL || path == NULL)
		return 1;

	f = fopen(path, "r");
	if (f == NULL) {
		perror(path);
		return 1;
	}
	while (fgets(buf, sizeof(buf), f) != NULL) {
		p = buf;
		while (*p == ' ' || *p == '\t')
			p++;
		if (*p == '\0' || *p == '\n' || *p == '\r')
			continue;
		if (!isdigit((unsigned char)*p))
			continue;
		lineno = 0;
		while (isdigit((unsigned char)*p)) {
			lineno = lineno * 10 + (*p - '0');
			p++;
		}
		while (*p == ' ' || *p == '\t')
			p++;
		/* trim newline */
		{
			char *e;
			e = p + strlen(p);
			while (e > p && (e[-1] == '\n' || e[-1] == '\r'))
				e--;
			*e = '\0';
		}
		if (!add_line(vm, lineno, p)) {
			fclose(f);
			return 1;
		}
	}
	fclose(f);

	if (vm->nlines == 0)
		return 0;
	qsort(vm->lines, (size_t)vm->nlines, sizeof(ProgLine), line_cmp);

	/* quick pass: ignore lines that start with nothing */
	for (lineno = 0; lineno < vm->nlines; lineno++) {
		s = vm->lines[lineno].text;
		skip_ws(&s);
		if (*s == '\0')
			vm->lines[lineno].text[0] = '\0';
	}

	if (!collect_data(vm)) {
		fprintf(stderr, "?OUT OF MEMORY\n");
		return 1;
	}
	vm->data_pos = 0;

	return run_program(vm);
}
