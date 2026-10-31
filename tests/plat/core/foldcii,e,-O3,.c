#include "test.h"

/* Narrowing conversions of constants that ego propagates: (int8_t)16384
 * is 0. */

int16_t a = 1, b = 2;
int16_t sh = 0;
int16_t fe = 29988;
int32_t fb = -1021131558L;
int8_t arr[8];

static void f(void)
{
    uint16_t l = 16384;

    arr[((uint8_t)((int32_t)((uint8_t)((uint16_t)((uint8_t)l) << (sh & 7)))
        | (int32_t)(((int8_t)l) ? ((uint8_t)fe) : ((uint8_t)fb)))) & 7] = 1;
}

static int16_t g(void)
{
    uint16_t l = 16384;

    return (int8_t)l ? a : b;
}

static int32_t h(void)
{
    uint16_t l = 45219;

    return (int32_t)(int16_t)l;
}

/* Bypasses the CRT, so there's no stdio. */
void _m_a_i_n(void)
{
    f();
    ASSERT(arr[2] == 1);
    ASSERT(arr[4] == 0);
    ASSERT(g() == 2);
    ASSERT(h() == 45219L - 65536L);

    finished();
}
