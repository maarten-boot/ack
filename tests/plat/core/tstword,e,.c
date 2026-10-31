#include "test.h"

/* The result of ! is a byte that is not yet extended; a word test of it
 * must not see the stale upper byte. */

int32_t a[2] = {0x10000L, 0x12300L};
uint16_t g = 77;

static uint16_t f(int i)
{
    return (uint16_t)(!(int32_t)(uint16_t)a[i]) == 0
        ? g : g % (uint16_t)(!(int32_t)(uint16_t)a[i]);
}

/* Bypasses the CRT, so there's no stdio. */
void _m_a_i_n(void)
{
    ASSERT(f(0) == 0);
    ASSERT(f(1) == 77);
    ASSERT((uint16_t)(!(int32_t)(uint16_t)a[1]) == 0);
    ASSERT((uint16_t)(!(int32_t)(uint16_t)a[0]) == 1);

    finished();
}
