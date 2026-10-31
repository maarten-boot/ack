/*  Copyright (c) 2022. See the file License in
 *  the root directory for more information.
 */
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include "system.h"
#include "astring.h"

#if defined WIN32 || defined __minix
/* No rand_r() there. Use unsigned long: the tempering needs 32 bits, and
 * int is only 16 bits on Minix-ST. */
typedef unsigned long seed_t;

static unsigned long temper(unsigned long x)
{
	x ^= x>>11;
	x ^= x<<7 & 0x9D2C5680UL;
	x ^= x<<15 & 0xEFC60000UL;
	x ^= x>>18;
	return x;
}

static unsigned long next_random(seed_t *seed)
{
	*seed = *seed * 1103515245UL + 12345;
	return temper(*seed)/2;
}
#else
typedef unsigned int seed_t;

static unsigned long next_random(seed_t *seed)
{
	return rand_r(seed);
}
#endif

char* sys_maketempfile(const char* prefix, const char* suffix)
{
#if defined __minix
	/* Minix 2.0.4 (the rosco build of the passes): no clock_gettime() */
	seed_t seed = (seed_t)(long)prefix
		^ (seed_t)time(NULL) ^ (seed_t)getpid();
#else
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	seed_t seed = (seed_t)(intptr_t)prefix
		^ (seed_t)ts.tv_sec ^ (seed_t)ts.tv_nsec;
#endif
	const char* tempdir = sys_gettmpdir();

	for (;;)
	{
		unsigned long hash = next_random(&seed);
		char* filename = aprintf("%s/ack.%s-%lx%s%s",
			tempdir, prefix, hash,
			((*suffix == 0) || (*suffix == '.')) ? "" : ".",
			suffix);
		int fd = open(filename, O_CREAT|O_EXCL|O_RDWR, 0600);
		if (fd != -1)
		{
			close(fd);
			return filename;
		}
		if (errno != EEXIST)
			perror("could not create temporary file");
		free(filename);
	}
}
