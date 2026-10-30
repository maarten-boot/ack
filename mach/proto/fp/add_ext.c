/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Id$ */

/*
	ADD TWO EXTENDED FORMAT NUMBERS
*/

#include "FP_bias.h"
#include "FP_types.h"

void
add_ext(e1,e2)
EXTEND	*e1,*e2;
{
	if (e1->exp == EXT_INFNAN || e2->exp == EXT_INFNAN) {
		if (EXT_ISNAN(e1))
			return;
		if (EXT_ISNAN(e2) || e1->exp != EXT_INFNAN)
			*e1 = *e2;	/* NaN, or infinity + finite */
		else if (e2->exp == EXT_INFNAN && e1->sign != e2->sign) {
			e1->m1 = EXT_QNAN;	/* infinity - infinity */
			e1->m2 = 0;
		}
		return;
	}
	if ((e2->m1 | e2->m2) == 0L) {
		if ((e1->m1 | e1->m2) == 0L && e1->sign != e2->sign)
			e1->sign = 0;	/* -0 + +0 == +0 */
		return;
	}
	if ((e1->m1 | e1->m2) == 0L) {
		*e1 = *e2;
		return;
	}
	sft_ext(e1, e2);	/* adjust mantissas to equal powers */
	if (e1->sign != e2->sign) {
		/* e1 + e2 = e1 - (-e2) */
		if (e2->m1 > e1->m1 ||
                    (e2->m1 == e1->m1 && e2->m2 > e1->m2)) {
                	/*      abs(e2) > abs(e1) */
			EXTEND x;

			x = *e1;
			*e1 = *e2;
                	if (x.m2 > e1->m2) {
                        	e1->m1 -= 1;    /* carry in */
                	}
                	e1->m1 -= x.m1;
                	e1->m2 -= x.m2;
        	}
        	else {
                	if (e2->m2 > e1->m2)
                        	e1->m1 -= 1;    /* carry in */
                	e1->m1 -= e2->m1;
                	e1->m2 -= e2->m2;
        	}
		if ((e1->m1 | e1->m2) == 0L)
			e1->sign = 0;	/* x - x == +0 */
	}
	else {
		if (b64_add(&e1->mantissa,&e2->mantissa)) {	/* addition carry */
			b64_rsft(&e1->mantissa);	/* shift mantissa one bit RIGHT */
			e1->m1 |= 0x80000000L;	/* set max bit	*/
			e1->exp++;		/* increase the exponent */
		}
	}
	nrm_ext(e1);
}
