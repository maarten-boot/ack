/* F3: text + data + bss just under 64K. cv gives the rest of the 64K as
 * stack, which here is only a few bytes, so it must give at least
 * MIN_STACK instead. PAD is chosen by cv.sh. */
#ifndef PAD
#define PAD 2
#endif
char pad[PAD];

int main(void)
{
	pad[0] = 1;
	return pad[PAD - 1];
}
