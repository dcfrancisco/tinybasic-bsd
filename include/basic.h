#ifndef BASIC_H
#define BASIC_H

/* K&R C friendly API */

typedef struct BasicVm BasicVm;

BasicVm *basic_create(void);
void basic_destroy(BasicVm *vm);

int basic_run_file(BasicVm *vm, const char *path);

/* 60 ticks = 1 second */
int host_sleep_ticks(long ticks);

#endif
