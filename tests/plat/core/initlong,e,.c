#include "test.h"

/* Initialised longs take sizeof(long) bytes in the data, so what follows
 * them is where the compiler expects it. The m68020 code generator once
 * emitted every 4-byte constant as .data8 when int is 2 bytes (minix68k):
 * each long took 8 bytes and shifted the data behind it.
 */

long arr[4] = { 0x11223344L, -2L, 0x55667788L, 1L };

struct mixed {
	char c;
	long l;
	short s;
	long m;
	char d;
};

struct mixed st[2] = {
	{ 'a', 0x01020304L, 0x0506, -0x0708090AL, 'b' },
	{ 'c', -1L, -2, 0x7FFFFFFFL, 'd' },
};

static long sl = 0x12345678L;
static int after = 0x1234;

/* Bypasses the CRT, so there's no stdio. */
void _m_a_i_n(void)
{
	ASSERT(arr[0] == 0x11223344L);
	ASSERT(arr[1] == -2L);
	ASSERT(arr[2] == 0x55667788L);
	ASSERT(arr[3] == 1L);

	ASSERT(st[0].c == 'a');
	ASSERT(st[0].l == 0x01020304L);
	ASSERT(st[0].s == 0x0506);
	ASSERT(st[0].m == -0x0708090AL);
	ASSERT(st[0].d == 'b');
	ASSERT(st[1].c == 'c');
	ASSERT(st[1].l == -1L);
	ASSERT(st[1].s == -2);
	ASSERT(st[1].m == 0x7FFFFFFFL);
	ASSERT(st[1].d == 'd');

	ASSERT(sl == 0x12345678L);
	ASSERT(after == 0x1234);

	finished();
}
