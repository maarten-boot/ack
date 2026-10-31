/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Id$ */

# include "FP_types.h"

void
b64_sft(e1,n)
B64	*e1;
int	n;
{
	if (n > 0) {
		if (n > 63) {
			e1->l_32 = 0;
			e1->h_32 = 0;
			return;
		}
		if (n >= 32) {
			e1->l_32 = e1->h_32;
			e1->h_32 = 0;
			n -= 32;
		}
		if (n > 0) {
			e1->l_32 >>= n;
			if (e1->h_32 != 0) {
				e1->l_32 |= (e1->h_32 << (32 - n));
				e1->h_32 >>= n;
			}
		}
		return;
	}
	n = -n;
	if (n > 0) {
		if (n > 63) {
			e1->l_32 = 0;
			e1->h_32 = 0;
			return;
		}
		if (n >= 32) {
			e1->h_32 = e1->l_32;
			e1->l_32 = 0;
			n -= 32;
		}
		if (n > 0) {
			e1->h_32 <<= n;
			if (e1->l_32 != 0) {
				e1->h_32 |= (e1->l_32 >> (32 - n));
				e1->l_32 <<= n;
			}
		}
	}
}

/*
 * Shift right n > 0 bits and OR the bits shifted out into the lowest bit
 * (the "sticky" bit): rounding later must know whether anything nonzero
 * was lost. There are always guard bits below a single or double result.
 */
void
b64_jsft(e1,n)
B64	*e1;
int	n;
{
	unsigned long lost;

	if (n > 63)
		lost = e1->h_32 | e1->l_32;
	else if (n >= 32)
		lost = e1->l_32 | (e1->h_32 & ((1UL << (n - 32)) - 1));
	else
		lost = e1->l_32 & ((1UL << n) - 1);
	b64_sft(e1, n);
	if (lost)
		e1->l_32 |= 1;
}

void
b64_lsft(e1)
B64	*e1;
{
	/*	shift left 1 bit */
	e1->h_32 <<= 1;
	if (e1->l_32 & 0x80000000L) e1->h_32 |= 1;
	e1->l_32 <<= 1;
}

void
b64_rsft(e1)
B64	*e1;
{
	/*	shift right 1 bit */
	e1->l_32 >>= 1;
	if (e1->h_32 & 1) e1->l_32 |= 0x80000000L;
	e1->h_32 >>= 1;
}
