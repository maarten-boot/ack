#include "test.h"

/* A 16-bit multiplication tested for zero: the low word is zero, the
 * 32-bit product of mulu.w/muls.w is not. */

uint16_t g = 32768;
int16_t s = 16384;

static int t(uint16_t x)
{
    return (uint16_t)(g * x) ? 1 : 0;
}

static int ts(int16_t x)
{
    return (int16_t)(s * x) ? 1 : 0;
}

/* Bypasses the CRT, so there's no stdio. */
void _m_a_i_n(void)
{
    ASSERT(t(0xe5b2) == 0);
    ASSERT(t(1) == 1);
    ASSERT(ts(4) == 0);
    ASSERT(ts(1) == 1);
    if ((uint16_t)(g * (uint16_t)2) != 0)
        fail(__LINE__);

    finished();
}
