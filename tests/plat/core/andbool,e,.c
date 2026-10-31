#include "test.h"

/* x & (a op b) is x & 0 or x & 1, not "x with bit 0 cleared if false". */

int16_t one = 1;
int16_t two = 2;
int8_t g = 0x40;
int32_t sh = 0;

static int16_t f(void)
{
    return (int16_t)((int32_t)-8017 >> ((uint8_t)sh & 31))
        & (int16_t)((int32_t)-1L && (int16_t)g);
}

static int16_t h(int16_t x)
{
    return (int16_t)((int32_t)x >> ((uint8_t)sh & 31)) & (int16_t)(one < two);
}

static int16_t k(int16_t x)
{
    return (int16_t)((int32_t)x >> ((uint8_t)sh & 31)) & (int16_t)(one == two);
}

/* Bypasses the CRT, so there's no stdio. */
void _m_a_i_n(void)
{
    ASSERT(f() == 1);
    ASSERT(h(-8017) == 1);
    ASSERT(h(-8018) == 0);
    ASSERT(k(-8017) == 0);

    finished();
}
