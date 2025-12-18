#include <stdio.h>
#include <stdlib.h>

#include "basic.h"

int main(argc, argv)
	int argc;
	char **argv;
{
	BasicVm *vm;
	int rc;

	if (argc < 2) {
		fprintf(stderr, "usage: %s program.bas\n", argv[0]);
		return 2;
	}

	vm = basic_create();
	if (vm == NULL) {
		fprintf(stderr, "bsdbasic: out of memory\n");
		return 1;
	}

	rc = basic_run_file(vm, argv[1]);
	basic_destroy(vm);
	return rc;
}
