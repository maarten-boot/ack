#include "test.h"

/* An unsigned long comparison used twice to guard a division: ego keeps
 * the result of .cmu in a local. */

int16_t a = 5;

static int16_t f(uint16_t p)
{
    return (int16_t)(((uint32_t)p ^ 2128779349UL) >= 3881263963UL) == 0
        || (int16_t)(((uint32_t)p ^ 2128779349UL) >= 3881263963UL) == -1
        ? a : a / (int16_t)(((uint32_t)p ^ 2128779349UL) >= 3881263963UL);
}

static int16_t g(int32_t p)
{
    return (int16_t)(p * 3L >= 300000L) == 0 || (int16_t)(p * 3L >= 300000L) == 7
        ? a : a / (int16_t)(p * 3L >= 300000L);
}

/* Bypasses the CRT, so there's no stdio. */
void _m_a_i_n(void)
{
    ASSERT(f(100) == 5);
    ASSERT(f(3) == 5);
    ASSERT(g(1L) == 5);
    ASSERT(g(200000L) == 5);

    finished();
}
