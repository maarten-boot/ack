/* Pointers in initialised data, to text, rom, data and bss. */
#include <stdio.h>

static const char* const names[] = { "zero", "one", "two" };
static int (*fn)(const char*, ...) = printf;
static int counter;
static int* pc = &counter;
static const char* const* pn = names;

int main(void)
{
	return fn("%s %d\n", pn[2], *pc) < 0;
}
