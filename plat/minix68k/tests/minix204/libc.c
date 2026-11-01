/*
 * The Minix 2.0.4 C library as the rosco board links it, run in the
 * emulator (minix204.sh). The output is compared with libc.out.
 */
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static jmp_buf env;
static int depth;

static void handler(int sig)
{
	(void)sig;
}

static void deep(int n)
{
	depth = n;
	if (n == 5)
		longjmp(env, n * 10);
	deep(n + 1);
}

static int cmp(const void* a, const void* b)
{
	int x = *(const int*)a, y = *(const int*)b;

	return (x > y) - (x < y); /* x - y overflows with 16 bit int */
}

int main(int argc, char** argv)
{
	char buf[80];
	char* p;
	FILE* f;
	int i, r;
	int v[6] = { 5, -3, 9, 0, 32767, -32768 };
	sigset_t set, old;
	double d;

	/* numbers */
	printf("int %d %u %x %o %ld %lu\n", -12345, 54321u, 0xbeef, 0777,
	    -2000000000L, 4000000000UL);
	printf("str [%5s] [%-5s] [%.2s] %c\n", "ab", "ab", "abcdef", 'z');

	/* floating point: %e %f %g, and back */
	printf("flt %f %e %g %g\n", 2.5, 1e10, 0.125, 1.0 / 3.0);
	printf("flt %.3f %.10g %g\n", -1.0 / 8.0, 1.0 / 3.0, 123456789.0);
	d = strtod("1.5e3", &p);
	printf("strtod %g rest [%s] atof %g\n", d, p, atof("-0.25"));
	sprintf(buf, "%.17g", 0.1);
	printf("0.1 = %s back %d\n", buf, strtod(buf, NULL) == 0.1);
	r = sscanf("42 3.75 word", "%d %lf %s", &i, &d, buf);
	printf("sscanf %d: %d %g %s\n", r, i, d, buf);

	/* heap */
	p = malloc(1000);
	strcpy(p, "heap");
	p = realloc(p, 20000);
	printf("realloc keeps [%s]\n", p);
	free(p);
	p = calloc(100, 2);
	for (i = 0, r = 0; i < 200; i++)
		r |= p[i];
	printf("calloc zeroed %d\n", r == 0);
	free(p);

	/* files */
	f = fopen("minix204.tmp", "w");
	fprintf(f, "first %d\nsecond line\n", 1);
	fclose(f);
	f = fopen("minix204.tmp", "r");
	fgets(buf, sizeof(buf), f);
	buf[strcspn(buf, "\n")] = 0;
	printf("read [%s] ", buf);
	fseek(f, 0L, SEEK_END);
	printf("size %ld\n", ftell(f));
	fclose(f);
	printf("unlink %d\n", unlink("minix204.tmp"));
	errno = 0;
	r = open("minix204.tmp", O_RDONLY);
	printf("open %d errno %d %s\n", r, errno, strerror(errno));

	/* signals (2.0.4: signal() is sigaction(); nothing is delivered) */
	printf("signal first %d\n", signal(SIGINT, handler) == SIG_DFL);
	printf("signal again %d\n", signal(SIGINT, SIG_IGN) == handler);
	printf("signal ign %d\n", signal(SIGINT, SIG_DFL) == SIG_IGN);
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGKILL);
	sigprocmask(SIG_BLOCK, &set, NULL);
	sigprocmask(SIG_SETMASK, NULL, &old);
	printf("mask INT %d KILL %d\n", sigismember(&old, SIGINT),
	    sigismember(&old, SIGKILL));

	/* setjmp/longjmp (2.0.4's setjmp saves the signal mask) */
	if ((r = setjmp(env)) == 0)
		deep(1);
	printf("longjmp %d depth %d\n", r, depth);

	/* misc */
	qsort(v, 6, sizeof(v[0]), cmp);
	printf("qsort %d %d %d %d %d %d\n", v[0], v[1], v[2], v[3], v[4], v[5]);
	printf("strtol %ld %ld\n", strtol("-077", NULL, 0), strtol("7fffffff", NULL, 16));
	printf("argc %d argv0 %s\n", argc, strrchr(argv[0], '/') ? "path" : argv[0]);
	printf("@@FINISHED\n");
	return 0;
}
