#include <stddef.h>

#include "basic.h"

#if defined(__APPLE__) || defined(__linux__) || defined(__unix__)
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#if defined(__APPLE__) || defined(__linux__)
#include <time.h>
#endif

int host_sleep_ticks(ticks)
	long ticks;
{
	long t;
	long usec;

	if (ticks <= 0)
		return 0;

	/* 60 ticks = 1 second */
	t = ticks;

#ifdef PDP11
	/* 2.11BSD clock granularity is typically ~1/30s, so round up to 2 ticks */
	if (t & 1)
		t++;
#endif

	usec = (t * 1000000L) / 60L;
	if (usec <= 0)
		return 0;

#if defined(__APPLE__) || defined(__linux__)
	{
		struct timespec ts;
		ts.tv_sec = usec / 1000000L;
		ts.tv_nsec = (usec % 1000000L) * 1000L;
		(void)nanosleep(&ts, (struct timespec *)0);
		return 0;
	}
#else
	{
		/* Portable BSD-ish sub-second sleep */
		struct timeval tv;
		tv.tv_sec = usec / 1000000L;
		tv.tv_usec = usec % 1000000L;
		(void)select(0, (void *)0, (void *)0, (void *)0, &tv);
		return 0;
	}
#endif
}
