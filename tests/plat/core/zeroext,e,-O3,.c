#include "test.h"

/* Zero extension of a 16-bit constant to 32 bits, also when ego has
 * propagated the constant into the conversion. */

uint16_t d = 40000;
uint32_t r;

static int32_t f(void)
{
    int16_t l = -1;

    return (int32_t)d == 0 ? (int32_t)(uint16_t)l : (int32_t)(uint16_t)l % (int32_t)d;
}

static uint32_t g(void)
{
    uint16_t u = (uint16_t)-1;

    return (uint32_t)u;
}

/* Bypasses the CRT, so there's no stdio. */
void _m_a_i_n(void)
{
    r = (uint32_t)(uint16_t)-1;
    ASSERT(r == 0xFFFFUL);
    ASSERT(f() == 25535L);
    ASSERT(g() == 0xFFFFUL);
    ASSERT((uint32_t)(uint16_t)-32768 == 0x8000UL);

    finished();
}
